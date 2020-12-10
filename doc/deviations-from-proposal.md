# Deviations from `std::expected` Proposal

Although this library is heavily based on the `std::expected` proposal, it's
important to be aware that this handles a few things differently from what is
described in `P0323`. These deviations are outline below

1. Most obviously, this type is named `result`, rather than `expected`, since
   `expected<T,E>`'s name conveys that `E` is an expected result, when in
   actuality it is _unexpected_. The `result<T,E>` is more idiomatic in
   other modern languages like Swift and Rust, and carries no such implication.
   To account for the name change, `unexpected<E>` is also named `failure<E>`,
   and a helper factory function `fail` exists better code readability.

2. This does not implement `emplace` functions, since this functionality is
   seen as orthogonal to the goals of `expected`, which should not be to
   arbitrarily be an `either<T,E>` type

3. `expected` (`result`) has been given support for reference `T` types, so that
   this may behave as a consistent vocabulary type for error-handling

4. `unexpected` (`failure`) has been given support for reference `E` types, in
   order to avoid expensive construction of objects only used for comparisons

5. Assignment operators are only enabled if the corresponding constructors are
   marked `noexcept`. This deviates from `std::expected`'s proposal of
   introducing an intermediate object to hold the type during assignment.

6. Rather than allowing direct referential access to the underlying error,
   `result::error()` _always_ returns a value that acts as the result's
   status. The defaut-constructed state of `E` is always considered the "good"
   state (e.g. `std::error_code{}`, `std::errc{}`, etc). This reduces the
   number of cases where this API may throw an exception to just `value()`

7. Rather than using `unexpect_t` to denote in-place construction of errors,
   this library uses `in_place_error_t`. This change was necessary with the
   rename of `expected` -> `result`.
