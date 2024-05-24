# Virtual Function Table Hooking With GNU C: A Comprehensive Guide

### Try it out:

#### Compile with

```bash
g++ dummylib/dummylib.cpp -shared -fpic -o libdummy.so -std=c++17

g++ dummyproc.cpp -L. -ldummy -o dummyproc -std=c++17

gcc dynhooklib.c -shared -fpic -o dynhooklib.so
```

#### Run the main process with

```bash
LD_LIBRARY_PATH=. ./dummyproc
```

#### Load the hook with

```bash
sudo bash gdb-dlopen.sh
```

**_Start the main process. Then, from another terminal, initialize the hook._**

#

## WiP Guide


### Context of the hook
We'll be putting the target function of the hook in a dummy library (dummylib), **compiled separate from the main application**. This library will be loaded dynamically at runtime by the main application. This allows developers to avoid lengthy compilation times; if you change a part of the library's code you only need to recompile the library, the main application will remain the same and resolve references to the library's code at runtime. We'll demonstrate how libraries are usually dynamically loaded through our test application and then we'll go on to use the GNU Debugger to inject a library into an already running application.

So our test application's source directory looks like this:

```
Main Application
├── Library
│   ├── dummylib.hpp
│   └── dummylib.cpp
└── dummyproc.cpp
```

`dummylib.cpp` will be compiled into `libdummy.so`. `dummyproc.cpp` will be compiled into `dummyproc`. At runtime `dummyproc` will link with `libdummy.so` and will call the target function defined in `libdummy.so`. We will then, from a remote process, initialize the vtable hook. After the hook is in place all calls to the target function will instead call our function.


We'll start with the centerpiece of the test application: a class with a virtual function table (vtable) and a target function to hook.

### What is `virtual`?

This guide assumes you have basic knowledge of polymorphism, vtables, and their usage. As a refresher, please see the [What is Virtual?](docs/virtual.md) guide.

### Writing a Dummy Class

We're going to focus less on C++ semantics and more on memory structures. I encourage you to brush up on C++ and pure virtual functions at this point.

To model our application after the applications we will see in practice, we'll start by defining an interface ([What is an Interface?](docs/virtual.md/#what-is-an-interface)) and a derrived class.

```cpp
// dummylib.hpp

struct ITestInterface
{
    virtual int TestMethod(int x, int y) = 0;
};

struct TestClass : ITestInterface
{
    virtual int TestMethod(int x, int y) override;
};
```

```cpp
// dummylib.cpp
#include "dummylib.hpp"
#include <stdio.h>

int TestClass::TestMethod(int x, int y)
{
    printf("x: %d, y: %d\n", x, y);

    return x + y;
}
```

We now have our target function to hook: `TestMethod`. Notice that `TestMethod` is a virtual function, so a vtable will be generated for `TestClass`. We'll take a look at the generated vtable within the compiled library in a moment.

### A Simple Factory Function

Now let's use a common polymorphic technique you'll almost certainly come across in practice, a factory function! Factory functions are common to return a concrete implementation of an interface. You'll likely see something like this:

```cpp
ITestInterface *test = CreateInterface<ITestInterface>();
test->TestMethod();
```

The actual type of `test` doesn't really matter to the caller, we can trust `TestMethod` was implemented because it was defined in the interface. We'll define the factory function for our application like this:
```cpp
// dummylib.hpp

...

ITestInterface *CreateTestClass();
```
```cpp
// dummylib.cpp

...

ITestInterface *CreateTestClass()
{
    return new TestClass();
}
```

For our sample application we only need to return one derrived class of one interface, so this will do. You'll see why factory functions are used in the context of dynamic libraries in the next section.

### The Main Dummy Application

Let's finish up by creating the main part of the dummy application that calls the `TestMethod` function from `dummylib`.

```cpp
// dummyproc.cpp
#include <stdio.h>
#include <stdlib.h>

#include "Library/dummylib.hpp"

int main()
{
    ITestInterface *test = CreateTestClass();

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
```

Here we call `TestMethod` any time a key is pressed, so when we are hooked we can test the hook by pressing a key.

Let's try it out.

```bash
pat@ubuntu:~/vtable-hook$ g++ dummyproc.cpp -o dummyproc

/usr/bin/ld: /tmp/ccaIjGCG.o: in function `main':
dummyproc.cpp:(.text+0xd): undefined reference to `CreateTestClass()'
collect2: error: ld returned 1 exit status
```

Of course, we recieve a linker error. The linker isn't able to find the `CreateTestClass` function, as we've only included the header for dummylib and haven't compiled the library yet. We can certainly include it as a target of our current compilation using:

```bash
g++ dummyproc.cpp Library/dummylib.cpp -o dummyproc
``` 

This will produce our desired result, but we want to separate the main application and the library into separate binaries. There are a few ways we can achieve this separatation: [Static Linking](docs/libraries.md), Dynamic Linking, and [Semi-Dynamic Linking](docs/libraries.md). We'll be using <u>dynamic linking</u>, although it is important for you to understand the differences between static and dynamic linking.

### Compiling `dummylib.cpp` as a Dynamic Library

To compile as a dynamic library we'll make use of a few GNU compiler options. The [gcc manual page](https://man7.org/linux/man-pages/man1/gcc.1.html) is very long, but it is a very good reference for our purposes. Search through it as necessary.

We'll be using the `-shared` option to compile `dummylib.cpp` as a shared object. From the man page:

```md
-shared
    Produce a shared object which can then be linked with other
    objects to form an executable.  Not all systems support this
    option.  For predictable results, you must also specify the
    same set of options used for compilation (-fpic, -fPIC, or
    model suboptions) when you specify this linker option.
```

This also clues us in to common options used with `-shared`, namely `-fpic` and `-fPIC` which we will be using to compile `dummylib.cpp`. Generally you should use `-fpic`, you can read more on the [man page](https://man7.org/linux/man-pages/man1/gcc.1.html). A lot of GNU Compiler options are prefaced with `-f`, meaning "flag" or "feature". In this case we want to tell the compiler to use the <u>Platform Independent Code</u> feature, which addresses objects relatively instead of absolutely. More on this in the next section.

We can compile from the root directory with:

```bash
g++ Library/dummylib.cpp -shared -fpic -o dummylib.so
```

### Platform Independent Code

PIC addresses objects in memory by relative address, not absolute address. This is a extremely important concept to understand when trying to reverse engineer. 

PIC is important for shared objects, because they do not know where they will be placed in memory. This is in contrast to an executable, who due to virtual memory can address absolutely. 

When an executable is loaded into memory the operating system creates a <u>virtual address space</u> for it and assigns the executable a <u>base address</u> within the virtual address space. The executable can then use absolute addresses, the operating system will adjust the addresses based on the assigned base address, and the Memory Management Unit will translate the virtual addresses into physical addresses in RAM.

Shared objects, however, are not loaded at a specific base address and instead loaded at any available address in the virtual address space. They must use relative addressing, which adds an offset to the instruction pointer to address objects.

To fully understand this, see [what the difference actually looks like in practice](docs/libraries.md/#pic-vs-non-pic).

### Dynamically Linking Using `dlopen` and `dlsym`

Now that we've compiled our library, we need to link it with our main application. We'll do this by calling the `dlopen` function from `dlfcn.h`. The `dlopen` function will load the shared object into the main application's virtual address space (if not already loaded) and return a handle (pointer) to the shared object.

Let's add this call to the main application:

```cpp
// dummyproc.cpp
#include <dlfcn.h>
...

int main()
{
    void *libHandle = dlopen("./dummylib.so", RTLD_LAZY);
    ...

    return 0;
}
```

We can use the `RTLD_LAZY` RunTime LoaDer flag, which uses lazy bindings.

Okay, so we've loaded the library and we have a handle to it, but we still can't resolve the reference to `CreateTestClass`. If you try to compile after adding the call to `dlopen`, you'll recieve the same error as before. We need to use the handle to resolve the symbol.

We'll do this by calling the `dlsym` function, also from `dlfcn.h`. The `dlsym` function is used to obtain the address of a symbol in a shared object. `dlsym` takes a handle and a symbol name and returns the adress of where the symbol is loaded.

```cpp
// dummyproc.cpp
#include <dlfcn.h>
...

int main()
{
    void *libHandle = dlopen("./dummylib.so", RTLD_LAZY);
    void *createTestClassPtr = dlsym(libHandle, "CreateTestClass");
    printf("%p\n", createTestClassPtr);
    ...

    return 0;
}
```

At this point we should expect this to work, but if you ran this code you would find it prints `(nil)`, because `dlsym` is not able to find the `CreateTestClass` symbol. Interestingly, if you were to convert this project to C instead of C++ at this point this example WOULD work (note: you'd have to dumb down `TestClass`). This is because during the compilation of C++ code <u>Name Mangling</u> occurs. C++ has to mangle symbol names due to features like function overloading and namespaces; the mangled names encode information like parameter types and namespace to create a unique symbol. C does not have these features so it does not mangle symbol names.

We can read what symbols are available to us through examining a compiled binary with the `nm` command line utility. We'll use the `-D` option to analyze the data section.

```bash
pat@ubuntu:~/vtable-hook$ nm -D dummylib.so

...

00000000000011b6 T _Z15CreateTestClassv
00000000000011e8 W _ZN14ITestInterfaceC1Ev
00000000000011e8 W _ZN14ITestInterfaceC2Ev
000000000000117a T _ZN9TestClass10TestMethodEii

...
```

Here we can see the mangled name of our function, `_Z15CreateTestClassv` as well as the other symbols available to us. By default all symbols are exported, however usually developers will change this and only export a select few functions, like a factory function!

Let's now change our `dlsym` call to include the mangled name.

```cpp
// dummyproc.cpp
#include <dlfcn.h>
...

int main()
{
    void *libHandle = dlopen("./dummylib.so", RTLD_LAZY);
    void *factoryPtr = dlsym(libHandle, "_Z15CreateTestClassv");
    printf("%p\n", factoryPtr);
    ...

    return 0;
}
```

Running this should now dynamically load `dummylib` and print the address of the `CreateTestClass` function. To call the function we can simply cast the void pointer to a function pointer:

```cpp
void *factoryPtr = dlsym(libHandle, "_Z15CreateTestClassv");
ITestInterface *(*factory)() = (ITestInterface *(*)()) factoryPtr;
ITestInterface *test = factory();
```

Here we define a function pointer, named `factory`, that takes no arguments and returns an `ITestInterface` pointer. We are casting `factoryPtr` from a void pointer to a function pointer that returns an `ITestInterface` pointer and takes no arguments. Again, I don't want to dive into C/C++ semantics too much.

With that we have our finished application:

```cpp
// dummyproc.cpp
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "Library/dummylib.hpp"

int main()
{
    void *libHandle = dlopen("./dummylib.so", RTLD_LAZY);
    void *factoryPtr = dlsym(libHandle, "_Z15CreateTestClassv");

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
```

If you have already compiled `dummylib.so` then you can compile and run the main application with:

```bash
g++ dummyproc.cpp -o dummyproc

./dummyproc
```

Remember: you don't need to recompile the library and the main process if you only change one of them and it does not matter what order you compile them in.

### Avoiding Name Mangling in C++

We can actually avoid name mangling in C++ by adding `extern "C"` to our function prototype, which tells the compiler to use C linkage (no name mangling) for that function.

```cpp
// dummylib.hpp

...

extern "C" ITestInterface *CreateTestClass();
```

```cpp
// dummyproc.cpp

...

void *factoryPtr = dlsym(libHandle, "CreateTestClass");

...