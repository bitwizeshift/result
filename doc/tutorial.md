# Tutorial

**Expected** is has a small API surface area of only two types:

* `expected<T,E>`, and
* `unexpected<E>`

The former is used to denote an expected result along with the possible failure,
and the latter represents a possible error type.

## Table of Contents

1. [The Basics](#the-basics)
    1. [Using `expected` in an API](#using-expected-in-an-api)
    2. [Returning References](#returning-references)
    3. [Returning `void`](#returning-void)
    4. [Using Assignment](#using-assignment)
    5. [Checking for error state](#checking-for-error-state)
2. [Advanced](#advanced)
    1. [Monadic-functions](#monadic-functions)
    2. [Type-erasure with `expected<void,e>`](#type-erasure-with-expectedvoide)
    3. [`unexpected` with references](#unexpected-with-references)

## The Basics

### Using `expected` in an API

The `expected` class is used to convey an API's intended return value, along
with the API's potential failure conditions. In most cases, the error-type
will often be a discrete list of error-types like a custom `enum class`, or a
`std::error_code`/`std::error_condition` type.

The common way to use `expected` is to return objects using the `T`-types
move-constructors and leveraging `expected`'s implicit conversion. Error types
are denoted by using the `unexpected<E>` type:

```cpp
auto to_double(const char* str) noexcept -> expected<double, std::errc>
{
  auto* last_entry = static_cast<char*>(nullptr);

  errno = 0;
  const auto result = std::strtod(str, &last_entry);

  if (errno != 0) {
    // Returns an error value
    return make_unexpected(static_cast<std::errc>(errno));
  }
  // Returns a value
  return result;
```

**Note:** When working in <kbd>C++17</kbd> `make_unexpected` can be replaced by
using `unexpected`, which will leverage CTAD to determine the error type.

In the case that either the `T` or the `E` type being returned are large and/or
complex, `expected` also exposes in-place construction using either `in_place`
for constructing `T` directly, or `in_place_error` for constructing `E`
directly. This is more verbose, and thus not the recommended/idiomatic approach
-- but is offered as a 0-cost alternative to move-construction:

```cpp
auto widget_repository::try_get_widget() -> expected<widget,widget_error>
{
  if (widgets_unavailable()) {
    return expected<widget,widget_error>{
      in_place_error,
      foo, bar, /* arguments to widget_error's constructor */
    };
  }
  return expected<widget,widget_error>{
    in_place,
    foo, bar, /* arguments to widget's constructor */
  };
}
```

### Returning references

It's not uncommon in APIs to want to return a reference to something that may
conditionally fail or be unavailable -- a simple example of this is container
`at(...)` functions which idiomatically throw exceptions on failure.

This is something that can also be modeled by this utility by using
`expected<T&,E>` -- where the `T` type is an lvalue-reference. In this case,
the value you return is an lvalue reference to an outside object. For example:

```cpp
template <typename T>
auto my_vector<T>::at(std::size_t n) noexcept -> expected<T&,my_error>
{
  if (n >= m_size) {
    return make_unexpected(my_error::out_of_range);
  }
  return m_storage[n]; // <- returns m_storage[n] by *reference*
}
```

Using an `expected<T&,E>` type is the same any `expected`, except it behaves
through indirection like a pointer. Assigning a value to an `expected<T&,E>`
will rebind the reference:

```cpp
auto a = int{};
auto b = int{};

expected<int&,int> x = a; // x refers to 'a'
x = b; // x now refers to 'b'
```

-------------------------------------------------------------------------------

Since `expected` supports conversion-constructors and assignments, it's also
possible to return `expected` references of types that behave polymorphically.
For example:

```cpp
struct base {};
struct derived : base {};

auto d = derived{};

// Holds onto a base-reference, refers to derived.
auto exp = expected<base&,int>{d};
```

### Returning `void`

It's not uncommon to want to represent functions that don't have any data worth
returning to the caller -- yet may also be, itself, fallible. This is not
uncommon for class member functions performing internal operations, such as
a `start()` or `stop()` function on a service. These cases may not have anything
useful to return to the caller other than a success or failure state.

Such a case is handled easily using the `expected<void,E>` specialization,
for example:

```cpp
class service
{
  auto start() noexcept -> expected<void,service_error>;
};
```

When returning a `void` value to an expected, you must return a
default-constructed `expected` object, since you _are_ returning a value; it's
just a no-op value:

```cpp
auto service::start() noexcept -> expected<void,service_error>
{
  ...
  return {}; // <- return's an expected<void,E> in success state
}
```

Although this may be represented as well in other ways, such as returning an
error-code, this does not identify the _semantic_ meaning the same way.
`expected<void,E>` provides a consistent way of marking-up functions that may
fail. More importantly, it also applies consistent semantics and helpful
utiliy functionality that returning a raw error type would not.

### Using assignment

An `expected<T,E>` object may be assigned to by other `expected` objects
provided that both `T` and `E` are assignable and construction would be
non-throwing. Additionally, the value or error types can be directly assigned to
via `T` or `unexpected<E>` types provided the respective constructors would be
non-throwing as well:

```cpp
auto exp = expected<unsigned,error_type> {42};

exp = 0xdeadbeef; // changes underlying value

exp = make_unexpected(error_type::some_error); // changes to error

exp = expected<unsigned short,error_type>{0}; // conversion-constructs

// etc
```

The non-throwing requirement is necessary since assigning `expected<T,E>`
objects may result in the active type changing. Changing the active type
requires destruction of the current type, which would otherwise leave this type
vulnerable to potentially being _stuck_ in this state if we allowed the new
type's constructor to throw. By forcing a requirement of `noexcept`, we ensure
that we can't be left in this valueless-by-exception state -- which helps us
ensure we can always guarantee containing _some_ value.

This does not mean that assignment will not be available at all if construction
isn't directly non-throwing. In fact, **as long as `T` and `E` have either a
non-throwing _move_ or _copy_ constructor, you can still perform assignment** --
it will just construct an intermediate `expected<T,E>` object first to ensure
that no object becomes valueless. If the intermediate object's construction
throws, the `expected` being assigned to is unchanged -- which is what we want.

For example, an `expected<std::string, int>` can still assign a `const char*`,
even though `std::string(const char*)` isn't `noexcept`:

```cpp
auto exp = expected<std::string,int>{"hello world"};

exp = "goodbyte world!";
```

The assignment will construct an intermediate `expected` object _first_ before
leveraging `std::string`'s non-throwing move-constructor.

In general, this restriction should have minimal impact on most workflows.
Very seldomly is it necessary to assign or change the result of an `expected`,
since the common case for this type is to be used as a `return` type from an
API. In the unlikely event that the assignment is even needed, we at least
can guarante that the `expected` remains coherent and always contains a value.

### Checking for error state

The current state of an `expected` object can be queried in a few different ways:

#### Checking `has_error()` or `has_value()`

To do a quick top-level query of whether the `expected` contains a value or an
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

An `expected`'s error type can be directly extracted using the `error()`
function. Doing this will return either a default constructed `E` error if the
`expected` contains a value, or it will extract a copy (or move) of the
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

You may use the `unexpected` type to compare directly with an underlying error
at any point to avoid the need to branch on `has_value()` explicitly.
For example:

```cpp
if (exp == make_unexpected(error::my_error)) {
  /* `exp` contains something equality-comparable to `error::my_error` */
}
```

#### Comparing with the underlying value directly

Alternatively, you may also compare `expected` objects directly with an
underlying value, if there is a particular state that is expected. This prevents
the need to check for discrete error types if the value is of more interest than
the error is.

```cpp
auto exp = try_to_uint8("255");

if (exp == 255) {
  /* exp contains a value equality-comparable to 255 */
}
```

## Advanced

### Monadic functions

The `expected` class offers monadic functionality to enable simple composability
and construction of return types. This allows functional chaining of calls to
produce simple and coherent expressions.

The supported monadic functionalities are:

* `value_or`: Gets the current value, or a supplied alternative
* `error_or`: Gets the current error, or a supplied alternative
* `and_then`: _If_ the `expected` contains a value, creates an `expected` with
  the supplied value. Otherwise creates the `expected` contain the error
* `map`: Executes a function on the current value (if any) and returns an
  `expected` containing the result. If it contains an error, this returns an
  `expected` containing that error.
* `flat_map`: Similar to `map`, except it assumes that the function itself
  returns an `expected` object
* `map_error`: Similar to `map`, except it operates on the error rather than the
  value

For example:

```cpp
// (1) 'value_or'
auto value = exp.value_or(42);
// gets the current value _or_ 42

// (2) 'error_or'
auto error = exp.error_or(error_code::some_distinct_error);
// Gets the stored error, or the supplied error

// (3) `and_then`
auto next_exp = exp.and_then("hello world");
// if 'exp' contains a value, creates an expected with "hello world";
// otherwise creates an error

// (4) `map`
auto next_exp = exp.map(to_string);
// Calls 'to_string' on the stored expected value, creating an
// 'expected<string,E>' on return

// (5) `flat_map`
auto next_exp = exp.map(to_uint);
// Tries to convert the stored value to an integral value, which may itself
// return an expected value

// (6) `map_error`
auto consumer_exp = internal_exp.map_error(to_external_error);
// Calls 'to_external_error' to convert an internal error code to an external
// (consumer-facing) error-code
```

A practical example of this composition is chaining a conversion of a
`string` into an integral value, mapping that value to an enum and converting
the parse-error to a user-facing error:

```cpp
auto try_to_uint8(const std::string&) noexcept -> expected<std::uint8_t,parse_error>;
auto to_client_code(std::uint8_t) noexcept -> client_code;
auto to_user_error(parse_error) noexcept -> user_error;

// Example composition:

auto result = try_to_uint8(str).map(to_client_code).map_error(to_user_error);
```

### Type-erasure with `expected<void,E>`

The `expected<void,E>` specialization is constructible from any `expected<T,E2>`
type as long as `E2` is convertible to `E`. This allows for a very simple and
effective form of type-erasure in composition if the error is important but the
result can be discarded.

For example, it's easy to create compositions that simply discard the T result:

```cpp
template <typename Fn>
auto try_invoke(Fn&& fn) noexcept -> void
{
  // Coalesce all results to 'void'
  expected<void,E> result = std::invoke(std::forward<Fn>(fn));
  if (!result) {
    // Do something with the error
    signal_service::notify_error(result.error());
  }
}
```

This erasure also works in terms of other utilities as well. In the same way
that `std::function` can erase any `R` return type to `void`, the implicit
conversions also allow for any `expected<R,E>` return type to
`expected<void,E>`:

```cpp

auto register_handler(std::function<expected<void,E>()> fn) -> void;

/* ... */

auto int_handler() -> expected<int,E>;
auto string_handler() -> expected<std::string,E>;

/* ... */

register_handler(&int_handler);
register_handler(&string_handler);
```

### `unexpected` with references

For the common-case, `expected`'s error-types are generally intended to be
lightweight and inexpensive to construct/copy/move; however, there will always
be exceptions to the rule. In some cases, you may already have an instance of an
expensive `E` error type to compare against, and can't afford to copy it for
an `unexpected` object.

In such cases, you can construct `unexpected` objects that refer to the local
instance without paying for the cost of a copy. To do this, you can use either
`std::ref` with `make_unexpected` (or `unexpected` with CTAD in <kbd>C++17</kbd>)

For example:

```cpp
const auto some_error_object = /* some expensive object */

if (exp == make_unexpected(std::ref(some_error_object))) {
  ...
}
```
