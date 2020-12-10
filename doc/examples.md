# Examples

## Chaining Member Functions

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
