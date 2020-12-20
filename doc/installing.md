# Installing

Since **Result** is a single-header, header-only library -- there are
various possible options for how this could be installed and used in
any project.

The short version is that, in order to use this, you must at
least have a `-I` to the `include` directory of this project, or
directly to the directory `result.hpp` is installed in.

You can easily copy the `result.hpp` file wherever you would like into
your project, `git submodule` this repository in, install it with your
favorite package manager, etc.

However, if you are using either [`conan`](#conan)  or [`CMake`](#cmake)
(or both), you can also follow the additional instructions below:

## Conan

This project provides a `conanfile.py` and public hosting on
[bintray](https://bintray.com/bitwizeshift/public-conan/Result%3Aresult) for packages.

Just follow the instructions in the bintray link for setting up this
source as a conan repository, and add
`Result/<the version you want>@result/stable` as a dependency in your
`conanfile.py`, and you can install this into your project.

## CMake

This project is written with idiomatic & "modern" `CMake` (3.4+) and
provides the target `Result::Result` which can be used as a dependency
in your cmake build.

You can either [add this project via `add_subdirectory`](#via-subdirectory)
or [pull this project in with a `find_package`](#via-installation)
call after installing it.

### Via subdirectory

If you have added this repo as a `git submodule`, or as a subtree,
or just straight-up copied directly into your repo, you can add this
project via `add_subdirectory` call before your target so that you can
use the `Result::Result` target.

For example:

```cmake
# Add the 'Result' repo sometime before you depend on the target
add_subdirectory("external/Result")

# ...

add_library(MyLibrary ...)
target_link_libraries(MyLibrary
  PRIVATE Result::Result
)
```

And in your implementation of `MyLibrary`, you can easily include
the project with `#include "result.hpp"`!

### Via installation

You can also create an installation package of **Result** and
share with other users, or just use locally on your system.

Then, to consume this package you just need to find it with
`find_package`. For example:

```cmake
find_package(Result REQUIRED)

# ...

add_library(MyLibrary ...)
target_link_libraries(MyLibrary
  PRIVATE Result::Result # 'PRIVATE" assuming private dependency...
)
```

And in your implementation of `MyLibrary`, you can easily include
the project with `#include "result.hpp"`!
