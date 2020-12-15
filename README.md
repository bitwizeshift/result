# Result

[![Ubuntu Build Status](https://github.com/bitwizeshift/result/workflows/Ubuntu/badge.svg?branch=master)](https://github.com/bitwizeshift/result/actions?query=workflow%3AUbuntu)
[![macOS Build Status](https://github.com/bitwizeshift/result/workflows/macOS/badge.svg?branch=master)](https://github.com/bitwizeshift/result/actions?query=workflow%3AmacOS)
[![Windows Build Status](https://github.com/bitwizeshift/result/workflows/Windows/badge.svg?branch=master)](https://github.com/bitwizeshift/result/actions?query=workflow%3AWindows)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/e163a49b3b2e4f1e953c32b7cbbb2f28)](https://www.codacy.com/gh/bitwizeshift/result/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=bitwizeshift/result&amp;utm_campaign=Badge_Grade)
[![Coverage Status](https://coveralls.io/repos/github/bitwizeshift/result/badge.svg?branch=master)](https://coveralls.io/github/bitwizeshift/result?branch=master)
[![Github Issues](https://img.shields.io/github/issues/bitwizeshift/result.svg)](http://github.com/bitwizeshift/result/issues)
<br>
[![Github Releases](https://img.shields.io/github/v/release/bitwizeshift/result.svg?include_prereleases)](https://github.com/bitwizeshift/result/releases)
[![Bintray Releases](https://api.bintray.com/packages/bitwizeshift/Result/Result%3Aresult/images/download.svg)](https://bintray.com/bitwizeshift/Result/Result%3Aresult/_latestVersion)
<br>
[![Try online](https://img.shields.io/badge/try-online-blue.svg)](https://godbolt.org/z/nKfqbK)

**Result** is a modern, simple, and light-weight error-handling alternative to exceptions.

## Teaser

```cpp
template <typename To, typename From>
auto try_narrow(const From& from) noexcept -> cpp::result<To,narrow_error>
{
  const auto to = static_cast<To>(from);

  if ((to < To{}) != (from < From{})) {
    return cpp::fail(narrow_error::sign_change);
  }
  if (static_cast<From>(to) != from) {
    return cpp::fail(narrow_error::loss_of_data);
  }
  return to;
}
```

<kbd>[Live Example](https://godbolt.org/z/nKfqbK)</kbd>

## Features

* [x] Offers a coherent, light-weight alternative to exceptions
* [x] Compatible with <kbd>C++11</kbd> (with more features in <kbd>C++14</kbd> and <kbd>C++17</kbd>)
* [x] Single-header, **header-only** solution -- easily drops into any project
* [x] Zero overhead abstractions -- don't pay for what you don't use.
* [x] No dependencies
* [x] Support for value-type, reference-type, and `void`-type values in `result`
* [x] Monadic composition functions like `map`, `flat_map`, and `map_error` for
      easy functional use
* [x] [Comprehensively unit tested](https://coveralls.io/github/bitwizeshift/result?branch=master) for both static
      behavior and runtime validation
* [x] [Incurs minimal cost when optimized](https://godbolt.org/z/M69T4v), especially for trivial types

For more details and examples on what is available in **Result**, please
check out the [tutorial](doc/tutorial.md) section.

For details describing how this implementation deviates from the
`std::result` proposals, see [this page](doc/deviations-from-proposal.md).

## Documentation

* [Background](#background) \
  A background on the problem **Result** solves
* [Installation](doc/installing.md) \
  For a quick guide on how to install/use this in other projects
* [Tutorial](doc/tutorial.md) \
  A quick pocket-guide to using **Result**
* [Examples](doc/examples.md) \
  Some preset live-examples of this library in use
* [API Reference](https://bitwizeshift.github.io/result/api/latest/) \
  For doxygen-generated API information
* [Attribution](doc/legal.md) \
  Information about how to attribute this project
* [FAQ](doc/faq.md) \
  A list of frequently asked questions

## Background

Error cases in C++ are often difficult to discern from the API. Any function
not marked `noexcept` can be assumed to throw an exception, but the exact _type_
of exception, and if it even derives from `std::exception`, is ambiguous.
Nothing in the language forces which exceptions may propagate from an API, which
can make dealing with such APIs complicated.

Often it is more desirable to achieve `noexcept` functions where possible, since
this allows for better optimizations in containers (e.g. optimal moves/swaps)
and less cognitive load on consumers.

Having a `result<T, E>` type on your API not only semantically encodes that
a function is _able to_ fail, it also indicates to the caller _how_ the function
may fail, and what discrete, testable conditions may cause it to fail -- which
is what this library intends to solve.

As a simple example, compare these two identical functions:

```cpp
// (1)
auto to_uint32(const std::string& x) -> std::uint32_t;

// (2)
enum class parse_error { overflow=1, underflow=2, bad_input=3};
auto to_uint32(const std::string& x) noexcept -> result<std::uint32_t,parse_error>;
```

In `(1)`, it is ambiguous _what_ (if anything) this function may throw on
failure, or how this error case may be accounted for.

In `(2)`, on the other hand, it is explicit that `to_uint32` _cannot_ throw --
so there is no need for a `catch` handler. It's also clear that it may fail for
whatever reasons are in `parse_error`, which discretely enumerates any possible
case for failure.

## Optional Features

Although not required or enabled by default, **Result** supports two optional
features that may be controlled through preprocessor symbols:

1. Using a custom namespace, and
2. Disabling all exceptions

### Using a Custom Namespace

The `namespace` that `result` is defined in is configurable. By default,
it is defined in `namespace cpp`; however this can be toggled by defining
the preprocessor symbol `RESULT_NAMESPACE` to be the name of the desired
namespace.

This could be done either through a `#define` preprocessor directive:

```cpp
#define RESULT_NAMESPACE example
#include <result.hpp>

auto test() -> example::result<int,int>;
```

<kbd>[Try Online](https://godbolt.org/z/a4GccT)</kbd>

Or it could also be defined using the compile-time definition with `-D`, such
as:

`g++ -std=c++11 -DRESULT_NAMESPACE=example test.cpp`

```cpp
#include <result.hpp>

auto test() -> example::result<int,int>;
```

<kbd>[Try Online](https://godbolt.org/z/5xTsdj)</kbd>

### Disabling Exceptions

Since `result` serves to act as an orthogonal/alternative error-handling
mechanism to exceptions, it may be desirable to not have _any_ exceptions at
all. IF the compiler has been configured to disable exception entirely, simply
having a path that even encounters a `throw` -- even if never reached in
practice may trigger compile errors.

To account for this possibility, **Result** may have exceptions removed by
defining the preprocessor symbol `RESULT_DISABLE_EXCEPTIONS`.

Note that if this is done, contract-violations will now behave differently:

* Contract violations will call `std::abort`, causing immediate termination
  (and often, core-dumps for diagnostic purposes)
* Contract violations will print directly to `stderr` to allow context for the
  termination
* Since exceptions are disabled, there is no way to perform a proper stack
  unwinding -- so destructors will _not be run_. There is simply no way to
  allow for proper RAII cleanup without exceptions in this case.

<kbd>[Try Online](https://godbolt.org/z/bjbqaG)</kbd>

## Building the Unit Tests

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

## Compiler Compatibility

**Result** is compatible with any compiler capable of compiling valid
<kbd>C++11</kbd>. Specifically, this has been tested and is known to work
with:

* GCC 5, 6, 7, 8, 9, 10
* Clang 3.5, 3.6, 3.7, 3.8, 3.9, 4, 5, 6, 7, 8, 9, 10, 11
* Apple Clang (Xcode) 10.3, 11.2, 11.3, 12.3
* Visual Studio 2015, 2017, 2019

Latest patch level releases are assumed in the versions listed above.

## License

<img align="right" src="http://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png">

**Result** is licensed under the
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
