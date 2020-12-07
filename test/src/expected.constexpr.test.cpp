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

namespace cpp {
namespace test {
namespace {

/// \brief A pointless utility that exists to force a move operation to occur
///        in a constexpr context
///
/// \param x the value to move
/// \return a new T object
template <typename T>
constexpr auto force_move(T&& x) -> T {
  return static_cast<T&&>(x);
}

struct constexpr_type
{
  constexpr constexpr_type() noexcept = default;
  constexpr constexpr_type(const constexpr_type&) noexcept = default;
  constexpr constexpr_type(constexpr_type&&) noexcept = default;
  constexpr constexpr_type(int x) noexcept : m_storage{x}{}

  auto operator=(const constexpr_type&) noexcept -> constexpr_type& = default;
  auto operator=(constexpr_type&&) noexcept -> constexpr_type& = default;
  auto operator=(int x) noexcept -> constexpr_type&
  {
    m_storage = x;
    return (*this);
  }
  int m_storage = 0;
};

inline constexpr
auto operator==(const constexpr_type& lhs, const constexpr_type& rhs)
  noexcept -> bool
{
  return lhs.m_storage == rhs.m_storage;
}

using literal_sut = expected<constexpr_type, constexpr_type>;

static_assert(std::is_literal_type<literal_sut>::value, "");
static_assert(std::is_trivially_copyable<literal_sut>::value, "");
static_assert(std::is_trivially_destructible<literal_sut>::value, "");

} // namespace <anonymous>

//=============================================================================
// class : expected<T,E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Destructor / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("constexpr expected<T,E>::expected()", "[constexpr][ctor]") {
  constexpr literal_sut sut{};

  STATIC_REQUIRE(sut.has_value());
}

TEST_CASE("constexpr expected<T,E>::expected(const expected&)", "[constexpr][ctor]") {
  constexpr literal_sut original{};
  constexpr literal_sut sut{original};

  STATIC_REQUIRE(sut.has_value());
}

TEST_CASE("constexpr expected<T,E>::expected(expected&&)", "[constexpr][ctor]") {
  constexpr literal_sut sut{ force_move(literal_sut{}) };

  STATIC_REQUIRE(sut.has_value());
}

TEST_CASE("constexpr expected<T,E>::expected(in_place_t, Args&&...)", "[constexpr][ctor]") {
  constexpr literal_sut sut{in_place, 42};

  STATIC_REQUIRE(sut.has_value());
}

TEST_CASE("constexpr expected<T,E>::expected(in_place_error_t, Args&&...)", "[constexpr][ctor]") {
  constexpr literal_sut sut{in_place_error, 42};

  STATIC_REQUIRE(sut.has_error());
}

TEST_CASE("constexpr expected<T,E>::expected(U&&) (implicit)", "[constexpr][ctor]") {
  constexpr literal_sut sut = 42;

  STATIC_REQUIRE(sut.has_value());
}

TEST_CASE("constexpr expected<T,E>::expected(U&&) (explicit)", "[constexpr][ctor]") {
  constexpr literal_sut sut{42};

  STATIC_REQUIRE(sut.has_value());
}

TEST_CASE("constexpr expected<T,E>::expected(const unexpected<U>&)", "[constexpr][ctor]") {
  constexpr unexpected<int> value{42};
  constexpr literal_sut sut{value};

  STATIC_REQUIRE(sut.has_error());
}

#if __cplusplus >= 201402L

// This constructor cannot be constexpr since C++11 does not allow non-const
// member functions to be constexpr, which disallows extracting an rvalue
// references from 'unexpected' in a constant expression. C++14 fixes this
TEST_CASE("constexpr expected<T,E>::expected(unexpected<U>&&)") {
  constexpr literal_sut sut{unexpected<int>{42}};

  STATIC_REQUIRE(sut.has_error());
}
#endif

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

TEST_CASE("constexpr expected<T,E>::value() const &", "[constexpr][observer]") {
  constexpr auto value = 42;
  constexpr literal_sut sut{value};

  STATIC_REQUIRE(sut.value() == value);
}

TEST_CASE("constexpr expected<T,E>::value() const &&", "[constexpr][observer]") {
  constexpr auto value = 42;
  constexpr literal_sut sut{value};

  STATIC_REQUIRE(static_cast<const literal_sut&&>(sut).value() == value);
}

TEST_CASE("constexpr expected<T,E>::error() const &", "[constexpr][observer]") {
  constexpr auto value = 42;
  constexpr auto error = unexpected<int>{value};
  constexpr literal_sut sut{error};

  STATIC_REQUIRE(sut.error() == value);
}

//=============================================================================
// class : expected<void,E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Destructor / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("constexpr expected<void,E>::expected()", "[constexpr][ctor]") {
  constexpr expected<void,int> sut{};

  STATIC_REQUIRE(sut.has_value());
}

TEST_CASE("constexpr expected<void,E>::expected(const expected&)", "[constexpr][ctor]") {
  constexpr expected<void,int> original{};
  constexpr expected<void,int> sut{original};

  STATIC_REQUIRE(sut.has_value());
}

TEST_CASE("constexpr expected<void,E>::expected(expected&&)", "[constexpr][ctor]") {
  constexpr expected<void,int> sut{ force_move(expected<void,int>{}) };

  STATIC_REQUIRE(sut.has_value());
}


TEST_CASE("constexpr expected<void,E>::expected(in_place_error_t, Args&&...)", "[constexpr][ctor]") {
  constexpr expected<void,int> sut{in_place_error, 42};

  STATIC_REQUIRE(sut.has_error());
}

TEST_CASE("constexpr expected<void,E>::expected(const unexpected<U>&)", "[constexpr][ctor]") {
  constexpr unexpected<int> value{42};
  constexpr expected<void,int> sut{value};

  STATIC_REQUIRE(sut.has_error());
}

#if __cplusplus >= 201402L

// This constructor cannot be constexpr since C++11 does not allow non-const
// member functions to be constexpr, which disallows extracting an rvalue
// references from 'unexpected' in a constant expression. C++14 fixes this
TEST_CASE("constexpr expected<void,E>::expected(unexpected<U>&&)") {
  constexpr expected<void,int> sut{unexpected<int>{42}};

  STATIC_REQUIRE(sut.has_error());
}
#endif

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

#if __cplusplus >= 201402L

TEST_CASE("constexpr expected<void,E>::value() const", "[constexpr][observer]") {
  constexpr expected<void,int> sut{};

  STATIC_REQUIRE((sut.value(),true));
}

#endif // __cplusplus >= 201402L

TEST_CASE("constexpr expected<void,E>::error() const &", "[constexpr][observer]") {
  constexpr auto value = 42;
  constexpr auto error = unexpected<int>{value};
  constexpr expected<void,int> sut{error};

  STATIC_REQUIRE(sut.error() == value);
}

} // namespace test
} // namespace cpp
