ccint: a C/C++ interpreter 
===

ccint is a C/C++ interpreter, built on top of Clang and LLVM compiler infrastructure. 

<img src="doc/ccint.svg" alt="ccint internals"  width="600"/>

## features

* supports all C++11/C++14/C++17 features 
* supports for linking static/dynamic libraries
* high performance: same performance as C/C++ binaries
* separate parser and execution engine

## build

```
$ git clone git@github.com:llvm/llvm-project.git
$ cd llvm-project/clang/tools/
$ git clone git@github.com:remysys/ccint.git
$ echo "add_clang_subdirectory(ccint)" >> CMakeLists.txt
$ cd ../..
$ cmake -S llvm -B build -G "Unix Makefiles"  -DLLVM_ENABLE_PROJECTS="clang" -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi"  -DCMAKE_BUILD_TYPE=Release
$ cd build && make -j16
```

## usage

```
./clang-ccint --help
USAGE: clang-ccint [options] <input file>

OPTIONS:
General options:
  -I <string>                                        - specify include paths
  -L <string>                                        - load given libs
```
### examples

* print "hello world"
```
/* main.cpp */
#include <stdio.h>
#include <vector>
#include <string>

void ccint_main() {
  std::vector<std::string> vec = {"hello", " world\n"};
  for (auto &s : vec) {
    printf("%s", s.c_str());
  }
}

$ ./ccint main.cpp
hello world
```

* link static library
```
/* add.h */
int add(int, int);

/* add.c */
int add(int a, int b) {
  return a + b;
}

/* main.cpp */
#include <stdio.h>
#include "add.h"

void ccint_main() {
  printf("32 + 64 = %d\n", add(32, 64));
}

$ clang -c add.c -o add.o
$ ar rcs libadd.a add.o
$ ./ccint main.cpp -L ./libadd.a
32 + 64 = 96
```

* link dynamic library

```
/* add.h */
int add(int, int);

/* add.c */

int add(int a, int b) {
  return a + b;
}

/* main.cpp */

#include <stdio.h>
#include "add.h"

void ccint_main() {
  printf("32 + 64 = %d\n", add(32, 64));
}

$ clang -shared -fPIC add.c -o libadd.so
$ ./ccint main.cpp -L ./libadd.so
32 + 64 = 96
```

* specify include paths

```
/* add.h */
int add(int, int);

/* add.c */

int add(int a, int b) {
  return a + b;
}

/* main.cpp */

#include <stdio.h>
#include "add.h"

void ccint_main() {
  printf("32 + 64 = %d\n", add(32, 64));
}

$ clang -shared -fPIC add.c -o libadd.so
$ mv ./add.h ..
$ ./ccint main.cpp -L ./libadd.so -I ..
32 + 64 = 96
```