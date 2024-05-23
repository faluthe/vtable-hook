# Virtual Function Table Hooking With GNU C: A Comprehensive Guide

### Try it out:

#### Compile with

```bash
g++ dummylib/dummylib.cpp -shared -fpic -o libdummy.so -std=c++17

g++ dummyproc.cpp -L. -ldummy -ldl -o dummyproc -std=c++17

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

Context of the hook:
We'll be putting the target function of the hook in a library (a dummy library/dummylib), compiled separate from the main application. This library will be loaded dynamically at runtime by the main application. Through this we'll demonstrate how libraries are usually dynamically loaded (with LD_PRELOAD/LD_LIBRARY_PATH and dlopen) and then we'll go on to use the GNU Debugger to load our own library into whatever application we want.

So our application's source directory looks like this:

```
Main Application
├── Library
│   ├── dummylib.hpp
│   └── dummylib.cpp
└── dummyproc.cpp
```

We'll start with the centerpiece: a class with a virtual function table (vtable) and a target function to hook.

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
}

struct TestClass : ITestInterface
{
    virtual int TestMethod(int x, int y) override;
}

```

```cpp
// dummylib.cpp
#include <dummylib.hpp>
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

For our sample application we only need to return one derrived class of one interface, so this will do. 

### The Main Dummy Application

Let's finish up by creating the main part of the dummy application that calls the `TestMethod` function from `dummylib`.

```cpp
// dummyproc.cpp
#include <stdio.h>
#include <stdlib.h>

#include <Library/dummylib.hpp>

int main()
{
    ITestInterface *test = CreateTestClass();

    while (true)
    {
        test->TestMethod();
        printf("Press any key to run TestMethod again...\n");
        getchar();
    }

    delete test;

    return 0;
}
```

Here we call `TestMethod` any time a key is pressed, so when we are hooked we can test the hook by pressing a key.

### Compiling the Dummy Application

We've got two separate pieces of our application: the main process and the library. These two pieces will need to be linked together in order for the main process to resolve the `TestMethod` function. 

To keep this part of the guide brief, please see the [Guide to Compiling Libraries](docs/libraries.md) for a deeper dive into C/C++ library compilation and linkage.

### Running the Dummy Application

```bash
LD_LIBRARY_PATH=. ./dummyproc
```

### Dissecting the Dummy Application/Library