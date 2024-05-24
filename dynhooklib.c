#include <dlfcn.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

__int64_t (*original_func)(void *, float, unsigned int *) = NULL;
void **test_vtable = NULL;

__int64_t hook_func(void *, float sample_time, unsigned int *dummy_struct)
{
    printf("We are hooked!\n");
    printf("hook_func: %f {%d, %d}", sample_time, dummy_struct[0], dummy_struct[1]);

    return 0;
}

void write_to_table(void **vtable, int index, void *func)
{
    long page_size = sysconf(_SC_PAGESIZE);
    void *page_start = (void *)((__uint64_t)vtable & ~(page_size - 1));
    printf("page_start = %p\n", page_start);

    if (mprotect(page_start, page_size, PROT_READ | PROT_WRITE) != 0)
    {
        printf("mprotect failed to change page protection\n");
        return;
    }

    vtable[index] = func;

    if (mprotect(page_start, page_size, PROT_READ) != 0)
    {
        printf("mprotect faild to reset page protection\n");
        return;
    }
}

__attribute__((constructor)) void init()
{
    void *handle = dlopen("./libdummy.so", RTLD_NOLOAD | RTLD_NOW);
    if (!handle)
    {
        printf("Failed to load libdummy.so\n");
        return;
    }

    void *(*create_test_class)() = dlsym(handle, "CreateTestClass");
    if (!create_test_class)
    {
        printf("Failed to find CreateTestClass\n");
        return;
    }

    void *test_class = create_test_class();
    printf("test_class = %p\n", test_class);

    test_vtable = *(void ***)test_class;
    printf("test_vtable = %p\n", test_vtable);

    original_func = test_vtable[4];
    printf("hookMe = %p\n", original_func);

    write_to_table(test_vtable, 4, hook_func);

    printf("At this point we should be hooked...\n");
}

__attribute__((destructor)) void unload()
{
    if (original_func)
    {
        write_to_table(test_vtable, 4, original_func);
    }

    printf("Original function restored\n");
}