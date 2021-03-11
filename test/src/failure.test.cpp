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

#include "result.hpp"

#include <catch2/catch.hpp>

#include <utility> // std::reference_wrapper
#include <type_traits>

namespace cpp {
namespace test {
namespace {

struct not_nothrow_default_constructible
{
  not_nothrow_default_constructible() noexcept(false){}
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

template <typename T>
struct explicit_type : public T
{
  template <typename...Args,
            typename = typename std::enable_if<std::is_constructible<T,Args...>::value>::type>
  explicit explicit_type(Args&&...args)
    : T(std::forward<Args>(args)...)
  {
  }

  explicit explicit_type() = default;
  explicit explicit_type(const explicit_type&) = default;
  explicit explicit_type(explicit_type&&) = default;

  auto operator=(const explicit_type&) -> explicit_type& = default;
  auto operator=(explicit_type&&) -> explicit_type& = default;
};

template <typename T>
struct move_only : public T
{
  using T::T;

  move_only() = default;

  move_only(T&& other) : T(std::move(other)){}
  move_only(move_only&&) = default;
  move_only(const move_only&) = delete;

  auto operator=(move_only&&) -> move_only& = default;
  auto operator=(const move_only&) -> move_only& = delete;
};

} // namespace <anonymous>

//=============================================================================
// class : failure<E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("failure<E>::failure()", "[ctor]") {
  SECTION("T is default constructible") {
    SECTION("Unexpected is also default constructible") {
      using sut_type = failure<int>;

      STATIC_REQUIRE(std::is_default_constructible<sut_type>::value);
    }

    SECTION("T is nothrow default constructible") {
      SECTION("Unexpected propagates noexcept status") {
        using sut_type = failure<int>;

        STATIC_REQUIRE(std::is_nothrow_default_constructible<sut_type>::value);
      }
    }

    SECTION("T is not nothrow default constructible") {
      SECTION("Unexpected propagates noexcept status") {
        using sut_type = failure<not_nothrow_default_constructible>;

        STATIC_REQUIRE_FALSE(std::is_nothrow_default_constructible<sut_type>::value);
      }
    }

    SECTION("T is trivial default constructible") {
      SECTION("Unexpected propagates trivial constructor") {
        using sut_type = failure<int>;

        STATIC_REQUIRE(std::is_trivially_default_constructible<sut_type>::value);
      }
    }

    SECTION("T is not trivial default constructible") {
      SECTION("Unexpected propagates trivial constructor") {
        using sut_type = failure<not_trivial_default_constructible>;

        STATIC_REQUIRE_FALSE(std::is_trivially_default_constructible<sut_type>::value);
        STATIC_REQUIRE(std::is_default_constructible<sut_type>::value);
      }
    }

    SECTION("Default constructs underlying type") {
      auto sut = failure<default_construct_test>{};

      REQUIRE(sut.error().value == default_construct_test::default_value);
    }
  }

  SECTION("T is not default constructible") {
    SECTION("Unexpected is also not default constructible") {
      using sut_type = failure<not_default_constructible>;

      STATIC_REQUIRE_FALSE(std::is_default_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("failure<E>::failure(const element_type&)", "[ctor]") {
  SECTION("T is copy-constructible") {
    SECTION("Unexpected is also copy-constructible") {
      using underlying = int;
      using sut_type = failure<underlying>;

      STATIC_REQUIRE(std::is_constructible<sut_type,const underlying&>::value);
    }
  }
  SECTION("T is not copy-constructible") {
    SECTION("Unexpected is also not copy-constructible") {
      using underlying = not_copy_or_moveable;
      using sut_type = failure<underlying>;

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type,const underlying&>::value);
    }
  }
  SECTION("Copies the contents of the other failure") {
    const auto sut = failure<int>{42};

    const auto copy = sut;

    REQUIRE(copy == sut);
  }
}

TEST_CASE("failure<E>::failure(element_type&&)", "[ctor]") {
  SECTION("T is move-constructible") {
    SECTION("Unexpected is also move-constructible") {
      using underlying = std::string;
      using sut_type = failure<underlying>;

      STATIC_REQUIRE(std::is_constructible<sut_type,underlying&&>::value);
    }
  }
  SECTION("T is not move-constructible") {
    SECTION("Unexpected is also not move-constructible") {
      using underlying = not_copy_or_moveable;
      using sut_type = failure<underlying>;
      // Note: If a type is copy-constructible but is not move-constructible,
      //       the copy-constructor will be enabled -- but we have no way of
      //       properly detecting this statically

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type,underlying&&>::value);
    }
  }
}

TEST_CASE("failure<E>::failure(in_place_t, Args&&...)", "[ctor]") {
  SECTION("Constructs E with args") {
    const auto string = "Hello World";
    const auto sut = failure<std::string>{in_place, string, 5};
    const auto expected = std::string{string, 5};

    REQUIRE(sut.error() == expected);
  }
}

TEST_CASE("failure<E>::failure(in_place_t, std::initializer_list<U>, Args&&...)", "[ctor]") {
  SECTION("Constructs E with args") {
    const auto sut = failure<std::string>{
      in_place,
      {'H','e','l','l','o'},
      std::allocator<char>{}
    };
    const auto expected = std::string{
      {'H','e','l','l','o'},
      std::allocator<char>{}
    };

    REQUIRE(sut.error() == expected);
  }
}

TEST_CASE("failure<E>::failure(U&&)", "[ctor]") {
  SECTION("U is convertible to E") {
    SECTION("U is convertible to failure<E>") {
      using source_type = const char*;
      using sut_type = failure<std::string>;

      STATIC_REQUIRE(std::is_convertible<const source_type&,sut_type>::value);
    }
    SECTION("failure<E> is constructible from U") {
      using source_type = const char*;
      using sut_type = failure<std::string>;

      STATIC_REQUIRE(std::is_constructible<sut_type,const source_type&>::value);
    }
    SECTION("Constructs the underlying type") {
      const auto underlying = "Hello World";
      const failure<std::string> sut = underlying;

      const std::string expected = underlying;

      REQUIRE(sut.error() == expected);
    }
  }
  SECTION("U is not convertible to E") {
    SECTION("U is not convertible to failure<E>") {
      using source_type = int;
      using sut_type = failure<std::string>;

      STATIC_REQUIRE_FALSE(std::is_convertible<const source_type&,sut_type>::value);
    }
  }
}

TEST_CASE("failure<E>::failure(U&&) (explicit)", "[ctor]") {
  SECTION("E is constructible from U") {
    SECTION("failure<E> is constructible from U") {
      using source_type = const char*;
      using sut_type = failure<explicit_type<std::string>>;

      STATIC_REQUIRE(std::is_constructible<sut_type,const source_type&>::value);
    }
    SECTION("U is not convertible to failure<E>") {
      using source_type = const char*;
      using sut_type = failure<explicit_type<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_convertible<const source_type&,sut_type>::value);
    }
    SECTION("Constructs the underlying type") {
      const auto underlying = "Hello World";
      const auto sut = failure<explicit_type<std::string>>{underlying};

      const explicit_type<std::string> expected{underlying};

      REQUIRE(sut.error() == expected);
    }
  }
  SECTION("E is not constructible from U") {
    SECTION("failure<E> is not constructible from U") {
      using source_type = int;
      using sut_type = failure<explicit_type<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type,const source_type&>::value);
    }
  }
}

TEST_CASE("failure<E>::failure(const failure<E2>&) ", "[ctor]") {
  SECTION("E2 is copy-convertible to E") {
    SECTION("failure<E> is copy-convertible from failure<E2>") {
      using source_type = failure<const char*>;
      using sut_type = failure<std::string>;

      STATIC_REQUIRE(std::is_constructible<sut_type,const source_type&>::value);
    }
    SECTION("Copy-converts the underlying value") {
      const auto source = fail("Hello World");
      const auto sut = failure<std::string>{source};

      REQUIRE(sut == source);
    }
  }
  SECTION("E2 is not copy-convertible to E") {
    SECTION("failure<E> is not copy-convertible from failure<E2>") {
      using source_type = failure<const char*>;
      using sut_type = failure<int>;

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type,const source_type&>::value);
    }
  }
}

TEST_CASE("failure<E>::failure(failure<E2>&&)", "[ctor]") {
  SECTION("E2 is move-convertible to E") {
    SECTION("failure<E> is move-convertible from failure<E2>") {
      using source_type = failure<std::string>;
      using sut_type = failure<move_only<std::string>>;

      STATIC_REQUIRE(std::is_constructible<sut_type,source_type&&>::value);
    }
    SECTION("Move-converts the underlying value") {
      auto source = fail(std::string{"Hello World"});
      const auto expected = source;
      const auto sut = failure<move_only<std::string>>{std::move(source)};

      REQUIRE(sut == expected);
    }
  }
  SECTION("E2 is not move-convertible to E") {
    SECTION("failure<E> is not move-convertible from failure<E2>") {
      using source_type = failure<int>;
      using sut_type = failure<move_only<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type,source_type&&>::value);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("failure<E>::operator=(E2&&) ", "[assign]") {
  SECTION("E2 is assignable to E") {
    SECTION("E2 is assignable to failure<E>") {
      using source_type = const char*;
      using sut_type = failure<std::string>;

      STATIC_REQUIRE(std::is_assignable<sut_type,source_type>::value);
    }
    SECTION("Assigns the underlying value") {
      const auto expected = "Goodbye world";
      auto sut = fail<std::string>("Hello World");

      sut = expected;

      REQUIRE(sut.error() == expected);
    }
  }
  SECTION("E2 is not assignable to E") {
    SECTION("E2 is not assignable to failure<E>") {
      using source_type = const char*;
      using sut_type = failure<int>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,source_type>::value);
    }
  }
}

TEST_CASE("failure<E>::operator=(const failure<E2>&) ", "[assign]") {
  SECTION("const E2 is assignable to E") {
    SECTION("const failure<E2> is assignable to failure<E>") {
      using source_type = failure<const char*>;
      using sut_type = failure<std::string>;

      STATIC_REQUIRE(std::is_assignable<sut_type,const source_type&>::value);
    }
    SECTION("Copy-converts the underlying value") {
      const auto expected = fail("Goodbye World");
      auto sut = fail<std::string>("Hello world");

      sut = expected;

      REQUIRE(sut == expected);
    }
  }
  SECTION("const E2 is not assignable to E") {
    SECTION("const failure<E2> is not assignable to failure<E>") {
      using source_type = failure<const char*>;
      using sut_type = failure<int>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const source_type&>::value);
    }
  }
}

TEST_CASE("failure<E>::operator=(failure<E2>&&)", "[assign]") {
  SECTION("E2&& is assignable to E") {
    SECTION("failure<E2>&& is assignable to failure<E>") {
      using source_type = failure<std::string>;
      using sut_type = failure<move_only<std::string>>;

      STATIC_REQUIRE(std::is_assignable<sut_type,source_type&&>::value);
    }
    SECTION("Assigns the underlying value") {
      auto source = fail(std::string{"Goodbe World"});
      const auto expected = source;

      auto sut = failure<move_only<std::string>>{"Hello World"};

      sut = std::move(source);

      REQUIRE(sut == expected);
    }
  }
  SECTION("E2&& is not assignable to E") {
    SECTION("failure<E2>&& is not assignable to failure<E>") {
      using source_type = failure<int>;
      using sut_type = failure<move_only<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,source_type&&>::value);
    }
  }
}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

TEST_CASE("failure<E>::error() &", "[accessor]") {
  const auto expected = 42;
  auto sut = failure<int>(expected);

  SECTION("Returns a reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(sut.error()),int&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(sut.error() == expected);
  }
}

TEST_CASE("failure<E>::error() &&", "[accessor]") {
  const auto expected = 42;
  auto sut = failure<int>(expected);

  SECTION("Returns a reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(std::move(sut).error()),int&&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(std::move(sut).error() == expected);
  }
}

TEST_CASE("failure<E>::error() const &", "[accessor]") {
  const auto expected = 42;
  const auto sut = failure<int>(expected);

  SECTION("Returns a reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(sut.error()),const int&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(sut.error() == expected);
  }
}


TEST_CASE("failure<E>::error() const &&", "[accessor]") {
  const auto expected = 42;
  const auto sut = failure<int>(expected);

  SECTION("Returns a reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(std::move(sut).error()),const int&&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(std::move(sut).error() == expected);
  }
}

//=============================================================================
// class : failure<E&>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("failure<E&>::failure()", "[ctor]") {
  SECTION("Is not defined for references") {
    STATIC_REQUIRE_FALSE(std::is_default_constructible<failure<int&>>::value);
  }
}

TEST_CASE("failure<E&>::failure(const element_type&)", "[ctor]") {
  SECTION("Is not defined for references") {
    STATIC_REQUIRE_FALSE(std::is_constructible<failure<int&>,const int&>::value);
  }
}

TEST_CASE("failure<E&>::failure(element_type&&)", "[ctor]") {
  SECTION("Is not defined for references") {
    STATIC_REQUIRE_FALSE(std::is_constructible<failure<int&>,int&&>::value);
  }
}

TEST_CASE("failure<E&>::failure(element_type&)", "[ctor]") {
  SECTION("Is defined for references") {
    STATIC_REQUIRE(std::is_constructible<failure<int&>,int&>::value);
  }
  SECTION("Binds a reference to the failure value") {
    auto value = 42;
    auto sut = failure<int&>(value);

    SECTION("References underlying reference") {
      REQUIRE(&value == &sut.error());
    }
  }
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

TEST_CASE("failure<E&>::error() &", "[accessor]") {
  auto expected = 42;
  auto sut = failure<int&>(expected);

  SECTION("Returns a reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(sut.error()),int&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(&sut.error() == &expected);
  }
}

TEST_CASE("failure<E&>::error() &&", "[accessor]") {
  auto expected = 42;
  auto sut = failure<int&>(expected);

  SECTION("Returns an lvalue reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(std::move(sut).error()),int&>::value);
  }
  SECTION("Is equal to the input value") {
    int& x = std::move(sut).error();
    REQUIRE(&x == &expected);
  }
}

TEST_CASE("failure<E&>::error() const &", "[accessor]") {
  auto expected = 42;
  const auto sut = failure<int&>(expected);

  SECTION("Returns an lvalue reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(sut.error()),int&>::value);
  }
  SECTION("Is equal to the input value") {
    REQUIRE(&sut.error() == &expected);
  }
}

TEST_CASE("failure<E&>::error() const &&", "[accessor]") {
  auto expected = 42;
  const auto sut = failure<int&>(expected);

  SECTION("Returns an lvalue reference to the internal value") {
    STATIC_REQUIRE(std::is_same<decltype(std::move(sut).error()),int&>::value);
  }
  SECTION("Is equal to the input value") {
    int& x = std::move(sut).error();
    REQUIRE(&x == &expected);
  }
}

//=============================================================================
// non-member functions : class : failure<E&>
//=============================================================================

//-----------------------------------------------------------------------------
// Comparison
//-----------------------------------------------------------------------------

TEST_CASE("operator==(const failure<E1>&, const failure<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = failure<int>(value);
  const auto rhs = fail(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs == rhs);
  }
}

TEST_CASE("operator!=(const failure<E1>&, const failure<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = failure<int>(0);
  const auto rhs = fail(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs != rhs);
  }
}

TEST_CASE("operator>=(const failure<E1>&, const failure<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = failure<int>(100);
  const auto rhs = fail(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs >= rhs);
  }
}

TEST_CASE("operator<=(const failure<E1>&, const failure<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = failure<int>(0);
  const auto rhs = fail(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs <= rhs);
  }
}

TEST_CASE("operator<(const failure<E1>&, const failure<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = failure<int>(0);
  const auto rhs = fail(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs < rhs);
  }
}

TEST_CASE("operator>(const failure<E1>&, const failure<E2>&)", "[compare]") {
  const auto value = 42l;
  const auto lhs = failure<int>(100);
  const auto rhs = fail(std::ref(value));

  SECTION("Uses underlying comparison operator") {
    REQUIRE(lhs > rhs);
  }
}

//-----------------------------------------------------------------------------
// Utilities
//-----------------------------------------------------------------------------

TEST_CASE("fail(E&& e)", "[utilities]") {
  using error_type = std::string;
  const auto input = error_type{"Hello world"};

  auto sut = fail(input);

  SECTION("Deduces a decayed input") {
    STATIC_REQUIRE(std::is_same<decltype(sut),failure<error_type>>::value);
  }
  SECTION("Contains same value as input") {
    REQUIRE(sut.error() == input);
  }
}

TEST_CASE("fail<E>(Args&&...)", "[utilities]") {
  SECTION("Invokes E's constructor") {
    const auto input = "hello world";
    const auto size = 5u;
    const auto expected = std::string{input, size};
    const auto sut = fail<std::string>(input, size);

    REQUIRE(sut.error() == expected);
  }
}

TEST_CASE("fail<E>(std::initializer_list<U>, Args&&...)", "[utilities]") {
  SECTION("Invokes E's constructor") {
    const auto expected = std::string{
      {'H','e','l','l','o'},
      std::allocator<char>{}
    };
    const auto sut = fail<std::string>(
      {'H','e','l','l','o'},
      std::allocator<char>{}
    );

    REQUIRE(sut.error() == expected);
  }
}

TEST_CASE("fail(std::reference_wrapper<E>)", "[utilities]") {
  using error_type = std::string;

  auto error = error_type{};
  auto sut = fail(std::ref(error));

  SECTION("Deduces failure<E&>") {
    STATIC_REQUIRE(std::is_same<decltype(sut),failure<error_type&>>::value);
  }
  SECTION("Refers to input") {
    REQUIRE(&sut.error() == &error);
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("swap(failure<E>&, failure<E>&)", "[utilities]") {
  SECTION("Swaps the underlying contents") {
    const auto left_input = std::string{"Hello"};
    const auto right_input = std::string{"Goodbye"};

    auto lhs = fail(left_input);
    auto rhs = fail(right_input);

    swap(lhs, rhs);

    SECTION("lhs contains old value of rhs") {
      REQUIRE(lhs.error() == right_input);
    }
    SECTION("rhs contains old value of lhs") {
      REQUIRE(rhs.error() == left_input);
    }
  }
}

} // namespace test
} // namespace cpp
