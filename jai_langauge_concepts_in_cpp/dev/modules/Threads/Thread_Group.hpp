struct Thread_Group;

struct Work_Entry {
    Work_Entry*  next            = {};
    void*        work            = {};
    Thread_Index thread_index    = {};
    String       logging_name    = {};

    f64          issue_time      = -1.0;
    s64          work_list_index = -1;
};

struct Work_List {
    Semaphore   semaphore = {};
    Mutex       mutex     = {};

    Work_Entry* first     = {};
    Work_Entry* last      = {};
    s64         count     = {};
};

struct Worker_Info {
    // NOTE(WALKER): Must match the anonymous struct below for align_forward to work
    struct Unpadded_Worker_Info {
        Thread        thread       = {};
        Work_List     available    = {};
        Work_List     completed    = {};

        Thread_Group* group        = {};
        s64           worker_index = -1;
    };

    union {
        // struct {
        //     Thread        thread       = {};
        //     Work_List     available    = {};
        //     Work_List     completed    = {};

        //     Thread_Group* group        = {};
        //     s64           worker_index = -1;
        // };
        Unpadded_Worker_Info info               = {};
        u8                   padding[align_forward(sizeof(Unpadded_Worker_Info), CACHE_LINE_SIZE)];
    };
    Array_View<s64>          work_steal_indices = {};
};

void init_work_list(Work_List* list) {
    init(&list->semaphore);
    init(&list->mutex);
}

void deinit_work_list(Work_List* list) {
    destroy(&list->semaphore);
    destroy(&list->mutex);
}

void add_work(Work_List* list, Work_Entry* entry) {
    auto& l = *list;

    {
        lock(&l.mutex);
        defer { unlock(&l.mutex); };

        if (l.last) {
            l.last->next = entry;
        } else {
            l.first      = entry;
        }

        l.last   = entry;
        l.count += 1;
    }

    signal(&l.semaphore);
}

auto get_work(Work_List* list) -> Work_Entry* {
    auto& l = *list;

    lock(&l.mutex);
    defer { unlock(&l.mutex); };

    if (!l.first) return nullptr;

    auto result = l.first;

    l.first = result->next;

    if (!l.first) l.last = nullptr;

    l.count -= 1;

    return result;
}

// TODO(WALKER): Finish this

enum class Thread_Continue_Status {
    STOP,
    CONTINUE
};

struct Thread_Group;
using Thread_Group_Proc = auto(*)(Thread_Group* group, Thread* thread, void* work) -> Thread_Continue_Status;

struct Thread_Group {
    // User:
    void*                   data                     = {};
    Thread_Group_Proc       proc                     = {};
    String                  name                     = {};
    bool                    logging                  = true;
    using Time_Proc = auto(*)() -> f64;
    Time_Proc               time_proc                = {};

    // Internal:
    Allocator               allocator                = {};
    Array_View<Worker_Info> worker_info              = {};
    void*                   worker_info_data_to_free = {};

    s64                     next_worker_index        = {};
    bool                    initted                  = {};
    bool                    started                  = {};
    bool                    should_exit              = {};
};

auto thread_group_run(Thread* thread) -> s64 {
    auto& t = *thread;

    auto& info  = t.worker_info->info;
    auto& group = *info.group;

    context.allocator = context.temp_allocator;

    Work_Entry* entry = {};
    while (!group.should_exit) {
        defer { reset_temp_allocator(); };

        if (!entry) {
            wait_for(&info.available.semaphore);
            if (group.should_exit) break;

            entry = get_work(&info.available);
        }

        if (entry) {
            auto& e = *entry;

            e.thread_index = thread->index;
            e.next         = {};

            // logging here

            auto should_continue = Thread_Continue_Status::CONTINUE;
            if (group.proc) {
                should_continue = group.proc(&group, thread, e.work);
            }

            add_work(&info.completed, entry);

            if (should_continue == Thread_Continue_Status::STOP) break;
        }

        // Do the work stealing thing:
        if (t.worker_info->work_steal_indices.count) {
            if (group.should_exit) break;

            printf("work steal count = %ld\n", t.worker_info->work_steal_indices.count);

            entry = get_work(&info.available);
            if (entry) {
                wait_for(&info.available.semaphore); // We still have work to do, don't bother stealing from someone else
            } else {
                for (auto i : t.worker_info->work_steal_indices) {
                    entry = get_work(&group.worker_info[i].info.available);
                    if (entry) {
                        // logging here
                        break;
                    }
                }
            }
        } else {
            entry = {};
        }
    }

    return 0;
}

void thread_group_init(Thread_Group* group, s64 num_threads, Thread_Group_Proc group_proc, bool enable_work_stealing = false) {
    auto& g = *group;

    remember_allocators(group);

push_allocator(g.allocator,
    auto unaligned_worker_info = NewArray<Worker_Info>(num_threads + 1, false);
    g.worker_info_data_to_free = (void*) unaligned_worker_info.data;
    memset(g.worker_info_data_to_free, 0, sizeof(Worker_Info) * (num_threads + 1));
    g.worker_info.data         = align_forward(unaligned_worker_info.data, CACHE_LINE_SIZE);
    g.worker_info.count        = num_threads;

    g.proc = group_proc;

    s64 current_worker_index = {};
    for (auto& wi : g.worker_info) {
        auto& info = wi.info;
        defer { ++current_worker_index; };

        thread_init(&info.thread, thread_group_run);

        info.thread.worker_info = &wi;

        init_work_list(&info.available);
        init_work_list(&info.completed);

        info.group = group;
        info.worker_index = current_worker_index;

        // TODO(WALKER): Figure out a good work stealing algorithm.
        //               For now, just loop to the right of yourself
        //               until you get work
        if (enable_work_stealing && (num_threads > 1)) {
            auto indices = NewArray<s64>(num_threads - 1, false);
            s64 cursor = (current_worker_index + 1) % num_threads;
            for (s64 i = 0; i < num_threads - 1; ++i) {
                indices[i] = cursor;
                cursor = (cursor + 1) % num_threads;
            }

            wi.work_steal_indices = indices;
        }
    }

    g.initted = true;
)
}

void thread_group_start(Thread_Group* group) {
    for (auto& wi : group->worker_info) thread_start(&wi.info.thread);
    group->started = true;
}

#include <chrono> // In future probably get rid of this, but for now it can stay

auto thread_group_shutdown(Thread_Group* group, s32 timeout_milliseconds = -1) -> bool {
    auto& g = *group;

    // Assert(g.initted);

    bool all_done = true;
    if (g.started) {
        for (auto& wi : g.worker_info) signal(&wi.info.available.semaphore);

        std::chrono::time_point<std::chrono::steady_clock> start;
        if (timeout_milliseconds > 0) {
            start = std::chrono::steady_clock::now();
        }

        auto remaining_timeout_ms = timeout_milliseconds;
        for (auto& wi : g.worker_info) {
            auto& info = wi.info;

            if (remaining_timeout_ms > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
                remaining_timeout_ms = (timeout_milliseconds - (s32) elapsed);
                if (remaining_timeout_ms < 0) remaining_timeout_ms = 0;
            }

            bool is_done = thread_is_done(&info.thread, remaining_timeout_ms);
            if (!is_done) all_done = false;
        }
    }

    if (!all_done) return false;

    for (auto& wi : g.worker_info) {
        auto& info = wi.info;

        thread_deinit(&info.thread);
        deinit_work_list(&info.available);
        deinit_work_list(&info.available);
        push_allocator(g.allocator, dealloc(wi.work_steal_indices.data);)
    }

    push_allocator(g.allocator, dealloc(g.worker_info_data_to_free);)
    return true;
}

void thread_group_add_work(Thread_Group* group, void* work) {
    auto& g = *group;

    // Assert(g.worker_info.count >= 0);

push_allocator(g.allocator,
    auto entry = New<Work_Entry>();
    auto& e    = *entry;

    e.work = work;
    // e.logging_name = logging_name;
    // e.issue_time = get_time(group);

    auto thread_index = g.next_worker_index++;
    if (g.next_worker_index >= g.worker_info.count) g.next_worker_index = 0;

    e.work_list_index = thread_index;

    auto list = &g.worker_info[thread_index].info.available;
    add_work(list, entry);

    // do logging here
)
}

auto thread_group_get_completed_work(Thread_Group* group) -> Array_View<void*> /* uses temp_allocator */ {
    auto& g = *group;

    Resizable_Array<void*> results;
    results.allocator = context.temp_allocator;

    for (auto& wi : g.worker_info) {
        auto& info = wi.info;

        auto& list = info.completed;

        s64         count     = {};
        Work_Entry* completed = {};

        {
            lock(&list.mutex);
            defer { unlock(&list.mutex); };

            count     = list.count;
            completed = list.first;

            if (list.first) {
                list.first = {};
                list.last  = {};
                list.count = {};
            }
        }

        if (!completed) continue;

        array_reserve(&results, results.count + count); // @speed: make this better?

        while(completed) {
            array_add(&results, completed->work);
            auto next = completed->next;

            // do logging here

            push_allocator(g.allocator, dealloc(completed);)
            completed = next;
        }
    }

    // do logging here

    return results;
}
