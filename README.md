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

This guide assumes you have basic knowledge of polymorphism, vtables, and their usage. As a refresher, virtual function tables are a result of the `virtual` keyword in C++. When a function in a base class is marked with the `virtual` keyword, derrived classes may override that function. Then, at runtime when the function is called, the address of the function is resolved using the actual runtime object's type. We'll take a closer look at how this all works in a moment. For now we can think of it like this: for a base class with virtual methods there is a list of overwritable methods we can choose to overwrite when we create a derrived class. Say we have a `virtual void printType()` function in our `Shape` base class that prints "I am a Shape" to the console. We could create a new type `Square` that inherits from `Shape` and since `printType` is in the list of virtual methods we can override it and make it print "I am a Square" instead. We can see that in action here:

```cpp
#include <stdio.h>

struct Shape
{
    virtual void printType() { printf("I am a Shape\n"); }
}

struct Square : Shape
{
    virtual void printType() override { printf("I am a Square\n"); }
}

void printShape(Shape& shape)
{
    shape.printType();
}

Square square;
// Should print "I am a Square"
printShape(square);
```

In this example, both `Shape` and `Square` each have vtables that are identical in structure.

### Writing a dummy class

Back to our application. We're going to start focusing less on C++ semantics and more on memory structures. I encourage you to brush up on C++ and pure virtual functions at this point.

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

Now let's use a common polymorphic technique you'll almost certainly come across in practice, a factory function! Factory functions are common to return a concrete implementation of an interface. You'll likely see something like this:

```cpp
ITestInterface *test = CreateInterface<ITestInterface>();
test->TestMethod();
```

The actual type of `test` doesn't really matter to the caller, we can trust `TestMethod` was implemented.
