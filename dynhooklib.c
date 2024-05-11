#include <dlfcn.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

// tdb why is this camel case lmao

__int64_t hookFunc(void*, float sampleTime, unsigned int* dummyStruct)
{
    printf("We are hooked!\n");
    printf("hookFunc: %f {%d, %d}", sampleTime, dummyStruct[0], dummyStruct[1]);

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

    void *(*CreateTestClass)() = dlsym(handle, "CreateTestClass");
    if (!CreateTestClass)
    {
        printf("Failed to find CreateTestClass\n");
        return;
    }

    void *testClass = CreateTestClass();
    printf("testClass = %p\n", testClass);

    void **testVtable = *(void ***)testClass;
    printf("testVtable = %p\n", testVtable);

    __int64_t (*hookMeOriginal)(void*, float, unsigned int*) = testVtable[4];
    printf("hookMe = %p\n", hookMeOriginal);

    long pagesize = sysconf(_SC_PAGESIZE);
    void *pageStart = (void*)((__uint64_t)testVtable & ~(pagesize - 1));
    printf("pageStart = %p\n", pageStart);

    if (mprotect(pageStart, pagesize, PROT_READ | PROT_WRITE) != 0)
    {
        printf("mprotect failed to change page protection\n");
        return;
    }
    
    testVtable[4] = hookFunc;

    if (mprotect(pageStart, pagesize, PROT_READ) != 0)
    {
        printf("mprotect faild to reset page protection\n");
        return;
    }

    printf("At this point we should be hooked...\n");
}