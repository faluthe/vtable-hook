#!/bin/bash

# Compile with:
# g++ dummylib/dummylib.cpp -shared -fpic -o libdummy.so -std=c++17
# g++ dummyproc.cpp -L. -ldummy -ldl -o dummyproc -std=c++17
# gcc dynhooklib.c -shared -fpic -o dynhooklib.so

# Run main proc with:
# LD_LIBRARY_PATH=. ./dummyproc

LIB_NAME=dynhooklib
LIB_PATH=$(pwd)/$LIB_NAME.so
PROCID=$(pgrep dummyproc | head -n 1)

# Verify permissions
if [[ "$EUID" -ne 0 ]]; then
    echo "Please run as root"
    exit 1
fi

# Check if library exists
if [ ! -f "$LIB_PATH" ]; then
    echo "Library $LIB_PATH not found"
    exit 1
fi

# Check if library is already loaded
if lsof -p $PROCID 2> /dev/null | grep -q $LIB_PATH; then
    echo "Library is already loaded"
    exit 1
fi

# Check if the process is running
if [ -z "$PROCID" ]; then
    echo "Process is not running"
    exit 1
fi

unload() {
    echo -e "\nUnloading library..."

    if [ -z "$LIB_HANDLE" ]; then
        echo "Handle not found"
        exit 1
    fi

    gdb -n --batch -ex "attach $PROCID" \
                   -ex "call ((int (*) (void *)) dlclose)((void *) $LIB_HANDLE)" \
                   -ex "detach" > /dev/null 2>&1

    echo "Library has been unloaded!"
}

trap unload SIGINT

LIB_HANDLE=$(gdb -n --batch -ex "attach $PROCID" \
                            -ex "call ((void * (*) (const char *, int)) dlopen)(\"$LIB_PATH\", 1)" \
                            -ex "detach" 2> /dev/null | grep -oP '\$1 = \(void \*\) \K0x[0-9a-f]+')

if [ -z "$LIB_HANDLE" ]; then
    echo "Failed to load library"
    exit 1
fi

echo "Library loaded successfully at $LIB_HANDLE. Use Ctrl+C to unload."

tail -f ./$LIB_NAME.log