# Building the Unit Tests

If you're thinking to contribute to this project, you're going to need to build
the unit tests first.

Building the unit tests are not necessary to use this project. However, if
you are interested in running these yourself, you will require
the following installed:

* [CMake](https://cmake.org): Used for configuring/building the project
* [Catch2](https://github.com/catchorg/Catch2): the unit-test library

Additionally, you will need to toggle the `RESULT_COMPILE_UNIT_TESTS` option
during cmake configuration to ensure that unit tests configure and build.

The easiest way to install Catch2 is using the [`conan`](https://conan.io/index.html)
package manager and installing with `conan install <path to conanfile>` from your
build directory.

A complete example of configuring, compiling, and running the tests:

```sh
# Make the build directory and enter it
mkdir build && cd build
# Install Catch with conan (optional)
conan install ..
# Configure the project
cmake .. -DRESULT_COMPILE_UNIT_TESTS=On
# Build everything
cmake --build .
# run the tests
cmake --build . --target test
```
