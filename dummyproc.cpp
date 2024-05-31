#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "Library/dummylib.hpp"

int main()
{
    void *libHandle = dlopen("/home/pleb/test-vtable-hook/dummylib.so", RTLD_LAZY);

    if (!libHandle)
    {
        fprintf(stderr, "Failed to load library: %s\n", dlerror());
        return 1;
    }

    void *factoryPtr = dlsym(libHandle, "CreateTestClass");

    if (!factoryPtr)
    {
        fprintf(stderr, "Failed to find factory function: %s\n", dlerror());
        return 1;
    }

    ITestInterface *(*factory)() = (ITestInterface *(*)()) factoryPtr;
    ITestInterface *test = factory();

    while (true)
    {
        int i = test->TestMethod(1, 2);
        printf("TestMethod(1, 2) returned: %d\n", i);
        printf("Press any key to run TestMethod again...\n");
        getchar();
    }

    delete test;

    return 0;
}