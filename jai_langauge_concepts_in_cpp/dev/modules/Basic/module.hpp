#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <new>
#include <atomic>

// Aliases:
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using f32 = float;
using f64 = double;

// Constants:
#define CONST_VAR static constexpr

CONST_VAR s64 CACHE_LINE_SIZE = 64;

// Defer:
template<typename Code>
struct Defer {
    Code code;
    Defer(Code block) : code(block) {}
    ~Defer() { code(); } // call code at end of scope
};
struct Defer_Generator { template<typename Code> Defer<Code> operator+(Code code) { return Defer<Code>{code}; } };
#define GEN_DEFER_NAME_HACK(name, counter) name##counter
#define GEN_DEFER_NAME(name, counter) GEN_DEFER_NAME_HACK(name, counter)
#define defer auto GEN_DEFER_NAME(_defer_, __COUNTER__) = Defer_Generator{} + [&]()

// Maybe by reference:
template<typename T>
using Arg = typename std::conditional<sizeof(T) <= 8 && !std::is_class<T>::value,
            const T __restrict__, const T& __restrict__>::type;

// Helper functions:
template<typename T>
constexpr
auto align_forward(T n, s64 align) -> T {
    return (T)((((s64)n + align - 1) / align) * align);
}

template<typename T>
constexpr
auto align_pow2(T n, s64 align) -> T {
    return (T)(((s64)n + align - 1) & ~(align - 1));
}

template<typename T>
auto next_pow2(T n) -> T {
    s64 p = 1;
    s64 x = (s64) n;
    while (x > p) p += p;
    return (T) p;
}

template<typename T>
constexpr
auto max(const T& __restrict__ a, const T& __restrict__ b) -> T {
    return a >= b ? a : b;
}

template<typename T>
constexpr
auto min(const T& __restrict__ a, const T& __restrict__ b) -> T {
    return a <= b ? a : b;
}

// Context stuff:
enum Allocator_Mode {
    ALLOCATE,
    REALLOCATE,
    DEALLOCATE
};

using Allocator_Proc = auto(*)(Allocator_Mode mode, s64 requested_size, s64 old_size, void* old_memory, void* allocator_data) -> void*;

struct Allocator {
    Allocator_Proc proc = {};
    void*          data = {};

    constexpr Allocator()                                             {}
    constexpr Allocator(Allocator_Proc p, void* d) : proc(p), data(d) {}
};

// Forward Declarations:
auto default_allocator_proc(Allocator_Mode mode, s64 requested_size, s64 old_size, void* old_memory, void* allocator_data) -> void*;
auto    temp_allocator_proc(Allocator_Mode mode, s64 requested_size, s64 old_size, void* old_memory, void* allocator_data) -> void*;

struct Temp_Allocator {
    s64 alignment             = 8;
    s64 high_water_mark       = {};

    u8* current_point         = {};

    u8* current_memory_base   = {};
    u8* current_memory_limit  = {};

    u8* original_memory_base  = {};
    u8* original_memory_limit = {};

    struct Next_Pool_Footer {
        u8* next_memory_base  = {};
        u8* next_memory_limit = {};
    };
};

// Context:
struct Context {
    CONST_VAR auto default_allocator = Allocator{&default_allocator_proc, nullptr};
    CONST_VAR auto temp_allocator    = Allocator{&temp_allocator_proc,    nullptr};

    Allocator      allocator    = default_allocator;
    Temp_Allocator temp         = {};
    s64            thread_index = {};
};

// Globals:
thread_local
Context          context           = {};
std::atomic<s64> next_thread_index = {1};

auto alloc(s64 size) -> void* {
    auto& a = context.allocator;

    auto result = a.proc(ALLOCATE, size, 0, nullptr, a.data);

    return result;
}

auto realloc(void* memory, s64 size, s64 old_size) -> void* {
    auto& a = context.allocator;

    auto result = a.proc(REALLOCATE, size, old_size, memory, a.data);

    return result;
}

void dealloc(void* memory) {
    auto& a = context.allocator;

    a.proc(DEALLOCATE, 0, 0, memory, a.data);
}

#define push_allocator(new_allocator, ...) {      \
    auto old_allocator = context.allocator;       \
    context.allocator  = new_allocator;           \
    defer { context.allocator = old_allocator; }; \
    { __VA_ARGS__ }                               \
}

template<typename T>
void remember_allocators(T* data) {
    if (context.allocator.proc) {
        data->allocator = context.allocator;
    } else {
        data->allocator = context.default_allocator;
    }
}

// Arrays:
template<typename T>
struct Resizable_Array {
    s64       count     = {};
    T*        data      = {};
    s64       allocated = {};
    Allocator allocator = {};

    // Array access support:
    T& operator[](s64 index) { return data[index]; }

    // For loop support:
    T* begin() { return data; }
    T* end()   { return data + count; }
};

template<typename T>
struct Array_View {
    s64 count = {};
    T*  data  = {};

    constexpr Array_View() {}

    // Array access support:
    T& operator[](s64 index) { return data[index]; }

    // For loop support:
    T* begin() { return data; }
    T* end()   { return data + count; }

    // Resizable_Array support:
        constexpr
        Array_View(Resizable_Array<T>& arr) : count(arr.count), data(arr.data) {}
    void operator=(Resizable_Array<T>& arr) { count = arr.count; data = arr.data; }
};

// String:
struct String {
    s64 count = {};
    u8* data  = (u8*)"";
};

// NOTE(WALKER): If we go to C++17 or higher I can get access to if constexpr (compile time if statement)
//               and make this more optimal than a runtime initialized check
template<typename T>
auto New(bool initialized = true) -> T* {
    T* result = (T*) alloc(sizeof(T));

    if (initialized) {
        new (result) T;
    }

    return result;
}

template<typename T>
auto NewArray(s64 count, bool initialized = true) -> Array_View<T> {
    Array_View<T> result = {};
    if (count <= 0) return result;

    result.count = count;
    result.data  = (T*) alloc(sizeof(T) * count);

    if (initialized) {
        for (auto& t : result) {
            new (&t) T;
        }
    }

    return result;
}

#include "Default_Allocator.hpp"
#include "Temp_Allocator.hpp"
#include "Array.hpp"
#include "String.hpp"
