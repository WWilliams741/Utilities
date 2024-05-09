struct Mutex {
    pthread_mutex_t mutex = {};

    /* DEBUG_MODE stuff:
    bool initialized           = {};

    String name                = {};
    Mutex_Order order          = {};

    s32 redundant_lock_counter = {};

    String outer_name          = {};
    Mutex_Order outer_order    = {};
    Mutex* outer_mutex         = {};
    */
};

void init(Mutex* m) {
    pthread_mutex_init(&m->mutex, nullptr);
}

void destroy(Mutex* m) {
    pthread_mutex_destroy(&m->mutex);
}

void lock(Mutex* m) {
    pthread_mutex_lock(&m->mutex);
}

void unlock(Mutex* m) {
    pthread_mutex_unlock(&m->mutex);
}

struct Semaphore {
    sem_t semaphore = {};
};

void init(Semaphore* s, u32 initial_value = 0) {
    sem_init(&s->semaphore, 0, initial_value);
}

void destroy(Semaphore* s) {
    sem_destroy(&s->semaphore);
}

void signal(Semaphore* s) {
    sem_post(&s->semaphore);
}

enum class Wait_For_Result {
    SUCCESS,
    TIMEOUT,
    ERROR
};

auto wait_for(Semaphore* s, s32 milliseconds = -1) -> Wait_For_Result {
    s32 result;

    if (milliseconds < 0) {
        result = sem_wait(&s->semaphore);
        while ((result == -1) && (errno == EINTR)) {
            result = sem_wait(&s->semaphore);
        }
    } else {
        timespec current_time;
        if (clock_gettime(CLOCK_REALTIME, &current_time) == -1) return Wait_For_Result::ERROR;

        s64 new_nsec = (s64) current_time.tv_nsec + (s64) milliseconds * (s64) 1000000;

        s64 new_sec  = current_time.tv_sec + new_nsec / (s64) 1000000000;
            new_nsec = new_nsec % (s64) 1000000000;

        timespec end_time;
        end_time.tv_sec  = new_sec;
        end_time.tv_nsec = new_nsec;

        result = sem_timedwait(&s->semaphore, &end_time);
    }

    if (result == -1) {
        switch (errno) {
            case ETIMEDOUT: return Wait_For_Result::TIMEOUT;
                   default: return Wait_For_Result::ERROR;
        }
    }

    return Wait_For_Result::SUCCESS;
}

struct Thread;
struct Worker_Info;
using Thread_Index = s64;
using Thread_Proc = auto(*)(Thread* thread) -> s64;

struct Thread {
    Thread_Index index               = -1;
    Thread_Proc  proc                = {};
    void*        data                = {};

    Context      starting_context    = {};

    Worker_Info* worker_info         = {}; // Thread_Group

    pthread_t    thread_handle       = {};
    Semaphore    is_alive_semaphore  = {};
    Semaphore    suspended_semaphore = {};
    bool         is_done             = {};
};

auto thread_entry_proc(void* parameter) -> void* {
    auto& t = *(Thread*)(parameter);

    wait_for(&t.suspended_semaphore);

    context              = t.starting_context;
    context.thread_index = t.index;

    auto result = t.proc(&t);
    // deinit_context(); // TODO(WALKER): do this thing here
    t.is_done = true;
    signal(&t.is_alive_semaphore);
    return (void*) result;
}

auto thread_init(Thread* thread, Thread_Proc proc) -> bool {
    auto& t = *thread;

    init(&t.is_alive_semaphore);
    init(&t.suspended_semaphore);

    auto ok = pthread_create(&t.thread_handle, nullptr, thread_entry_proc, thread);

    if (ok != 0) {
        destroy(&t.is_alive_semaphore);
        destroy(&t.suspended_semaphore);
        return false;
    }

    t.proc                       = proc;
    t.starting_context.allocator = t.starting_context.temp_allocator;
    t.index                      = next_thread_index++;

    return true;
}

void thread_deinit(Thread* thread) {
    auto& t = *thread;

    if (t.thread_handle) {
        pthread_join(t.thread_handle, nullptr);
    }

    t.thread_handle = {};
    destroy(&t.is_alive_semaphore);
    destroy(&t.suspended_semaphore);

    // Assert(t.index > 0);

    t.index = -1;
}

void thread_start(Thread* thread) {
    signal(&thread->suspended_semaphore);
}

auto thread_is_done(Thread* thread, s32 milliseconds = 0) -> bool {
    auto& t = *thread;

    if (t.is_done) return true;

    auto result = wait_for(&t.is_alive_semaphore, milliseconds);
    if (result != Wait_For_Result::SUCCESS) return false;

    signal(&t.is_alive_semaphore);

    // Assert(t.is_done);
    return true;
}
