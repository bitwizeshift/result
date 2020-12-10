### FAQ

#### Why is `result<void,E>` valid?

There are many cases that may be useful to express fallibility on an API that
does not, itself, have a value to return. A simple example of such a case would
be a `Service` class' `start()` or `stop()` functionalities that rely on
possibly fallible APIs.

Although one option would be to simply return `error_code`, this would now
result in inverted semantics -- where `if (result)` means an error has occurred,
rather than the result was successful. Additionally, having `result<void,E>`
support enables better metaprogramming composition, where a function template
may arbitrarily call a fallible function without needing to branch on the
type result.

#### Why does assignment require `noexcept` constructors?

The `result` type is meant to uphold an invariant of _always containing state_.
Allowing for potentially-throwing constructors introduces the possibility of
achieving a "valueless-by-exception" state, similar to that of `std::variant`,
if the `result` being assigned to contains a different active state than the
argument. This can occur since the previous value's destructor needs to be
invoked before the new constructor that throws is invoked.

It's not ideal to expose a "valueless-by-exception" state in a class that
intends to always carry a valid state -- and so it's easier to remove this
possibility.

`result` offers an alternative method of error handling that **contrasts**
exceptions. Generally if you're using this pattern already, there is a pretty
good chance that your code will avoid throwing wherever possible.

#### Why is `result::emplace` absent?

`result` is _not_ meant as a generic `either<T, U>` class. It's meant
specifically for simple, semantic error-handling. The common pattern for this
is to `return` these types, but very rarely should a need arise to change the
active type.

In the few times where this may be relevant, the risk occurs that a
"valueless-by-exception" state may occur if the active type is changing.

Since this isn't the intended use of `result`, these were omitted in favor of
explicitly using assignment operators that require `noexcept` construction
(see the above answer on assignment operators for more detail).

#### Why is `result<T&,E>` supported?

The `result` type is meant to propogate and support any case that a typical
API supports, whether it be returning a value, returning `void`, or -- in this
case -- returning references.

The only alternatives to allow _failing_ to return an indirect type like this
would be to use either:

* `result<std::reference_wrapper<T>,E>`, which is verbose, or
* `result<T*, E>`, which now requires the caller to check both the error-case
  along with the `nullptr` case. This would also prevent the ability to use
  `operator->` or `operator*` shorthands -- since the `value_type` is `T*`.

The decision was made to make this as simple as possible so that practical
code can be authored with minimal effort, such as:

```cpp
auto nothrow_at(const std::array<T,N>& arr, std::size_t n)
  noexcept -> cpp::result<T&,my_error>
{
  if (n >= N) {
    return cpp::fail(my_error::out_of_range);
  }
  return arr[n];
}
```

#### Why does `failure` allow reference error-types, but `result` does not?

The `result` API will always either return the stored error object, or a
default-constructed one if the `result` does not contain an error. Since
references are not default-constructible, this does not work as an error type
for `result`.

The reason references were added for `failure` in the first place is to
support lightweight disambiguation checks of:

```cpp
auto exp = /* some result */
auto foo = /* some error value to compare against */

if (exp == cpp::fail(foo)) { ... }
```

In the case where the error type is not cheap to construct, it would be
undesirable to require a temporary to be materialized for this purpose. This is
why references are supported, which allow for comparisons to be lighter-weight
if needed

```cpp
if (exp == cpp::fail(std::ref(foo))) { ... }
```

This ensures that `exp` can still be compared to `foo`, only it does so through
a reference which effectively becomes 0-overhead after optimizing.
