# Deviations from `std::expected` Proposal

Although this library is heavily based on the `std::expected` proposal, it's
important to be aware that this handles a few things differently from what is
described in `P0323`. These deviations are outline below

1. This does not implement `emplace` functions, since this functionality is
   seen as orthogonal to the goals of `expected`, which should not be to
   arbitrarily be an `either<T,E>` type

2. `expected` has been given support for reference `T` types, so that this may
   behave as a consistent vocabulary type for error-handling

3. `unexpected` has been given support for reference `E` types, in order to
   avoid expensive construction of objects only used for comparisons

4. Assignment operators are only enabled if the corresponding constructors are
   marked `noexcept`. This deviates from `std::expected`'s proposal of
   introducing an intermediate object to hold the type during assignment.

5. Rather than allowing direct referential access to the underlying error,
   `expected::error()` _always_ returns a value that acts as the result's
   status. The defaut-constructed state of `E` is always considered the "good"
   state (e.g. `std::error_code{}`, `std::errc{}`, etc). This reduces the
   number of cases where this API may throw an exception to just `value()`

6. Rather than using `unexpect_t` to denote in-place construction of errors,
   this library uses `in_place_error_t`. This is done to prevent having 3 types
   that all sound similar (`expected`, `unexpected`, `unexpect_t`)
