# Expected

[![Build Status](https://github.com/bitwizeshift/expected/workflows/build/badge.svg)](https://github.com/bitwizeshift/expected/actions)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/e163a49b3b2e4f1e953c32b7cbbb2f28)](https://www.codacy.com/gh/bitwizeshift/expected/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=bitwizeshift/expected&amp;utm_campaign=Badge_Grade)
[![Github Issues](https://img.shields.io/github/issues/bitwizeshift/expected.svg)](http://github.com/bitwizeshift/expected/issues)
<br>
[![Compiler Compatibility](https://img.shields.io/badge/compilers-gcc%20%7C%20clang%20%7C%20msvc-blue.svg)](#compiler-compatibility)
[![GitHub License](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/bitwizeshift/expected/master/LICENSE)
[![Documentation](https://img.shields.io/badge/docs-doxygen-blue.svg)](https://bitwizeshift.github.io/expected/api/latest)
<br>
[![Try online](https://img.shields.io/badge/try-online-blue.svg)](https://gcc.godbolt.org/z/EoGb71)

**Expected** is a modern, simple, and light-weight error-handling alternative to exceptions.

## Teaser Sample

```cpp
template <typename To, typename From>
auto try_narrow(const From& from) noexcept -> expected<To,narrow_error>
{
  const auto to = static_cast<To>(from);

  if ((to < To{}) && (from < From{})) {
    return make_unexpected(narrow_error::sign_change);
  }
  if (static_cast<From>(to) != from) {
    return make_unexpected(narrow_error::loss_of_data);
  }
  return to;
}
```

[<kbd>Live Example</kbd>](https://gcc.godbolt.org/z/EoGb71)

## Features

* [x] Offers a coherent, light-weight alternative to exceptions
* [x] Compatible with <kbd>C++11</kbd> (with more features in <kbd>C++14</kbd> and <kbd>C++17</kbd>)
* [x] Single-header, **header-only** solution -- easily drops into any project
* [x] No dependencies
* [x] Support for value-type, reference-type, and `void`-type values in `expected`
* [x] Monadic composition functions like `map`, `flat_map`, and `map_error` for
      easy functional use
* [x] [Comprehensively unit tested](test/src/expected.test.cpp) for both static
      behavior and runtime validation

For more details and examples on what is available in **Expected**, please
check out the [tutorial](doc/tutorial.md) section.

For details describing how this implementation deviates from the
`std::expected` proposals, see [this page](doc/deviations-from-proposal.md).

## Documentation

* [Background](#background) \
  A background on the problem **Expected** solves
* [Tutorial](doc/tutorial.md) \
  Some simple references of how to use **Expected**
* [API Reference](https://bitwizeshift.github.io/expected/api/latest/) \
  For doxygen-generated API information
* [Legal](doc/legal.md) \
  Information about how to attribute this project
* [How to install](doc/installing.md) \
  For a quick guide on how to install/use this in other projects
* [FAQ](doc/faq.md) \
  A list of frequently asked questions
* [Contributing Guidelines](.github/CONTRIBUTING.md) \
  Guidelines that must be followed in order to contribute to **Expected**

## Background

Error cases in C++ are often difficult to discern from the API. Any function
not marked `noexcept` can be assumed to throw an exception, but the exact _type_
of exception, and if it even derives from `std::exception`, is ambiguous.
Nothing in the language forces which exceptions may propagate from an API, which
can make dealing with such APIs complicated.

Often it is more desirable to achieve `noexcept` functions where possible, since
this allows for better optimizations in containers (e.g. optimal moves/swaps)
and less cognitive load on consumers.

Having an `expected<T, E>` type on your API not only semantically encodes that
a function is _able to_ fail, it also indicates to the caller _how_ the function
may fail, and what discrete, testable conditions may cause it to fail -- which
is what this library intends to solve.

As a simple example, compare these two identical functions:

```cpp
// (1)
auto to_uint32(const std::string& x) -> std::uint32_t;

// (2)
enum class parse_error { overflow=1, underflow=2, bad_input=3};
auto to_uint32(const std::string& x) noexcept -> expected<std::uint32_t,parse_error>;
```

In `(1)`, it is ambiguous _what_ (if anything) this function may throw on
failure, or how this error case may be accounted for.

In `(2)`, on the other hand, it is explicit that `to_uint32` _cannot_ throw --
so there is no need for a `catch` handler. It's also clear that it may fail for
whatever reasons are in `parse_error`, which discretely enumerates any possible
case for failure.

## Building the Unit Tests

Building the unit tests are not necessary to use this project. However, if
you want to contribute to the project or simply test it yourself, you will need
the following installed:

* [CMake](https://cmake.org): Used for configuring/building the project
* [Catch2](https://github.com/catchorg/Catch2): the unit-test library

Additionally, you will need to toggle the `EXPECTED_COMPILE_UNIT_TESTS` option
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
cmake .. -DEXPECTED_COMPILE_UNIT_TESTS=On
# Build everything
cmake --build .
# run the tests
cmake --build . --target test
```

## Compiler Compatibility

**Expected** is compatible with any compiler capable of compiling valid
<kbd>C++11</kbd>. Specifically, this has been tested and is known to work
with:

* GCC 5, 6, 7, 8, 9, 10
* Clang 3.5, 3.6, 3.7, 3.8, 3.9, 4, 5, 6, 7, 8, 9, 10, 11
* Apple Clang (Xcode) 10.3, 11.2, 11.3, 12.3
* Visual Studio 2015, 2017, 2019

Latest patch level releases are assumed in the versions listed above.

## License

<img align="right" src="http://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png">

**Expected** is licensed under the
[MIT License](http://opensource.org/licenses/MIT):

> Copyright &copy; 2017 Matthew Rodusek
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## References

* [P0323R9](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0323r9.html):
  `std::expected` proposal was used as an inspiration for the general template
  structure.
* [bit::stl](https://github.com/bitwizeshift/bit-stl/blob/20f41988d64e1c4820175e32b4b7478bcc3998b7/include/bit/stl/utilities/expected.hpp): the original version that seeded this repository, based off an earlier proposal version.
