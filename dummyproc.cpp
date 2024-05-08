#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "dummylib/dummylib.hpp"

int main()
{
    dlopen("./thiswillfail", RTLD_LAZY);
    ITestInterface *test = CreateTestClass();
    
    test->TestMethod();

    printf("test->TestMethod2(1, 2): %d\n", test->TestMethod2(1, 2));
    
    printf("test->TestHookMe(1.0f, {1, 2}): %s\n", test->TestHookMe(1.0f, new DummyStruct{1, 2}) ? "true" : "false");
    
    test->TestMethod3();

    while (true)
    {
        printf("Press any key to run TestHookMe again...\n");
        getchar();
        printf("test->TestHookMe(1.0f, {1, 2}): %s\n", test->TestHookMe(1.0f, new DummyStruct{1, 2}) ? "true" : "false");
    }
    
    delete test;
    return 0;
}