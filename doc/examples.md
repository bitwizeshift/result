# Examples

## Chaining Member Functions

The various monadic functions in `expected` are made to work with any invocable
expression. As a result, it's really simple to try to call
a possibly failing `T` call and transform it into a `U`:

```cpp
enum class some_error : int;
auto try_get_string() -> expect::expected<std::string,some_error>;

// ...

auto exp = try_get_string().map(&std::string::size);
```

[<kbd>Live example</kbd>](https://gcc.godbolt.org/z/onen18)
