### FAQ

#### Why is `expected<void,E>` valid?

There are many cases that may be useful to express fallibility on an API that
does not, itself, have a value to return. A simple example of such a case would
be a `Service` class' `start()` or `stop()` functionalities that rely on
possibly fallible APIs.

Although one option would be to simply return `error_code`, this would now
result in inverted semantics -- where `if (result)` means an error has occurred,
rather than the result was successful. Additionally, having `expected<void,E>`
support enables better metaprogramming composition, where a function template
may arbitrarily call a fallible function without needing to branch on the
type result.

#### Why is `expected::flat_map_error` absent?

`expected::flat_map` exists to allow for monadic chaining of fallible functions,
piping the input of one possibly-failing result into another that may fail based
on that input.

`expected::map_error` was added to allow converting internal error-code types to
external-facing types, so that users may simply map public and private error
codes in a simple way. An equivalent `flat_map_error` does not have as much
utility, since mapping of error codes should not have any coherent reason for
it to fail in a way that benefits returning an expected.

Whereas

```cpp
expected<int,E> from_string(int)
```

makes sense, an

```cpp
expected<int,E2> from_error(E1)
```

doesn't make sense -- and was thus not included as part of the monadic API.

#### Why does assignment require `noexcept` constructors?

The `expected` type is meant to uphold an invariant of _always containing state_.
Allowing for potentially-throwing constructors introduces the possibility of
achieving a "valueless-by-exception" state, similar to that of `std::variant`,
if the `expected` being assigned to contains a different active state than the
argument. This can occur since the previous value's destructor needs to be
invoked before the new constructor that throws is invoked.

It's not ideal to expose a "valueless-by-exception" state in a class that
intends to always carry a valid state -- and so it's easier to remove this
possibility.

`expected` offers an alternative method of error handling that **contrasts**
exceptions. Generally if you're using this pattern already, there is a pretty
good chance that your code will avoid throwing wherever possible.

#### Why is `expected::emplace` absent?

`expected` is _not_ meant as a generic `either<T, U>` class. It's meant
specifically for simple, semantic error-handling. The common pattern for this
is to `return` these types, but very rarely should a need arise to change the
active type.

In the few times where this may be relevant, the risk occurs that a
"valueless-by-exception" state may occur if the active type is changing.

Since this isn't the intended use of `expected`, these were omitted in favor of
explicitly using assignment operators that require `noexcept` construction
(see the above answer on assignment operators for more detail).

#### Why does `unexpected` allow reference error-types, but `expected` does not?

The `expected` API will always either return the stored error object, or a
default-constructed one if the `expected` does not contain an error. Since
references are not default-constructible, this does not work as an error type
for `expected`.

The reason references were added for `unexpected` in the first place is to
support lightweight disambiguation checks of:

```cpp
auto exp = /* some expected */
auto foo = /* some error value to compare against */

if (exp == unexpected(foo)) { ... }
```

In the case where the error type is not cheap to construct, it would be
undesirable to require a temporary to be materialized for this purpose. This is
why references are supported, which allow for comparisons to be lighter-weight
if needed

```cpp
if (exp == unexpected(std::ref(foo))) { ... }
```

This ensures that `exp` can still be compared to `foo`, only it does so through
a reference which effectively becomes 0-overhead after optimizing.
