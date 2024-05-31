#include <dlfcn.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

int (*original_func)(void *, int, int) = NULL;
void **vtable = NULL;
long page_size = 0;
void *page_start = NULL;

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

int test_hook(void * this, int x, int y)
{
    printf("Hello from the hook! x: %d, y: %d\n", x, y);

    return 0;
}

__attribute__((constructor)) void init()
{
    void *lib_handle = dlopen("./dummylib.so", RTLD_NOLOAD | RTLD_LAZY);

    if (!lib_handle)
    {
        printf("Failed to load library\n");
        return;
    }

    void *(*factory)() = dlsym(lib_handle, "CreateTestClass");

    if (!factory)
    {
        printf("Failed to get factory function\n");
        return;
    }
    
    void *test = factory();

    vtable = *(void ***)test;
    original_func = vtable[0];

    page_size = sysconf(_SC_PAGESIZE);
    page_start = (void *)((__uint64_t)vtable & ~(page_size - 1));

    write_to_table(vtable, 0, test_hook);
}

__attribute__((destructor)) void unload()
{
    write_to_table(vtable, 0, original_func);
}