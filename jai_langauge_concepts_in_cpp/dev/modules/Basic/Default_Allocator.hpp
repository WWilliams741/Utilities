
struct Default_Allocator {};

auto default_allocator_proc(Allocator_Mode mode, s64 requested_size, s64, void* old_memory, void*) -> void* {
    switch(mode) {
        case ALLOCATE:   { return malloc(requested_size); }
        case REALLOCATE: { return realloc(old_memory, requested_size); }
        case DEALLOCATE: { free(old_memory); break; }
    }

    return nullptr;
}
