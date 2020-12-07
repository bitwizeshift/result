/*
  The MIT License (MIT)

  Copyright (c) 2020 Matthew Rodusek All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "expected.hpp"

#include <catch2/catch.hpp>

#include <utility> // std::reference_wrapper
#include <type_traits>

namespace cpp {
namespace test {
namespace {

struct not_nothrow_default_constructible
{
  not_nothrow_default_constructible() noexcept(false){};
};

struct not_trivial_default_constructible
{
  not_trivial_default_constructible(){}
};

struct not_default_constructible
{
  not_default_constructible() = delete;
};

struct not_copy_or_moveable{
  not_copy_or_moveable() = default;
  not_copy_or_moveable(const not_copy_or_moveable&) = delete;
  not_copy_or_moveable(not_copy_or_moveable&&) = delete;
};

struct default_construct_test
{
  static constexpr auto default_value = 42;

  default_construct_test() : value{default_value}{}

  int value;
};
constexpr decltype(default_construct_test::default_value) default_construct_test::default_value;

} // namespace <anonymous>

//=============================================================================
// class : unexpected<E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("unexpected<E>::unexpected()", "[ctor]") {
  SECTION("T is default constructible") {
    SECTION("Unexpected is also default constructible") {
      using sut_type = unexpected<int>;

      STATIC_REQUIRE(std::is_default_constructible<sut_type>::value);
    }

    SECTION("T is nothrow default constructible") {
      SECTION("Unexpected propagates noexcept status") {
        using sut_type = unexpected<int>;

        STATIC_REQUIRE(std::is_nothrow_default_constructible<sut_type>::value);
      }
    }

    SECTION("T is not nothrow default constructible") {
      SECTION("Unexpected propagates noexcept status") {
        using sut_type = unexpected<not_nothrow_default_constructible>;

        STATIC_REQUIRE_FALSE(std::is_nothrow_default_constructible<sut_type>::value);
      }
    }

    SECTION("T is trivial default constructible") {
      SECTION("Unexpected propagates trivial constructor") {
        using sut_type = unexpected<int>;

        STATIC_REQUIRE(std::is_trivially_default_constructible<sut_type>::value);
      }
    }

    SECTION("T is not trivial default constructible") {
      SECTION("Unexpected propagates trivial constructor") {
        using sut_type = unexpected<not_trivial_default_constructible>;

        STATIC_REQUIRE_FALSE(std::is_trivially_default_constructible<sut_type>::value);
        STATIC_REQUIRE(std::is_default_constructible<sut_type>::value);
      }
    }

    SECTION("Default constructs underlying type") {
      auto sut = unexpected<default_construct_test>{};

      REQUIRE(sut.error().value == default_construct_test::default_value);
    }
  }

  SECTION("T is not default constructible") {
    SECTION("Unexpected is also not default constructible") {
      using sut_type = unexpected<not_default_constructible>;

      STATIC_REQUIRE_FALSE(std::is_default_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("unexpected<E>::unexpected(const element_type&)", "[ctor]") {
  SECTION("T is copy-constructible") {
    SECTION("Unexpected is also copy-constructible") {
      using underlying = int;
      using sut_type = unexpected<underlying>;

      STATIC_REQUIRE(std::is_constructible<sut_type,const underlying&>::value);
    }
  }
  SECTION("T is not copy-constructible") {
    SECTION("Unexpected is also not copy-constructible") {
      using underlying = not_copy_or_moveable;
      using sut_type = unexpected<underlying>;

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type,const underlying&>::value);
    }
  }
  SECTION("Copies the contents of the other unexpected") {
    const auto sut = unexpected<int>{42};

    const auto copy = sut;

    REQUIRE(copy == sut);
  }
}

TEST_CASE("unexpected<E>::unexpected(element_type&&)", "[ctor]") {
  SECTION("T is move-constructible") {

  }
  SECTION("T is not move-constructible") {
    SECTION("Unexpected is also not move-constructible") {
      using underlying = not_copy_or_moveable;
      using sut_type = unexpected<underlying>;
      // Note: If a type is copy-constructible but is not move-constructible,
      //       the copy-constructor will be enabled -- but we have no way of
      //       properly detecting this statically

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type,underlying&&>::value);
    }
  }
}

TEST_CASE("unexpected<E>::unexpected(element_type&)", "[ctor]") {
  SECTION("Is not defined for values") {
    using underlying = not_copy_or_moveable;
    using sut_type = unexpected<underlying>;

    // Note: If the error is copy-constructible, the `const element_type&` overload
    //       will be enabled instead
    STATIC_REQUIRE_FALSE(std::is_constructible<sut_type,underlying&>::value);
  }
}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

TEST_CASE("unexpected<E>::error() &", "[accessor]") {
  const auto expected = 42;
  auto sut = unexpected<int>(expected);

  SECTION("Returns a reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(sut.error()),int&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(sut.error() == expected);
  }
}

TEST_CASE("unexpected<E>::error() &&", "[accessor]") {
  const auto expected = 42;
  auto sut = unexpected<int>(expected);

  SECTION("Returns a reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(std::move(sut).error()),int&&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(std::move(sut).error() == expected);
  }
}

TEST_CASE("unexpected<E>::error() const &", "[accessor]") {
  const auto expected = 42;
  const auto sut = unexpected<int>(expected);

  SECTION("Returns a reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(sut.error()),const int&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(sut.error() == expected);
  }
}


TEST_CASE("unexpected<E>::error() const &&", "[accessor]") {
  const auto expected = 42;
  const auto sut = unexpected<int>(expected);

  SECTION("Returns a reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(std::move(sut).error()),const int&&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(std::move(sut).error() == expected);
  }
}

//=============================================================================
// class : unexpected<E&>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("unexpected<E&>::unexpected()", "[ctor]") {
  SECTION("Is not defined for references") {
    STATIC_REQUIRE_FALSE(std::is_default_constructible<unexpected<int&>>::value);
  }
}

TEST_CASE("unexpected<E&>::unexpected(const element_type&)", "[ctor]") {
  SECTION("Is not defined for references") {
    STATIC_REQUIRE_FALSE(std::is_constructible<unexpected<int&>,const int&>::value);
  }
}

TEST_CASE("unexpected<E&>::unexpected(element_type&&)", "[ctor]") {
  SECTION("Is not defined for references") {
    STATIC_REQUIRE_FALSE(std::is_constructible<unexpected<int&>,int&&>::value);
  }
}

TEST_CASE("unexpected<E&>::unexpected(element_type&)", "[ctor]") {
  SECTION("Is defined for references") {
    STATIC_REQUIRE(std::is_constructible<unexpected<int&>,int&>::value);
  }
  SECTION("Binds a reference to the unexpected value") {
    auto value = 42;
    auto sut = unexpected<int&>(value);

    SECTION("References underlying reference") {
      REQUIRE(&value == &sut.error());
    }
  }
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

TEST_CASE("unexpected<E&>::error() &", "[accessor]") {
  auto expected = 42;
  auto sut = unexpected<int&>(expected);

  SECTION("Returns a reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(sut.error()),int&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(&sut.error() == &expected);
  }
}

TEST_CASE("unexpected<E&>::error() &&", "[accessor]") {
  auto expected = 42;
  auto sut = unexpected<int&>(expected);

  SECTION("Returns an lvalue reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(std::move(sut).error()),int&>::value);
  }
  SECTION("Is equal to the input value") {
    int& x = std::move(sut).error();
    REQUIRE(&x == &expected);
  }
}

TEST_CASE("unexpected<E&>::error() const &", "[accessor]") {
  auto expected = 42;
  const auto sut = unexpected<int&>(expected);

  SECTION("Returns an lvalue reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(sut.error()),int&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(&sut.error() == &expected);
  }
}

TEST_CASE("unexpected<E&>::error() const &&", "[accessor]") {
  auto expected = 42;
  const auto sut = unexpected<int&>(expected);

  SECTION("Returns an lvalue reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(std::move(sut).error()),int&>::value);
  }
  SECTION("Is equal to the input value") {
    int& x = std::move(sut).error();
    REQUIRE(&x == &expected);
  }
}

//=============================================================================
// non-member functions : class : unexpected<E&>
//=============================================================================

//-----------------------------------------------------------------------------
// Comparison
//-----------------------------------------------------------------------------

TEST_CASE("operator==(const unexpected<E1>&, const unexpected<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = unexpected<int>(value);
  const auto rhs = make_unexpected(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs == rhs);
  }
}

TEST_CASE("operator!=(const unexpected<E1>&, const unexpected<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = unexpected<int>(0);
  const auto rhs = make_unexpected(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs != rhs);
  }
}

TEST_CASE("operator>=(const unexpected<E1>&, const unexpected<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = unexpected<int>(100);
  const auto rhs = make_unexpected(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs >= rhs);
  }
}

TEST_CASE("operator<=(const unexpected<E1>&, const unexpected<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = unexpected<int>(0);
  const auto rhs = make_unexpected(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs <= rhs);
  }
}

TEST_CASE("operator<(const unexpected<E1>&, const unexpected<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = unexpected<int>(0);
  const auto rhs = make_unexpected(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs < rhs);
  }
}

TEST_CASE("operator>(const unexpected<E1>&, const unexpected<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = unexpected<int>(100);
  const auto rhs = make_unexpected(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs > rhs);
  }
}

//-----------------------------------------------------------------------------
// Utilities
//-----------------------------------------------------------------------------

TEST_CASE("make_unexpected(E&& e)", "[utilities]") {
  using error_type = std::string;
  const auto input = error_type{"Hello world"};

  auto sut = make_unexpected(input);

  SECTION("Deduces a decayed input") {
    STATIC_REQUIRE(std::is_same<decltype(sut),unexpected<error_type>>::value);
  }
  SECTION("Contains same value as input") {
    REQUIRE(sut.error() == input);
  }
}

TEST_CASE("make_unexpected<E>(Args&&...)", "[utilities]") {
  SECTION("Invokes E's constructor") {
    const auto input = "hello world";
    const auto size = 5;
    const auto expected = std::string{input, size};
    const auto sut = make_unexpected<std::string>(input, size);

    REQUIRE(sut.error() == expected);
  }
}

TEST_CASE("make_unexpected(std::reference_wrapper<E>)", "[utilities]") {
  using error_type = std::string;

  auto error = error_type{};
  auto sut = make_unexpected(std::ref(error));

  SECTION("Deduces unexpected<E&>") {
    STATIC_REQUIRE(std::is_same<decltype(sut),unexpected<error_type&>>::value);
  }
  SECTION("Refers to input") {
    REQUIRE(&sut.error() == &error);
  }
}

} // namespace test
} // namespace cpp
