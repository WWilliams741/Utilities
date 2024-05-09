#pragma once

#include "Basic/module.hpp"
#include "Hashes.hpp"

CONST_VAR u32 NEVER_OCCUPIED_HASH = 0;
CONST_VAR u32 REMOVED_HASH        = 1;
CONST_VAR u32 FIRST_VALID_HASH    = 2;

template<typename Key_Type, typename Value_Type,
         u32  Load_Factor_Percent = 70,
         bool Refill_Removed      = true>
struct Hash_Table {
    CONST_VAR u32  LOAD_FACTOR_PERCENT = Load_Factor_Percent;
    CONST_VAR bool REFILL_REMOVED      = Refill_Removed;
    CONST_VAR s64  SIZE_MIN            = 32;

    s64 count            = {};

    s64 allocated        = {};
    s64 slots_filled     = {};

    Allocator allocator  = {};

    struct Entry {
        u32        hash  = {};
        Key_Type   key   = {};
        Value_Type value = {};
    };

    Array_View<Entry> entries = {};

    // For loop support:
};

#define Walk_Table(...)                                  \
auto mask = (u32)(t.allocated - 1);                      \
                                                         \
auto hash = get_hash(key);                               \
if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;   \
                                                         \
auto index = hash & mask;                                \
                                                         \
u32 probe_increment = 1;                                 \
                                                         \
auto table_while_loop = t.entries[index].hash;           \
while (table_while_loop) {                               \
    { __VA_ARGS__ }                                      \
    index            = (index + probe_increment) & mask; \
    probe_increment += 1;                                \
    table_while_loop = t.entries[index].hash;            \
}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
void table_resize(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, s64 slots_to_allocate = 0) {
    auto& t = *table;

    if (slots_to_allocate <= 0) slots_to_allocate = t.SIZE_MIN;
    auto n = next_pow2(slots_to_allocate);
    t.allocated = n;

    push_allocator(t.allocator,
        t.entries = NewArray<typename Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>::Entry>(n, false);
        for (auto& entry : t.entries) {
            entry.hash = NEVER_OCCUPIED_HASH;
        }
    )
}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
auto table_add(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, Arg<Key_Type> key, Arg<Value_Type> value) -> Value_Type*;

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
void table_expand(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table) {
    auto& t = *table;

    auto old_entries = t.entries;

    s64 new_allocated;

    if (((t.count * 2 + 1) * 100) < (t.allocated * t.LOAD_FACTOR_PERCENT)) {
        new_allocated = t.allocated;
    } else {
        new_allocated = t.allocated * 2;
    }

    if (new_allocated < t.SIZE_MIN) new_allocated = t.SIZE_MIN;

    table_resize(table, new_allocated);

    t.count        = 0;
    t.slots_filled = 0;

    for (auto& entry : old_entries) {
        if (entry.hash >= FIRST_VALID_HASH) table_add(table, entry.key, entry.value);
    }

    push_allocator(t.allocator, dealloc(old_entries.data);)
}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
void table_ensure_space(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, s64 items) {
    auto& t = *table;
    if (((t.slots_filled + items) * 100) >= (t.allocated * t.LOAD_FACTOR_PERCENT)) table_expand(table);
}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
void table_init(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, s64 slots_to_allocate = 0) {
    remember_allocators(table);
    table_resize(table, slots_to_allocate);
}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
void table_deinit(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table) {
    push_allocator(table->allocator, dealloc(table->entries.data);)
}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
void table_reset(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table) {
    auto& t = *table;

    t.count        = 0;
    t.slots_filled = 0;
    for (auto& entry : t.entries) {
        entry.hash = NEVER_OCCUPIED_HASH;
    }
}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
auto table_add(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, Arg<Key_Type> key, Arg<Value_Type> value) -> Value_Type* {
    auto& t = *table;
    static_assert((t.LOAD_FACTOR_PERCENT > 0) && (t.LOAD_FACTOR_PERCENT < 100), "Load_Factor_Percent must be between 1 and 99");

    if (((t.slots_filled + 1) * 100) > (t.allocated * t.LOAD_FACTOR_PERCENT)) table_expand(table);

    // Assert(t.slots_filled < t.allocated);

    Walk_Table(
        if (t.REFILL_REMOVED) {
            if (t.entries[index].hash == REMOVED_HASH) {
                t.slots_filled -= 1;
                break;
            }
        }
    )

    t.count        += 1;
    t.slots_filled += 1;

    auto& entry = t.entries[index];
    entry.hash  = hash;
    entry.key   = key;
    entry.value = value;

    return &entry.value;
}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
auto table_find_pointer(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, Arg<Key_Type> key) -> Value_Type* {
    auto& t = *table;
    if (!t.allocated) return nullptr;

    Walk_Table(
        auto& entry = t.entries[hash];
        if ((entry.hash == hash) && (entry.key == key)) {
            return &entry.value;
        }
    )

    return nullptr;
}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
auto table_set(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, Arg<Key_Type> key, Arg<Value_Type> value) -> Value_Type* {
    auto value_ptr = table_find_pointer(table, key);
    if (value_ptr) {
        *value_ptr = value;
        return value_ptr;
    }

    return table_add(table, key, value);
}

// NOTE(WALKER): Will reconsider this, might not be worth it to have really

// template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
// void table_find(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, s64 slots_to_allocate = 0) {}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
auto table_contains(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, Arg<Key_Type> key) -> bool {
    return table_find_pointer(table, key) != nullptr;
}

template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
auto table_find_multiple(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, Arg<Key_Type> key) -> Array_View<Value_Type> /* Uses temp_allocator */ {
    auto& t = *table;
    if (!t.allocated) return Array_View<Value_Type>{};

    Resizable_Array<Value_Type> results = {};
    results.allocator = context.temp_allocator;

    Walk_Table(
        auto& entry = t.entries[index];
        if ((entry.hash == hash) && (entry.key == key)) {
            array_add(&results, entry.value);
        }
    )

    return results;
}

// TODO(WALKER): multi-return value stuff, figure it out at some point

// template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
// void table_remove(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, Arg<Key_Type> key) {}

// template<typename Key_Type, typename Value_Type, u32 Load_Factor_Percent, bool Refill_Removed>
// void table_find_or_add(Hash_Table<Key_Type, Value_Type, Load_Factor_Percent, Refill_Removed>* table, Arg<Key_Type> key) {}
