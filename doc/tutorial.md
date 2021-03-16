# Tutorial

**Result** is has a small API surface area of only two types:

* `result<T,E>`, and
* `failure<E>`

The former is used to denote the expected result along with the possible failure,
and the latter represents a possible error type.

## Table of Contents

1. [The Basics](#the-basics)
    1. [Using `result` in an API](#using-result-in-an-api)
    2. [Returning References](#returning-references)
    3. [Returning `void`](#returning-void)
    4. [Using Assignment](#using-assignment)
    5. [Checking for error state](#checking-for-error-state)
    6. [assuming no error](#assuming-no-error)
2. [Advanced](#advanced)
    1. [Monadic-functions](#monadic-functions)
    2. [Type-erasure with `result<void,e>`](#type-erasure-with-resultvoide)
    3. [`failure` with references](#failure-with-references)
3. [Optional Features](#optional-features)
    1. [Using a custom namespace](#using-a-custom-namespace)
    2. [Disabling exceptions](#disabling-exceptions)

## The Basics

### Using `result` in an API

The `cpp::result` class is used to convey an API's intended return value, along
with the API's potential failure conditions. In most cases, the error-type
will often be a discrete list of error-types like a custom `enum class`, or a
`std::error_code`/`std::error_condition` type.

The common way to use `cpp::result` is to return objects using the `T`-types
move-constructors and leveraging `cpp::result`'s implicit conversion. Error
types are denoted by using the `cpp::failure<E>` type:

```cpp
auto to_double(const char* str) noexcept -> cpp::result<double, std::errc>
{
  auto* last_entry = static_cast<char*>(nullptr);

  errno = 0;
  const auto result = std::strtod(str, &last_entry);

  if (errno != 0) {
    // Returns an error value
    return cpp::fail(static_cast<std::errc>(errno));
  }
  // Returns a value
  return result;
```

**Note:** When working in <kbd>C++17</kbd> `fail` can be replaced by
using `failure`, which will leverage CTAD to determine the error type.

In the case that either the `T` or the `E` type being returned are large and/or
complex, `result` also exposes in-place construction using either `in_place`
for constructing `T` directly, or `in_place_error` for constructing `E`
directly. This is more verbose, and thus not the recommended/idiomatic approach
-- but is offered as a 0-cost alternative to move-construction:

```cpp
auto widget_repository::try_get_widget() -> cpp::result<widget,widget_error>
{
  if (widgets_unavailable()) {
    return cpp::result<widget,widget_error>{
      cpp::in_place_error,
      foo, bar, /* arguments to widget_error's constructor */
    };
  }
  return cpp::result<widget,widget_error>{
    cpp::in_place,
    foo, bar, /* arguments to widget's constructor */
  };
}
```

### Returning references

It's not uncommon in APIs to want to return a reference to something that may
conditionally fail or be unavailable -- a simple example of this is container
`at(...)` functions which idiomatically throw exceptions on failure.

This is something that can also be modeled by this utility by using
`result<T&,E>` -- where the `T` type is an lvalue-reference. In this case,
the value you return is an lvalue reference to an outside object. For example:

```cpp
template <typename T>
auto my_vector<T>::at(std::size_t n) noexcept -> cpp::result<T&,my_error>
{
  if (n >= m_size) {
    return cpp::fail(my_error::out_of_range);
  }
  return m_storage[n]; // <- returns m_storage[n] by *reference*
}
```

Using a `result<T&,E>` type is the same any `result`, except it
behaves through indirection like a pointer. Assigning a value to a
`result<T&,E>` will rebind the reference:

```cpp
auto a = int{};
auto b = int{};

cpp::result<int&,int> x = a; // x refers to 'a'
x = b; // x now refers to 'b'
```

-------------------------------------------------------------------------------

Since `result` supports conversion-constructors and assignments, it's also
possible to return `result` references of types that behave
polymorphically.

For example:

```cpp
struct base {};
struct derived : base {};

auto d = derived{};

// Holds onto a base-reference, refers to derived.
auto exp = cpp::result<base&,int>{d};
```

### Returning `void`

It's not uncommon to want to represent functions that don't have any data worth
returning to the caller -- yet may also be, itself, fallible. This is not
uncommon for class member functions performing internal operations, such as
a `start()` or `stop()` function on a service. These cases may not have anything
useful to return to the caller other than a success or failure state.

Such a case is handled easily using the `result<void,E>` specialization,
for example:

```cpp
class service
{
  auto start() noexcept -> cpp::result<void,service_error>;
};
```

When returning a `void` value to a result, you must return a
default-constructed `result` object, since you _are_ returning a value; it's
just a no-op value:

```cpp
auto service::start() noexcept -> cpp::result<void,service_error>
{
  ...
  return {}; // <- return's an result<void,E> in success state
}
```

Although this may be represented as well in other ways, such as returning an
error-code, this does not identify the _semantic_ meaning the same way.
`result<void,E>` provides a consistent way of marking-up functions that may
fail. More importantly, it also applies consistent semantics and helpful
utiliy functionality that returning a raw error type would not.

### Using assignment

A `result<T,E>` object may be assigned to by other `result` objects
provided that both `T` and `E` are assignable and construction would be
non-throwing. Additionally, the value or error types can be directly assigned to
via `T` or `failure<E>` types provided the respective constructors would be
non-throwing as well:

```cpp
auto exp = cpp::result<unsigned,error_type> {42};

exp = 0xdeadbeef; // changes underlying value

exp = cpp::fail(error_type::some_error); // changes to error

exp = cpp::result<unsigned short,error_type>{0}; // conversion-constructs

// etc
```

The non-throwing requirement is necessary since assigning `result<T,E>`
objects may result in the active type changing. Changing the active type
requires destruction of the current type, which would otherwise leave this type
vulnerable to potentially being _stuck_ in this state if we allowed the new
type's constructor to throw. By forcing a requirement of `noexcept`, we ensure
that we can't be left in this valueless-by-exception state -- which helps us
ensure we can always guarantee containing _some_ value.

This does not mean that assignment will not be available at all if construction
isn't directly non-throwing. In fact, **as long as `T` and `E` have either a
non-throwing _move_ or _copy_ constructor, you can still perform assignment** --
it will just construct an intermediate `result<T,E>` object first to ensure
that no object becomes valueless. If the intermediate object's construction
throws, the `result` being assigned to is unchanged -- which is what we want.

For example, a `result<std::string, int>` can still assign a `const char*`,
even though `std::string(const char*)` isn't `noexcept`:

```cpp
auto exp = cpp::result<std::string,int>{"hello world"};

exp = "goodbyte world!";
```

The assignment will construct an intermediate `result` object _first_ before
leveraging `std::string`'s non-throwing move-constructor.

In general, this restriction should have minimal impact on most workflows.
Very seldomly is it necessary to assign or change the result of a `result`,
since the common case for this type is to be used as a `return` type from an
API. In the unlikely event that the assignment is even needed, we at least
can guarante that the `result` remains coherent and always contains a value.

### Checking for error state

The current state of a `result` object can be queried in a few different ways:

#### Checking `has_error()` or `has_value()`

To do a quick top-level query of whether the `result` contains a value or an
error result, you can simply query `has_value()` or the inverse `has_error()`:

```cpp
if (exp.has_value()) {
  /* exp contains a value */
}
```

or

```cpp
if (exp.has_error()) {
  /* exp contains an error
}
```

The `has_value()` will always yield the opposite result of `has_error()`; the
two functions only exist for the purpose of symmetry.

#### Extracting the current error and directly checking against it

A `result`'s error type can be directly extracted using the `error()`
function. Doing this will return either a default constructed `E` error if the
`result` contains a value, or it will extract a copy (or move) of the
underlying error (depending on whether it is an lvalue or rvalue reference).

A default constructed `E` type is always assumed to be a "success" state for the
error indicator. This design is intentional to reduce the number of exceptions
the API may throw in. This makes it easy for consumers to request the error
state without caring whether it contains a proper error (for example, for
logging purposes).

```cpp
auto error = exp.error(); // or `std::move(exp).error()` for rvalues

if (error == /* some error */) { ... }
```

#### Comparing with the underlying error directly

You may use the `failure` type to compare directly with an underlying error
at any point to avoid the need to branch on `has_value()` explicitly.
For example:

```cpp
if (exp == cpp::fail(error::my_error)) {
  /* `exp` contains something equality-comparable to `error::my_error` */
}
```

#### Comparing with the underlying value directly

Alternatively, you may also compare `result` objects directly with an
underlying value, if there is a particular state that is expected. This prevents
the need to check for discrete error types if the value is of more interest than
the error is.

```cpp
auto exp = try_to_uint8("255");

if (exp == 255) {
  /* exp contains a value equality-comparable to 255 */
}
```

### Assuming no error

Sometimes you may be calling a fallible function and not care to handle the
failure case. However, thanks to `result` being marked `[[nodiscard]]`, you
must still consume the result otherwise you will be hit with a warning (or
error with `-Werror`/`/Wx`). Thankfully, there exists an easy way out:
`result::expect`.

If you want to make an assumption that a result contains a proper value, you
may call `expect` with a desired message. On failure, an exception will be
thrown that contains the specified message along with the underlying error, and
on success the code will proceed as planned. This ensures that the code is not
left in an error-state, and consumes the `result`.

```cpp
auto start_service() -> cpp::result<void,service_error>;

auto test() -> void {
  start_service().expect("Service failed to start!");
}
```

## Advanced

### Monadic functions

The `result` class offers monadic functionality to enable simple composability
and construction of return types. This allows functional chaining of calls to
produce simple and coherent expressions.

The supported monadic functionalities are:

* `value_or`: Gets the current value, or a supplied alternative
* `error_or`: Gets the current error, or a supplied alternative
* `and_then`: _If_ the `result` contains a value, creates a `result` with
  the supplied value. Otherwise creates the `result` contain the error
* `map`: Executes a function on the current value (if any) and returns an
  `result` containing the result. If it contains an error, this returns a
  `result` containing that error.
* `flat_map`: Similar to `map`, except it assumes that the function itself
  returns an `result` object
* `map_error`: Similar to `map`, except it operates on the error rather than the
  value

For example:

```cpp
// (1) 'value_or'
auto value = res.value_or(42);
// gets the current value _or_ 42

// (2) 'error_or'
auto error = res.error_or(error_code::some_distinct_error);
// Gets the stored error, or the supplied error

// (3) `and_then`
auto next_res = res.and_then("hello world");
// if 'exp' contains a value, creates an result with "hello world";
// otherwise creates an error

// (4) `map`
auto next_res = res.map(to_string);
// Calls 'to_string' on the stored result value, creating an
// 'result<string,E>' on return

// (5) `flat_map`
auto next_res = res.map(to_uint);
// Tries to convert the stored value to an integral value, which may itself
// return an result value

// (6) `map_error`
auto consumer_res = internal_res.map_error(to_external_error);
// Calls 'to_external_error' to convert an internal error code to an external
// (consumer-facing) error-code
```

A practical example of this composition is chaining a conversion of a
`string` into an integral value, mapping that value to an enum and converting
the parse-error to a user-facing error:

```cpp
auto try_to_uint8(const std::string&)
  noexcept -> cpp::result<std::uint8_t,parse_error>;
auto to_client_code(std::uint8_t)
  noexcept -> client_code;
auto to_user_error(parse_error)
  noexcept -> user_error;

// Example composition:

auto result = try_to_uint8(str).map(to_client_code).map_error(to_user_error);
```


#### Chaining Member Functions

The various monadic functions in `result` are made to work with any invocable
expression. Since member pointers are valid for the purposes of invoke
expressions, this allows for a really convenient way to chain functions on a
`result` result while also propagating the potential error.

For example:

```cpp
enum class some_error : int;
auto try_get_string() -> cpp::result<std::string,some_error>;

// ...

auto exp = try_get_string().map(&std::string::size);
```

<kbd>[Live example](https://godbolt.org/z/vbeWc1)</kbd>


### Type-erasure with `result<void,E>`

The `result<void,E>` specialization is constructible from any `result<T,E2>`
type as long as `E2` is convertible to `E`. This allows for a very simple and
effective form of type-erasure in composition if the error is important but the
result can be discarded.

For example, it's easy to create compositions that simply discard the T result:

```cpp
template <typename Fn>
auto try_invoke(Fn&& fn) noexcept -> void
{
  // Coalesce all results to 'void'
  auto result = cpp::result<void,E>{
    std::invoke(std::forward<Fn>(fn))
  };
  if (!result) {
    // Do something with the error
    signal_service::notify_error(result.error());
  }
}
```

**Note:** Erasure with `result<void,E>` requires explicit construction for both
construction and assignment, since `result<void,E>` is meant to model the
behavior and semantics of a `void` cast, which requires explicitness.

### `failure` with references

For the common-case, `result`'s error-types are generally intended to be
lightweight and inexpensive to construct/copy/move; however, there will always
be exceptions to the rule. In some cases, you may already have an instance of an
expensive `E` error type to compare against, and can't afford to copy it for
an `failure` object.

In such cases, you can construct `failure` objects that refer to the local
instance without paying for the cost of a copy. To do this, you can use either
`std::ref` with `cpp::fail` (or `failure` with CTAD in <kbd>C++17</kbd>)

For example:

```cpp
const auto some_error_object = /* some expensive object */

if (exp == cpp::fail(std::ref(some_error_object))) {
  ...
}
```


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

<kbd>[Try Online](https://godbolt.org/z/Pe7e6e)</kbd>

Or it could also be defined using the compile-time definition with `-D`, such
as:

`g++ -std=c++11 -DRESULT_NAMESPACE=example test.cpp`

```cpp
#include <result.hpp>

auto test() -> example::result<int,int>;
```

<kbd>[Try Online](https://godbolt.org/z/Kxf8nr)</kbd>

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

<kbd>[Try Online](https://godbolt.org/z/9sYrec)</kbd>
