
void array_reserve_nonpoly(Resizable_Array<void*>* arr, s64 desired_items, s64 size) {
    auto& a = *arr;

    if (desired_items <= a.allocated) return;

    if (!a.allocator.proc) remember_allocators(arr);

    push_allocator(a.allocator,
        a.data = (void**) realloc(a.data, desired_items + size, a.allocated + size);
    )

    a.allocated = desired_items;
}

template<typename T>
void maybe_grow(Resizable_Array<T>* arr) {
    auto& a = *arr;

    if (a.count >= a.allocated) {
        auto reserve = a.allocated * 2;
        if (reserve < 8) reserve = 8;
        array_reserve_nonpoly((Resizable_Array<void*>*) arr, reserve, sizeof(T));
    }
}

template<typename T>
void array_dealloc(Resizable_Array<T>* arr) {
    auto& a = *arr;
    push_allocator(a.allocator, dealloc(a.data);)
}

template<typename T>
void array_reset(Resizable_Array<T>* arr) {
    auto& a = *arr;

    push_allocator(a.allocator, dealloc(a.data);)

    a.count     = {};
    a.data      = {};
    a.allocated = {};
}

template<typename T>
void array_reset_keep_memory(Resizable_Array<T>* arr) {
    arr->count = 0;
}

template<typename T>
void array_reserve(Resizable_Array<T>* arr, s64 desired_items) {
    array_reserve_nonpoly((Resizable_Array<void*>*) arr, desired_items, sizeof(T));
}

template<typename T>
void array_resize(Resizable_Array<T>* arr, s64 new_count) {
    auto& a = *arr;

    auto old_count = a.count;
    array_reserve_nonpoly((Resizable_Array<void*>*) arr, new_count, sizeof(T));
    a.count = new_count;

    for (s64 i = old_count; i < new_count; ++i) {
        new (a.data + i) T;
    }
}

template<typename T>
void array_add(Resizable_Array<T>* arr, Arg<T> item) {
    auto& a = *arr;
    maybe_grow(arr);
    a.data[a.count] = item;
    a.count        += 1;
}

// Stack API:
template<typename T>
auto array_peek(Array_View<T> arr) -> T {
    return arr[arr.count - 1];
}
template<typename T>
auto array_peek_pointer(Array_View<T> arr) -> T* {
    return &arr[arr.count - 1];
}
template<typename T>
auto array_pop(Resizable_Array<T>* arr) -> T {
    auto& a = *arr;
    auto result = a[a.count - 1];
    a.count -= 1;
    return result;
}
