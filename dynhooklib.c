#include <dlfcn.h>
#include <stdio.h>

__attribute__((constructor)) void init()
{
    void *handle = dlopen("/home/pleb/vtable_hook/libdummy.so", RTLD_NOLOAD | RTLD_NOW);
    if (!handle)
    {
        printf("Failed to load libdummy.so\n");
        return;
    }

    void *(*CreateTestClass)() = dlsym(handle, "CreateTestClass");
    if (!CreateTestClass)
    {
        printf("Failed to find CreateTestClass\n");
        return;
    }

    void *testClass = CreateTestClass();

    printf("testClass = %p\n", testClass);
}