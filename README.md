# Virtual Function Table Hooking With GNU C: A Comprehensive Guide

### Try it out:

#### Compile with

```bash
g++ Library/dummylib.cpp -shared -fpic -o dummylib.so -std=c++17

g++ dummyproc.cpp -ldl -o dummyproc -std=c++17

gcc hook.c -shared -fpic -o hook.so
```

#### Run the main process with

```bash
./dummyproc
```

#### Load the hook with

```bash
sudo bash gdb-dlopen.sh
```

*_Start the main process. Then, from another terminal, initialize the hook._*

# Everything You Need to Write a VTable Hook

We'll write a test library and a test application that calls a function from the test library. Then, we'll inject another library that hooks the function from the test library.

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

Note: throughout this guide **I won't be checking return values**. Read the [`dlopen` man page](https://man7.org/linux/man-pages/man3/dlopen.3.html) to learn what these functions return upon error.

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
```

Note that you have to recompile both the main process and library if you make this addition.

```bash
g++ dummyproc.cpp -o dummyproc

g++ Library/dummylib.cpp -shared -fpic -o dummylib.so

./dummyproc

x: 1, y: 2
TestMethod(1, 2) returned: 3
Press any key to run TestMethod again...
```

### Components of the Hook

Now that we've got our test application and test library fully functional, let's dive into the hook. For simplicity I'll be writing the hook in C. I'll also put it in the same directory as the test application/library, though this is not necessary.

So our source directory now looks like this:

```
Main Application
├── Library
│   ├── dummylib.hpp
│   └── dummylib.cpp
└── dummyproc.cpp
└── hook.c
```

We'll compile the hook as a library and then use the GNU Debugger to inject the shared object. So we have two components of the hook: `hook.c` and GDB. You'll notice a bash script in this repository to handle the GDB part, titled `gdb-dlopen.sh`, but we'll be [injecting the library](#injecting-the-library) manually in this guide. 

### GNU C Attributes

GNU C attributes are used to add additional information about variables, functions, or types to the GNU Compiler. Attributes are specified using the `__attribute__` keyword followed by double parentheses enclosing an attribute.

Attributes like

```c
int old_function() __attribute__((deprecated));
```
will generate warning if the function or variable is used.

We'll be using two attributes in `hook.c`: 

```c
__attribute__((constructor))

__attribute__((destructor))
```
Functions marked with `constructor` and `destructor` will run before and after the `main` function respectively. In the context of libraries, since they do not have a `main` function, they are used to initalize library resources and perform any needed setup. We'll set up our hook in the constructor and restore it in the destructor.

To get up and running let's write a simple hello world:

```c
// hook.c
#include <stdio.h>

__attribute__((constructor)) void init()
{
    printf("Hello world!\n");
}

__attribute__((destructor)) void unload()
{
    printf("Goodbye world!\n");
}
```

We can compile this the exact same [way we compiled `dummylib.cpp`](#compiling-dummylibcpp-as-a-dynamic-library), this time using `gcc` instead of `g++`:

```bash
gcc hook.c -shared -fpic -o hook.so
```

### Injecting the Library

You should probably have some familarity with GDB at this point but it is not strictly necessary.

First, launch the `dummyproc` and ensure it is operational.

```bash
./dummyproc
```

Then, from another terminal, we'll attach GDB to the running `dummyproc`. We can do this a couple of ways: launch `gdb` with the `--pid=1234` option or by using the `attach 1234` command in the GDB terminal. Both methods require a process ID, which we can obtain using the `pgrep dummyproc` command. I'll attach with:

```bash
gdb --pid=$(pgrep dummyproc)
```

Loading the library is actually really simple, we just call `dlopen`!

```bash
(gdb) call dlopen("./hook.so", 1)
$1 = (void *) 0x55f084a56860
```

Remember, `dlopen` takes a path to a shared object and a flag variable and returns a handle to the loaded library. Here the flag is set to 1/`RTLD_LAZY`.

Switch back to the terminal running `dummyproc`:
```bash
x: 1, y: 2
TestMethod(1, 2) returned: 3
Press any key to run TestMethod again...
Hello world!
```

Our library is loaded! We can unload from `gdb` using `dlclose` and the handle that was previously returned by `dlopen`:

```bash
(gdb) call dlopen("./hook.so", 1)
$1 = (void *) 0x55f084a56860
(gdb) call dlclose((void *) 0x55f084a56860)
$2 = 0
```

```
x: 1, y: 2
TestMethod(1, 2) returned: 3
Press any key to run TestMethod again...
Hello world!
Goodbye world!
```

### Caveats

At this point, an intuitive programmer may find themselves asking, "Would this work if we had not imported `dlfcn.h` in `dummyproc.cpp?`" The answer is YES! We've only imported `dlopen` and `dlsym` in `dummyproc.cpp` to demonstrate how libraries are usually loaded. Calling `dlopen` from GDB is possible for most applications, even those who have not explicitly imported `dlfcn.h`.

[Read more on this](/docs/libraries.md) to understand why it works.

IMPORTANT: **You may also need to cast `dlopen` when calling it!!** Meaning you may need to call it from `gdb` with:

```bash
call ((void * (*) (const char *, int)) dlopen)("$LIB_PATH", 1)
```

IMPORTANT: **You may need to link using -ldl** to use `dlopen`.
 
I strongly suggest you read more on that [here](/docs/libraries.md).

### Getting a Pointer to CreateTestClass

At this point we've got a pretty good grasp on dynamic libraries. We'll get a pointer to the `CreateTestClass` function in `hook.c` through the same process as the [Dynamically Linking Using `dlopen` and `dlsym`](#dynamically-linking-using-dlopen-and-dlsym) section.

Here's how we did it in `dummyproc.cpp`:

```cpp
// dummyproc.cpp
...

#include "Library/dummylib.hpp"

int main()
{
    void *libHandle = dlopen("./dummylib.so", RTLD_LAZY);
    void *factoryPtr = dlsym(libHandle, "CreateTestClass");

    ITestInterface *(*factory)() = (ITestInterface *(*)()) factoryPtr;
    ITestInterface *test = factory();

    ...

```

We'll do the same thing in `hook.c`, with a couple of modifications:

```c
// hook.c
#include <dlfcn.h>

__attribute__((constructor)) void init()
{
    void *lib_handle = dlopen("./dummylib.so", RTLD_NOLOAD | RTLD_LAZY);
    void *(*factory)() = dlsym(lib_handle, "CreateTestClass");
    void *test = factory();
    printf("test: %p\n", test);
}

...
```

```md
x: 1, y: 2
TestMethod(1, 2) returned: 3
Press any key to run TestMethod again...
test: 0x55ca0d616b90
```

Firstly, we're getting a handle to `dummylib.so` with the `RTLD_NOLOAD` flag as well as `RTLD_LAZY`. Recall from the [Dynamically Linking Using `dlopen` and `dlsym`](#dynamically-linking-using-dlopen-and-dlsym) section that the `dlopen` function will <u>load the shared object into the main application's virtual address space (if not already loaded)</u> and return a handle (pointer) to the shared object. By using `RTLD_NOLOAD` we signal `dlopen` to NOT load the library and to only return a handle if it is already open. In a real world application here's where you would check that the application has already properly loaded the library you desire instead of potentially loading it an incorrect time. 

Secondly, we've avoided the extra `factoryPtr` step and casted the result of `dlsym` directly. You'll also notice that the return value of the `factory` function in `hook.c` is no longer an `ITestInterface *`, instead it just a `void *`. This is because in a real world application we likely wouldn't have access to `dummylib.hpp`, if we wanted to use a class from the application  we would have to reverse that class manually and reconstruct it. We'll only be using pointers from the test application, so `void *` will suffice. The only thing we care about is the size of the pointer; generally all pointers are the same size: 4 bytes in a 32-bit application and 8 bytes in a 64-bit application, with an exception being near offsets which you'll often see in [Platform Independent Code](#platform-independent-code).

### Getting the Address of the Virtual Function Table

So, now `hook.c` is able to get a pointer, returned from `CreateTestClass`. Let's remind ourselves what that function returns:

```cpp
ITestInterface *CreateTestClass()
{
    return new TestClass();
}
```

The function returns a pointer to <u>an instance of `TestClass`</u> somewhere on the heap. We printed out the value stored at this pointer (0x55ca0d616b90) in the last section. We'll take a look at that address in GDB in a moment.

For the purposes of this section I'm going to add a few members to `TestClass`. **You don't necassarily need to do this**, these members make no difference in hooking `TestMethod`.

```cpp
// dummylib.hpp

...

struct TestClass : ITestInterface
{
    virtual int TestMethod(int x, int y) override;
    virtual void TestMethod2() override;
    virtual void TestMethod3() override;

    int m_x;
    int m_y;

    bool TestMethod4();

    TestClass()
    {
        m_x = 0x2024;
        m_y = 0x1337;
    }
};

...
```

Now let's analyze the <u>instance</u> of `TestClass`, at 0x55ca0d616b90:

```md
pat@ubuntu:~/vtable-hook$ sudo gdb --pid=$(pgrep dummyproc)

...

(gdb) x/10xg 0x55ca0d616b90
0x55ca0d616b90: 0x00007ff2e305dd98      0x0000133700002024
0x55ca0d616ba0: 0x00007ff2e2c90968      0x0000000000000031
0x55ca0d616bb0: 0x000055ca0d616bc8      0x0000000000000000
0x55ca0d616bc0: 0x00007ff200000000      0x6c796d6d75642f2e
0x55ca0d616bd0: 0x0000006f732e6269      0x00000000000001e1
```

We've used the examine (`x`) GDB command, asking for 10 addresses, in hexidecimal (`x`) format, with giant word/8 byte sized (`g`) structure. To analyze 10 word/4 byte sized strucutres in hexidecimal format use `x/10xw address`.

Notice the 0x2024 and 0x1337 values are present, each are 4 bytes in size and since we are analyzing giant words (8 bytes) they are combined into a single output. You'll also notice the next 8 bytes after the 0x1337 and 0x2024 value's is an address, which we can assume to be the address of `TestMethod4`. 

A trained eye may also notice the first member of the structure is also an address. All <u>instances</u> of C++ classes containing virtual functions contain a hidden pointer, the `vptr`, that points to the VTable! This pointer will be located at the base address of the structure.

While there may be many instances of a class, there is only one VTable.

Let's examine the address stored in the `vptr`:

```md
(gdb) x/10a 0x7ff2e305dd98
0x7ff2e305dd98: 0x7ff2e305b19a      0x7ff2e305b1d4
0x7ff2e305dda8: 0x7ff2e305b1f4      0x0
0x7ff2e305ddb8: 0x7ff2e305ddf0      0x7ff2e2d19120
0x7ff2e305ddc8: 0x7ff2e2d19120      0x7ff2e2d19120
0x7ff2e305ddd8: 0x7ff2e2e41c98      0x7ff2e305c038
```

Here we are examing 10 addresses (`a`) which are 8 bytes (with leading 0's truncated).

Generally, the end of a structure in memory is marked with a null pointer (`0x0`). You can see this in the output above at 0x7ff2e305ddb0 and in the `TestClass` structure at 0x55ca0d616bb8. Before the null/end marker in the output above we can see 3 addresses, this is our VTable! 

`0x7ff2e305b19a`,`0x7ff2e305b1d4`, and `0x7ff2e305b1f4`, correspond to `TestMethod`, `TestMethod2`, and `TestMethod3` respectively.

We can even confirm this by looking at `TestMethod2` in memory. I've defined `TestMethod2` as:

```cpp
int TestClass::TestMethod2()
{
    return 5;
}
```

And here's `TestMethod2` at `0x7ff2e305b1d4`:

```md
(gdb) x/10i 0x7ff2e305b1d4
0x7ff2e305b1d4: endbr64
0x7ff2e305b1d8: push   %rbp
0x7ff2e305b1d9: mov    %rsp,%rbp
0x7ff2e305b1db: mov    %rdi,-0x8(%rbp)
0x7ff2e305b1df: mov    $0x5,%eax
0x7ff2e305b1e4: pop    %rbp
0x7ff2e305b1e5: ret    
0x7ff2e305b1e6: nop    
0x7ff2e305b1e7: endbr64
0x7ff2e305b1eb: push   %rbp
```

At `0x7ff2e305b1df`, the integer value 5 is loaded into the `eax` register, the register used to return integers and pointers from functions.

We should have everything we need to translate this to code in `hook.c`:

```c
// hook.c

...

__attribute__((constructor)) void init()
{
    void *lib_handle = dlopen("./dummylib.so", RTLD_NOLOAD | RTLD_LAZY);
    void *(*factory)() = dlsym(lib_handle, "CreateTestClass");
    void *test = factory();
    
    void **vtable = *(void ***)test;
    void *test_method = vtable[0];
    void *test_method2 = vtable[1];
    printf("TestMethod2: %p\n", test_method2);
}

...

```

We can treat the VTable as a list (pointer) of generic (void) pointers, hence `void **vtable`. We also know the first 8 bytes of `test` (`test` + 0) is a pointer to the vtable, so we cast it to a pointer to a list of pointers `(void ***)`.

Close `dummyproc` if you have it open, recompile `hook.c`, open `dummyproc`, and reinject `hook.so`.

```md
x: 1, y: 2
TestMethod(1, 2) returned: 3
Press any key to run TestMethod again...
TestMethod2: 0x7ff2e305b1d4
```

Note this address likely won't be the same as before if you've reopened `dummyproc`.

### Page Protection and Writing to The VTable

The actual hook is probably simpler than you think:

```c
// hook.c
...

int test_hook(void *, int x, int y)
{
    printf("Hello from the hook! x: %d, y: %d\n", x, y);

    return 3;
}

__attribute__((constructor)) void init()
{

    ...

    vtable[0] = test_hook;
}
```

We just set the first index in the vtable to point to `test_hook` instead of `TestMethod`! We must ensure that the hook function has the same prototype as `TestMethod` and have to include the `this` pointer in the hook function's prototype, hence the addition of the nameless `void *` parameter.

Of course it's not actually that easy. If you were to run this code you would recieve a segmentation fault as soon you write to the VTable. This is due to **page protection**.

Virtual memory is divided into pages and the operating system maintains a mapping of virtual pages to physical pages. If we print out the address of the VTable again, this time a new address, we can check what page the VTable is on and if we can read/write to that page. 

```md
x: 1, y: 2
TestMethod(1, 2) returned: 3
Press any key to run TestMethod again...
vtable: 0x7f3d2913ad78
```

We'll check what page `0x7f3d2913ad78` is on using `GDB`:

```md
(gdb) info proc mappings
process 80964
Mapped address spaces:

          Start Addr           End Addr       Size     Offset  Perms  objfile
      0x55f9430aa000     0x55f9430ab000     0x1000        0x0  r--p   ...

      ...

      0x7f3d29139000     0x7f3d2913a000     0x1000     0x2000  r--p   ...
      0x7f3d2913a000     0x7f3d2913b000     0x1000     0x2000  r--p   ...
      0x7f3d2913b000     0x7f3d2913c000     0x1000     0x3000  rw-p   ...
      0x7f3d2913c000     0x7f3d2913e000     0x2000        0x0  rw-p
```

`0x7f3d2913ad78` is on the `0x7f3d2913a000` - `0x7f3d2913b000` page which has `r--` (read-only) permissions. To write to the VTable we'll need to change the permissions on this page.

We can change page permissions using the `mprotect` function from `sys/mman.h`. According to the [the `mprotect` man page](https://man7.org/linux/man-pages/man2/mprotect.2.html), the function takes the starting address of the page, the size of the page, and the new permissions.

```c
int mprotect(void *addr, size_t len, int prot);
```

We'll need to calculate the starting address of the page dynamically; the VTable won't always be `0xd78` away from the start of the page, but we do know that the page is `0x1000` (4096 bytes) in size. This means we can round down to the nearest `0x1000` to get the start of the page.

I recommend you brush up on your bitwise operations at this point. We'll be using this formula:

```md
(VTable Address) & ~((Page Size) - 1)
```

We'll bitwise AND the VTable address with size of the page minus one. It works like this:

```md
1.
              4096                     4095
0x0000000000001000 - 1 = 0x0000000000000fff

2.

NOT( 0x0000000000000fff ) = 0xfffffffffffff000

3.

AND( 0x00007f3d2913ad78 ,
     0xfffffffffffff000 )
=    0x00007f3d2913a000     

```

We should have what we need to put it into `hook.c`:

```c
// hook.c

...

#include <sys/mman.h>
#include <unistd.h>

int test_hook(void *, int x, int y)
{
    printf("Hello from the hook! x: %d, y: %d\n", x, y);
}

__attribute__((constructor)) void init()
{
    void *lib_handle = dlopen("./dummylib.so", RTLD_NOLOAD | RTLD_LAZY);
    void *(*factory)() = dlsym(lib_handle, "CreateTestClass");
    void *test = factory();

    void **vtable = *(void ***)test;

    long page_size = sysconf(_SC_PAGESIZE);
    void *page_start = (void *)((__uint64_t)vtable & ~(page_size - 1));
    printf("page_start = %p\n", page_start);

    mprotect(page_start, page_size, PROT_READ | PROT_WRITE);
    vtable[0] = test_hook;
    mprotect(page_start, page_size, PROT_READ);
}

...
```

We can use `sysconf` from `unistd.h` to safely get the page size (4096). We must cast the vtable to an integer type (of the same size) in order to do arithmetic on it. To be safe we also restore the original read-only permissions back to the page after we've written to it.

```md
x: 1, y: 2
TestMethod(1, 2) returned: 3
Press any key to run TestMethod again...
page_start = 0x7f7c60829000


Hello from the hook! x: 1, y: 2
TestMethod(1, 2) returned: 0
Press any key to run TestMethod again...
```

We are officially hooked! Don't forget to `continue` in GDB before running `TestMethod` again.

### Restoring the Hook

Recall from the [GNU C Attributes](#gnu-c-attributes) section that code marked with `__attribute__((destructor))` will execute upon the closing of the shared object. We'll use this to write back the original address of the `TestMethod` function to the VTable.

Our final `hook.c`:

```c
// hook.c

#include <dlfcn.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

int (*original_func)(void *, int, int) = NULL;
void **vtable = NULL;
long page_size = 0;
void *page_start = NULL;

int test_hook(void * this, int x, int y)
{
    printf("Hello from the hook! x: %d, y: %d\n", x, y);

    return 0;
}

__attribute__((constructor)) void init()
{
    void *lib_handle = dlopen("./dummylib.so", RTLD_NOLOAD | RTLD_LAZY);
    void *(*factory)() = dlsym(lib_handle, "CreateTestClass");
    void *test = factory();

    vtable = *(void ***)test;
    original_func = vtable[0];

    page_size = sysconf(_SC_PAGESIZE);
    page_start = (void *)((__uint64_t)vtable & ~(page_size - 1));

    mprotect(page_start, page_size, PROT_READ | PROT_WRITE);
    vtable[0] = test_hook;
    mprotect(page_start, page_size, PROT_READ);
}

__attribute__((destructor)) void unload()
{
    mprotect(page_start, page_size, PROT_READ | PROT_WRITE);
    vtable[0] = original_func;
    mprotect(page_start, page_size, PROT_READ);
}
```

Remember, we can unload the shared object with `dlclose`. See the [Injecting The Library](#injecting-the-library) section for more.

```md
x: 1, y: 2
TestMethod(1, 2) returned: 3
Press any key to run TestMethod again...
                                        <--- INJECTED HERE
Hello from the hook! x: 1, y: 2
TestMethod(1, 2) returned: 0
Press any key to run TestMethod again...
                                        <--- CALLED dlclose HERE
x: 1, y: 2
TestMethod(1, 2) returned: 3
Press any key to run TestMethod again...
```
