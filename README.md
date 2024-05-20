
# Compile with:
```bash
g++ dummylib/dummylib.cpp -shared -fpic -o libdummy.so -std=c++17
```
```bash
g++ dummyproc.cpp -L. -ldummy -ldl -o dummyproc -std=c++17
```
```bash
gcc dynhooklib.c -shared -fpic -o dynhooklib.so
```

# Run main proc with:
```bash
LD_LIBRARY_PATH=. ./dummyproc
```

# Load dynhooklib with:
```bash
sudo bash gdb-dlopen.sh
```