#pragma once

#include "Basic/module.hpp"

CONST_VAR u32 HASH_INIT = 5381;

auto sdbm_hash(void* data, s64 size, u32 h = HASH_INIT) -> u32 {
    for (s64 i = 0; i < size; ++i) {
        h = (h << 16) + (h << 6) - h + ((u8*)data)[i];
    }

    return h;
}

auto sdbm_float_hash(f32* f, s64 count, u32 h = HASH_INIT) -> u32 {
    for (s64 i = 0; i < count; ++i) {
        union { f32 f; u32 u; } x;
        x.f = f[i];
        if (x.u == 0x80000000) x.u = 0;
        h = sdbm_hash(&x, 4, h);
    }

    return h;
}

CONST_VAR u64 FNV_64_PRIME       = 0x100000001b3;
CONST_VAR u64 FNV_64_OFFSET_BIAS = 0xcbf28ce484222325;

auto fnv1a_hash(u64 val, u64 h = FNV_64_OFFSET_BIAS) -> u64{
    h = h ^ val;
    return h * FNV_64_PRIME;
}
auto fnv1a_hash(void* data, s64 size, u64 h = FNV_64_OFFSET_BIAS) -> u64 {
    for (s64 i = 0; i < size; ++i) {
        h = fnv1a_hash(((u8*)data)[i], h);
    }

    return h;
}

auto knuth_hash(u64 x) -> u64 {
    CONST_VAR u64 KNUTH_GOLDEN_RATIO = 1140071481932319485ULL;
    return KNUTH_GOLDEN_RATIO * x;
}

template<typename T,
    typename std::enable_if<
        std::is_pointer<T>::value  ||
        std::is_integral<T>::value ||
        std::is_enum<T>::value,
        bool>::type = true>
auto get_hash(T x, u32 h = HASH_INIT) -> u32 {
    return (u32)(knuth_hash((u64)x ^ h) >> 32);
}

template<typename T,
    typename std::enable_if<
        std::is_floating_point<T>::value,
        bool>::type = true>
auto get_hash(T x, u32 h = HASH_INIT) -> u32 {
    return sdbm_hash((void*)&x, sizeof(T), h);
}

auto get_hash(String s, u32 h = HASH_INIT) -> u32 {
    return (u32) fnv1a_hash(s.data, s.count, h);
}

template<typename T>
auto get_hash(Array_View<T> arr, u32 h = HASH_INIT) -> u32 {
    return sdbm_hash((void*)arr.data, arr.count * sizeof(T), h);
}
