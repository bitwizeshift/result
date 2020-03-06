# Expected

## `expect` the `unexpected`

**Expected** is a light-weight error-handling mechanism that offers an
alternative to exception handling.

The implementation is based on a very simplified form of the `std::expected`
proposal in [P0323](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p0323r9.html),
using modern C++17 features like `std::variant` to implement this.

## Table of Contents

* [Rationale](#rationale) \
  A brief rational for what problem this intends to solve
* [Features](#features) \
  A summary of all existing features in **Expected**
* [API Reference](https://bitwizeshift.github.io/expected/api/latest/manual.html) \
  For doxygen-generated API information
* [Legal](doc/legal.md) \
  Information about how to attribute this project
* [How to install](doc/installing.md) \
  For a quick guide on how to install/use this in other projects
* [FAQ](doc/faq.md) \
  A list of frequently asked questions
* [Contributing Guidelines](.github/CONTRIBUTING.md) \
  Guidelines that must be followed in order to contribute to **Expected**

## Rationale

Error cases in C++ are often difficult to discern from the API. Any function
not marked `noexcept` can be assumed to throw an exception, but what kind of
exception, and if it even derives from `std::exception` is unclear. The best
way of determining this is from documentation which can easily fall out of date.

Having an `expected<T, E>` type on your API not only semantically encodes that
a function is _able to_ fail, it also indicates to the caller _how_ the function
may fail, and what discrete, testable conditions may cause it to fail.

## Features

**TODO**

## <a name="license"></a>License

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
