#include <dlfcn.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

__int64_t hook_func(void *, float sample_time, unsigned int *dummy_struct)
{
    printf("We are hooked!\n");
    printf("hook_func: %f {%d, %d}", sample_time, dummy_struct[0], dummy_struct[1]);

    return 0;
}

__attribute__((constructor)) void init()
{
    void *handle = dlopen("/home/pat/Documents/vtable-hook/libdummy.so", RTLD_NOLOAD | RTLD_NOW);
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

    void **test_vtable = *(void ***)test_class;
    printf("test_vtable = %p\n", test_vtable);

    __int64_t (*hook_me_original)(void *, float, unsigned int *) = test_vtable[4];
    printf("hookMe = %p\n", hook_me_original);

    long pagesize = sysconf(_SC_PAGESIZE);
    void *page_start = (void *)((__uint64_t)test_vtable & ~(pagesize - 1));
    printf("page_start = %p\n", page_start);

    if (mprotect(page_start, pagesize, PROT_READ | PROT_WRITE) != 0)
    {
        printf("mprotect failed to change page protection\n");
        return;
    }

    test_vtable[4] = hook_func;

    if (mprotect(page_start, pagesize, PROT_READ) != 0)
    {
        printf("mprotect faild to reset page protection\n");
        return;
    }

    printf("At this point we should be hooked...\n");
}