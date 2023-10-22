#ifndef DEFER_CPP
#define DEFER_CPP

// A useful macro to get arbitrary code to execute at the end of a scope using RAII and lambdas.
// This provides defer syntax similiar to golang. This is useful for interfacing
// with C APIs that need a "deinit" or "cleanup" function to be called at the end of scope.
// This creates code that is cleaner because you don't need to create lots of RAII wrappers.

// NOTE: The defer macro is assumed to be "noexcept" by default.
//       #define DEFER_WITH_EXCEPTIONS above "defer.hpp" to allow exceptions to be thrown
//       inside defer statements (although I would highly advise against it)
#ifdef DEFER_WITH_EXCEPTIONS
#define DEFER_NOEXCEPT noexcept(false)
#else
#define DEFER_NOEXCEPT noexcept(true)
#endif

#if __cplusplus >= 201703L

// Defer macro (>= C++17):
template<typename Code>
struct Defer {
    Code code;
// constexpr support (>= C++20):
#if __cplusplus >= 202002L
    constexpr Defer(Code block) DEFER_NOEXCEPT : code(block) {}
    constexpr ~Defer() DEFER_NOEXCEPT { code(); }
#else
    Defer(Code block) DEFER_NOEXCEPT : code(block) {}
    ~Defer() DEFER_NOEXCEPT { code(); }
#endif
};
#define GEN_DEFER_NAME_HACK(name, counter) name ## counter
#define GEN_DEFER_NAME(name, counter) GEN_DEFER_NAME_HACK(name, counter)
#define defer Defer GEN_DEFER_NAME(_defer_, __COUNTER__) = [&]() DEFER_NOEXCEPT

#else

// Defer macro (>= C++11)
template<typename Code>
struct Defer {
    Code code;
    Defer(Code block) DEFER_NOEXCEPT : code(block) {}
    ~Defer() DEFER_NOEXCEPT { code(); }
};
struct Defer_Generator { template<typename Code> Defer<Code> operator +(Code code) DEFER_NOEXCEPT { return Defer<Code>{code}; } };
#define GEN_DEFER_NAME_HACK(name, counter) name ## counter
#define GEN_DEFER_NAME(name, counter) GEN_DEFER_NAME_HACK(name, counter)
#define defer auto GEN_DEFER_NAME(_defer_, __COUNTER__) = Defer_Generator{} + [&]() DEFER_NOEXCEPT

#endif

// Example usage:
// auto some_func(auto& input) {
//     defer { ++input; /* Put code block here to execute at end of scope, you can refer to "input" in this code block like normal */ };
//     // put other code here you want to execute before defer like normal
//     return input;
// }
// Example main (should print 1 before 2):
// int main() {
//     defer { printf("hello, defer world! 2\n"); };
//     defer { printf("hello, defer world! 1\n"); };
// }

#endif
