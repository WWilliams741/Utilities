#ifndef DEFER_CPP
#define DEFER_CPP

// A useful macro to get arbitrary code to execute at the end of a scope using RAII and lambdas.
// This provides defer functionality similiar to golang. This is useful for interfacing 
// with C APIs that need a "deinit" or "cleanup" function to be called at the end of scope. 
// This creates code that is cleaner, because you don't need to create an RAII wrapper struct.

// NOTE: These defer macros are assumed to be "noexcept."
//       You are, of course, encouraged to change them to fit your needs if you want to use exceptions.

#if __cplusplus >= 201703L

// Defer macro (>= C++17):
template<typename Code>
struct Defer {
    Code code;
// constexpr support for C++20 and higher:
#if __cplusplus >= 202002L
    constexpr Defer(Code block) noexcept : code(block) {}
    constexpr ~Defer() noexcept { code(); }
#else
    Defer(Code block) noexcept : code(block) {}
    ~Defer() noexcept { code(); }
#endif
};
#define GEN_DEFER_NAME_HACK(name, counter) name ## counter
#define GEN_DEFER_NAME(name, counter) GEN_DEFER_NAME_HACK(name, counter)
#define defer Defer GEN_DEFER_NAME(__defer__, __COUNTER__) = [&]() noexcept

#else

// Defer macro (>= C++11)
template<typename Code>
struct Defer {
    Code code;
    Defer(Code block) noexcept : code(block) {}
    ~Defer() noexcept { code(); }
};
struct Defer_Generator { template<typename Code> Defer<Code> operator +(Code code) noexcept { return Defer<Code>{code}; } };
#define GEN_DEFER_NAME_HACK(name, counter) name ## counter
#define GEN_DEFER_NAME(name, counter) GEN_DEFER_NAME_HACK(name, counter)
#define defer auto GEN_DEFER_NAME(__defer__, __COUNTER__) = Defer_Generator{} + [&]() noexcept

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
