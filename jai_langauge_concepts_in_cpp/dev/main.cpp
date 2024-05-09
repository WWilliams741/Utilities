// This stuff can also work with standard containers thanks to operator new/delete overloads:
#include <vector>
#include <map>
#include <cstdio>
#include <thread>

#include "Basic/module.hpp"
#include "Hash_Table.hpp"
#include "Threads/module.hpp"

// NOTE(WALKER): These overloads below make it to where any "standard"/STL stuff we use
// automatically uses context.allocator convention. Except, they don't remember their allocators
// so make sure you don't mix/match allocators and use "push_allocator()" macro, and record the
// Allocator{} used for setup somewhere.
void* operator new(std::size_t size) {
    return alloc((s64) size);
}
void* operator new[](std::size_t size) {
    return alloc((s64) size);
}
void operator delete(void* memory) {
    dealloc(memory);
}
void operator delete(void* memory, std::size_t) {
    dealloc(memory);
}
void operator delete[](void* memory) {
    dealloc(memory);
}
void operator delete[](void* memory, std::size_t) {
    dealloc(memory);
}

struct List_Node {
    void*      data = {}; // You can imagine memory management getting quite out of hand with this
    List_Node* next = {};
};

/*
    In programming architecture there are four ways to think about memory "lifetimes":
    1. Short lived, end of scope.            (stack variables, alloca(), "defer", mutex lock/unlock)
    2. Short lived, end of cycle.            (shown below, in while(true) loop, or threads in a "work loop" who pop off a channel)
    3. Long  lived, well defined.            (database, subsystems, "ECS" in games)
    4. Long  lived, not well defined.        (what most languages assume everything is -> garbage collector, RAII, rust borrow checker, etc.)

    #4 is the assumed default by almost every language under the sun, and it all stems from "Individual Element Thinking," and OOP.
    This means people think of each individual "object" as having its own lifetime, and is a blackbox to everything else.
    As one can imagine, garbage collection, RAII, Smart Poitners, Rust's borrow checker appear as a good "solution" to this "problem."
    But really, it is the inability to categorize memory lifetime categories and assume everything is #4, hence why they appear
    necessary. Really all these languages and "solutions" are really bandaids over the root problem, which is micro allocating/pointer chasing,
    and these languages really cement the problem in the industry as a whole, because now the root problem is "solved" by the language
    you now have to fight to solve real problems. Basically, all of these languages are "solving" a "problem" that they are
    seeking in the first place, and are not themselves aware of, which means a very large baby was thrown out with
    the bathwater a long long time ago, when really some basic architecture setup was all that was required.

    Anyway, enough rambling, below is the example code that proves all of the above true, enjoy.
*/

// We're going to create a linked-list where each node is an "alloc",
// and therefore you have to follow the chain and "free" each node.
// You can imagine this would be painful if you tried to use malloc()/free()
// or RAII with destructors (following the pointer chain to free each
// individual node). In most languages it appears as though the only
// way to solve this is with a garbage collector, because that's the way
// you were taught, it's the heap and the stack and that's it.
auto make_list(s64 count) -> List_Node* {
    if (count <= 0) return nullptr;

    auto begin   = New<List_Node>();
    auto current = begin;

    for (s64 i = 1; i < count; ++i) {
        auto next     = New<List_Node>();
        current->next = next;
        current       = next;
    }

    return begin;
}

// You can imagine multiple threads doing this, but in this architecture
// each thread has their own temp_allocator thanks to "thread_local",
// which means every thread has their own garbage collection mechanism
void do_some_really_dumb_leaky_stuff_that_is_hard_to_memory_manage() {
    make_list(1000);
    make_list(1000);
    make_list(1000);
    make_list(1000);
    make_list(1000);
    make_list(1000);

    // Imagine for a second we replaced these with our own implementations (not that hard to do):
    std::vector<s64> ints;
    for (s64 i = 1; i <= 1000; ++i) {
        ints.emplace_back(i);
    }

    std::map<s64, s64> id_table;
    for (s64 i = 1; i <= 1000; ++i) {
        id_table.insert({i, i});
    }

    // Restriction for third-party library stuff:
    // std containers do NOT remember their allocators
    // therefore there is still the potential to leak some stuff
    // if you don't set the context.allocator correctly (wrapper APIs)

    // This is our stuff (arrays and hash table):
    Resizable_Array<s64> our_ints;
    for (s64 i = 1; i <= 1000; ++i) {
        array_add(&our_ints, i);
    }

    Hash_Table<s64, s64> our_id_table;
    table_init(&our_id_table);
    for (s64 i = 1; i <= 1000; ++i) {
        table_add(&our_id_table, i, i);
    }
}

// A Thread_Group is a pool of threads
// that pop "Work" off of their own queues
// and do the work. This is a "job's system" as
// you might know it.
Thread_Group tg = {};

// NOTE(WALKER): This Thread_Group_Proc is wrapped in a while(true) loop
//               where each thread calls reset_temp_allocator() after executing
//               its proc. In other words, each thread has garbage collection
//               and can write leaky code like below.
auto thread_group_do_leaky_things(Thread_Group*, Thread*, void*) -> Thread_Continue_Status {
    printf("thread #%ld before = %p\n", context.thread_index, context.temp.current_point);
    do_some_really_dumb_leaky_stuff_that_is_hard_to_memory_manage();
    printf("thread #%ld after  = %p\n", context.thread_index, context.temp.current_point);

    return Thread_Continue_Status::CONTINUE;
}

// Imagine this is for anything more permanent than the while(true) loop (like the Thread_Group below):
void init_program() {
    thread_group_init(&tg, 2, thread_group_do_leaky_things); // Each thread has their own temp_allocator
    thread_group_start(&tg);
}

int main() {
    // Init your program here (you could use temp_allocator here if you wanted to):
    init_program();

    // Main program loop here:
    context.allocator = context.temp_allocator;
    while (true) {
        defer { reset_temp_allocator(); }; // temp acts as a garbage collector for your while(true) loop (fire and forget)

        // Rest of your main program loop (doing dumb leaky stuff):
        printf("main before = %p\n", context.temp.current_point);
        do_some_really_dumb_leaky_stuff_that_is_hard_to_memory_manage();
        printf("main after  = %p\n", context.temp.current_point);

        // Have Thread_Group do some work:
        for (s64 i = 0; i < tg.worker_info.count; ++i) {
            thread_group_add_work(&tg, nullptr);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{1000});

        thread_group_get_completed_work(&tg);
    }
}
