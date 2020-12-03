# Examples

## Chaining Member Functions

The various monadic functions in `expected` are made to work with any invocable
expression. Since member pointers are valid for the purposes of invoke
expressions, this allows for a really convenient way to chain functions on an
`expected` result while also propagating the potential error.

For example:

```cpp
enum class some_error : int;
auto try_get_string() -> expect::expected<std::string,some_error>;

// ...

auto exp = try_get_string().map(&std::string::size);
```

<kbd>[Live example](https://gcc.godbolt.org/z/onen18)</kbd>
