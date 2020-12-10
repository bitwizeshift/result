#include "result.hpp"
#include <iostream>

auto get_return_value() noexcept -> cpp::result<int, int>
{
  return 0;
}

int main()
{
  return get_return_value().value();
}
