#include "expected.hpp"
#include <iostream>

auto get_return_value() noexcept -> expect::expected<int>
{
  return 0;
}

int main()
{
  return get_return_value().value();
}
