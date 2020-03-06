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
