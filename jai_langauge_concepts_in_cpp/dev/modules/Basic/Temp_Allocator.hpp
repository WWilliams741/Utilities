#include <sys/mman.h>
#include <cstring>

#define DEFAULT_TEMP_ALLOCATOR_VIRTUAL_MEMORY_RESERVE 256 *  1024 * 1024 // 256 Megabytes

CONST_VAR s64 TEMP_ALLOCATOR_PAGE_SIZE = 4096; // NOTE(WALKER): If we control the hardware and operating system, we can look into 2MB Huge Pages for this instead of the standard 4K.

// struct Temp_Allocator {
//     s64 alignment             = 8;
//     s64 high_water_mark       = {};

//     u8* current_point         = {};

//     u8* current_memory_base   = {};
//     u8* current_memory_limit  = {};

//     u8* original_memory_base  = {};
//     u8* original_memory_limit = {};

//     // Header vs. Footer?
//     struct Next_Pool_Footer {
//         u8* next_memory_base  = {};
//         u8* next_memory_limit = {};
//     };
// };

// Public API:
void init(Temp_Allocator* temp, s64 reserve = DEFAULT_TEMP_ALLOCATOR_VIRTUAL_MEMORY_RESERVE) {
    auto& t = *temp;

         reserve = align_pow2(reserve, TEMP_ALLOCATOR_PAGE_SIZE);
    auto base    = mmap(nullptr, (u64)reserve, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

    t.original_memory_base  = (u8*) base;
    t.original_memory_limit = t.original_memory_base + reserve - sizeof(Temp_Allocator::Next_Pool_Footer);
    t.current_memory_base   = t.original_memory_base;
    t.current_memory_limit  = t.original_memory_limit;
    t.current_point         = t.current_memory_base;
}

void deinit(Temp_Allocator* temp) {
    auto& t = *temp;

    while (t.original_memory_base) {
        auto& footer            = *(Temp_Allocator::Next_Pool_Footer*)(t.original_memory_limit);

        auto next_memory_base   = footer.next_memory_base;
        auto next_memory_limit  = footer.next_memory_limit;

        munmap(t.original_memory_base, (u64)(t.original_memory_limit - t.original_memory_base));

        t.original_memory_base  = next_memory_base;
        t.original_memory_limit = next_memory_limit;
    }
}

void grow_temp(Temp_Allocator* temp, s64 nbytes) {
    auto& t = *temp;

    auto& footer             = *(Temp_Allocator::Next_Pool_Footer*)(t.original_memory_limit);

    auto reserve             = align_pow2((s64)(t.current_memory_limit - t.current_memory_base + sizeof(Temp_Allocator::Next_Pool_Footer)) * 2, TEMP_ALLOCATOR_PAGE_SIZE);
         reserve             = max(reserve, align_pow2(nbytes, TEMP_ALLOCATOR_PAGE_SIZE));

    auto base                = mmap(nullptr, (u64)reserve, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);

    t.current_memory_base    = (u8*) base;
    t.current_memory_limit   = t.current_memory_base + reserve - sizeof(Temp_Allocator::Next_Pool_Footer);
    t.current_point          = t.current_memory_base;

    footer.next_memory_base  = t.current_memory_base;
    footer.next_memory_limit = t.current_memory_limit;
}

void* get_unaligned(Temp_Allocator* temp, s64 nbytes) {
    auto& t = *temp;

    if (!t.original_memory_base) {
        init(temp, max(nbytes + (s64)sizeof(Temp_Allocator::Next_Pool_Footer), (s64)(DEFAULT_TEMP_ALLOCATOR_VIRTUAL_MEMORY_RESERVE)));
    }

    auto result = t.current_point;
    auto end    = result + nbytes;

    if (end > t.current_memory_limit) {
        grow_temp(temp, nbytes);

        result = t.current_point;
        end    = result + nbytes;
    }

    t.current_point    = end;
    t.high_water_mark += nbytes;

    return result;
}

void* get(Temp_Allocator* temp, s64 nbytes) {
    auto& t = *temp;

    if (!t.original_memory_base) {
        init(temp, max(nbytes + (s64)sizeof(Temp_Allocator::Next_Pool_Footer), (s64)(DEFAULT_TEMP_ALLOCATOR_VIRTUAL_MEMORY_RESERVE)));
    }

    align_pow2(t.current_point, t.alignment);

    auto result = t.current_point;
    auto end    = result + nbytes;

    if (end > t.current_memory_limit) {
        grow_temp(temp, nbytes);

        align_pow2(t.current_point, t.alignment);

        result = t.current_point;
        end    = result + nbytes;
    }

    t.current_point    = end;
    t.high_water_mark += nbytes;

    return result;
}

// Both of the below use context.temp directly:
void reset_temp_allocator() {
    auto temp = &context.temp;
    auto& t   = *temp;

    if (!t.original_memory_base) return;

    // Recombine pools into one big pool:
    if (t.current_memory_base != t.original_memory_base) {
        deinit(temp);
        init(temp, t.high_water_mark);
    }

    t.high_water_mark = 0;
    t.current_point   = t.current_memory_base;
}

auto temp_allocator_proc(Allocator_Mode mode, s64 requested_size, s64 old_size, void* old_memory, void*) -> void* {
    auto temp = &context.temp;
    auto& t = *temp;

    switch(mode) {
        case REALLOCATE: {
            if (requested_size <= old_size) return old_memory;

            auto prev_data = t.current_point - old_size;
            auto remainder = requested_size - old_size;
            auto pool_left = (s64)(t.current_memory_limit - t.current_point);

            if ((prev_data == old_memory) && (remainder <= pool_left)) {
                get_unaligned(temp, remainder);
                return old_memory;
            }
        } __attribute__ ((fallthrough)); // [[fallthrough]]:
        case ALLOCATE:   {
            auto result = get(temp, requested_size);
            if (mode == REALLOCATE && old_memory) {
                memcpy(result, old_memory, old_size);
            }
            return result;
        }
        case DEALLOCATE: { break; }
    }

    return nullptr;
}
