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

#include <string>
#include <type_traits>
#include <ios> // std::ios_errc

#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4244) // Disable warnings about implicit conversions
#endif

namespace expect {
namespace test {
namespace {

struct not_copy_or_moveable{
  not_copy_or_moveable() = default;
  not_copy_or_moveable(const not_copy_or_moveable&) = delete;
  not_copy_or_moveable(not_copy_or_moveable&&) = delete;

  auto operator=(const not_copy_or_moveable&) -> not_copy_or_moveable& = delete;
  auto operator=(not_copy_or_moveable&&) -> not_copy_or_moveable& = delete;
};

struct not_default_constructible{
  not_default_constructible() = delete;
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

template <typename T>
struct nonthrowing : public T
{
  nonthrowing() noexcept : T(){}

  nonthrowing(const T& other) noexcept : T(other){}
  nonthrowing(T&& other) noexcept : T(std::move(other)){}
  nonthrowing(const nonthrowing& other) noexcept : T(other){}
  nonthrowing(nonthrowing&& other) noexcept : T(std::move(other)){}

  template <typename...Args,
            typename = typename std::enable_if<std::is_constructible<T,Args...>::value>::type>
  nonthrowing(Args&&...args) : T(std::forward<Args>(args)...){}

  auto operator=(nonthrowing&& other) noexcept -> nonthrowing&
  {
    T::operator=(std::move(other));
    return (*this);
  }

  auto operator=(const nonthrowing& other) noexcept -> nonthrowing&
  {
    T::operator=(other);
    return (*this);
  }
  template <typename U,
            typename = typename std::enable_if<std::is_nothrow_assignable<T,U>::value>::type>
  auto operator=(U&& other) noexcept -> nonthrowing&
  {
    T::operator=(std::forward<U>(other));
    return (*this);
  }
};

template <typename T>
struct throwing : public T
{
  using T::T;

  throwing() noexcept(false) : T(){}

  throwing(const T& other) noexcept(false) : T(other){}
  throwing(T&& other) noexcept(false) : T(std::move(other)){}
  throwing(throwing&& other) noexcept(false) : T(std::move(other)) {}
  throwing(const throwing& other) noexcept(false) : T(other){}

  using T::operator=;
  auto operator=(throwing&& other) noexcept(false) -> throwing&
  {
    T::operator=(std::move(other));
    return (*this);
  }

  auto operator=(const throwing& other) noexcept(false) -> throwing&
  {
    T::operator=(other);
    return (*this);
  }
};

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

struct report_destructor
{
  report_destructor() : output{nullptr}{}
  report_destructor(bool* b) : output{b}{}

  ~report_destructor() {
    *output = true;
  }
  bool* output;
};

struct base
{
  virtual ~base() = default;
  virtual auto get_value() const noexcept -> int { return 42; }
};

struct derived : public base
{
  derived(int value) : value{value}{}

  auto get_value() const noexcept -> int override { return value; }
  int value;
};

} // namespace <anonymous>

//=============================================================================
// class : expected<T, E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Destructor / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("expected<T,E>::expected()", "[ctor]") {
  SECTION("T is default-constructible") {
    SECTION("Expected is default-constructible") {
      using sut_type = expected<int,int>;

      STATIC_REQUIRE(std::is_default_constructible<sut_type>::value);
    }
    // Note: 'E' must be default-constructible for use in 'error()', but
    //       otherwise does not contribute to this.
    SECTION("Default constructs the underlying T") {
      using value_type = std::string;
      using sut_type = expected<value_type,int>;

      auto sut = sut_type{};

      REQUIRE(sut.value() == value_type());
    }
    SECTION("Invokes T's default-construction") {
      // TODO: Use a static mock
    }
  }

  SECTION("T is not default-constructible") {
    SECTION("Expected is not default-constructible") {
      using sut_type = expected<not_default_constructible,int>;

      STATIC_REQUIRE_FALSE(std::is_default_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("expected<T,E>::expected(const expected&)", "[ctor]") {

  // Constructible:

  SECTION("T and E are both trivially copy-constructible") {
    using sut_type = expected<int,int>;

    SECTION("Expected is trivially copy-constructible") {
      STATIC_REQUIRE(std::is_trivially_copy_constructible<sut_type>::value);
    }

    SECTION("Copy source contained a value") {
      const auto value = 42;
      const sut_type source{value};

      const auto sut = source;

      SECTION("Copy contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("Copy contains a value equal to the source") {
        REQUIRE(sut.value() == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = make_unexpected(42);
      const sut_type source{error};

      const auto sut = source;

      SECTION("Copy contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("Copy contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  } // T and E are both trivially copy-constructible

  SECTION("T is copy-constructible, but not trivial") {
    using sut_type = expected<std::string,int>;

    SECTION("Expected is not trivially copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_trivially_copy_constructible<sut_type>::value);
    }

    SECTION("Expected is copy-constructible") {
      STATIC_REQUIRE(std::is_copy_constructible<sut_type>::value);
    }

    SECTION("Copy source contained a value") {
      const auto value = "hello world";
      const sut_type source{value};

      const auto sut = source;

      SECTION("Copy contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("Copy contains a value equal to the source") {
        REQUIRE(sut.value() == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = make_unexpected(42);
      const sut_type source{error};

      const auto sut = source;

      SECTION("Copy contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("Copy contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  } // T is copy-constructible, but not trivial

  SECTION("E is copy-constructible, but not trivial") {
    using sut_type = expected<int,std::string>;

    SECTION("Expected is not trivially copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_trivially_copy_constructible<sut_type>::value);
    }

    SECTION("Expected is copy-constructible") {
      STATIC_REQUIRE(std::is_copy_constructible<sut_type>::value);
    }

    SECTION("Copy source contained a value") {
      const auto value = 42;
      const sut_type source{value};

      const auto sut = source;

      SECTION("Copy contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("Copy contains a value equal to the source") {
        REQUIRE(sut.value() == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = make_unexpected("Hello world");
      const sut_type source{error};

      const auto sut = source;

      SECTION("Copy contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("Copy contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  SECTION("T and E are both copy-constructible, but not trivial") {
    using sut_type = expected<std::string,std::string>;

    SECTION("Expected is not trivially copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_trivially_copy_constructible<sut_type>::value);
    }

    SECTION("Expected is copy-constructible") {
      STATIC_REQUIRE(std::is_copy_constructible<sut_type>::value);
    }

    SECTION("Copy source contained a value") {
      const auto value = "Hello world";
      const sut_type source{value};

      const auto sut = source;

      SECTION("Copy contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("Copy contains a value equal to the source") {
        REQUIRE(sut.value() == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = make_unexpected("Goodbye world");
      const sut_type source{error};

      const auto sut = source;

      SECTION("Copy contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("Copy contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  // Not constructible:

  SECTION("T is not copy-constructible") {
    using sut_type = expected<not_copy_or_moveable,int>;

    SECTION("Expected is not copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_copy_constructible<sut_type>::value);
    }
  }

  SECTION("E is not copy-constructible") {
    using sut_type = expected<int,not_copy_or_moveable>;

    SECTION("Expected is not copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_copy_constructible<sut_type>::value);
    }
  }

  SECTION("T and E are not copy-constructible") {
    using sut_type = expected<not_copy_or_moveable,not_copy_or_moveable>;

    SECTION("Expected is not copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_copy_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("expected<T,E>::expected(expected&&)", "[ctor]") {

  // Constructible:

  SECTION("T and E are both trivially move-constructible") {
    using sut_type = expected<int,int>;

    SECTION("Expected is trivially move-constructible") {
      STATIC_REQUIRE(std::is_trivially_move_constructible<sut_type>::value);
    }

    SECTION("Move source contained a value") {
      const auto value = 42;
      sut_type source{value};

      const auto sut = std::move(source);

      SECTION("New expected contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("New expected contains a value equal to the source") {
        REQUIRE(sut.value() == value);
      }
    }

    SECTION("Move source contained an error") {
      const auto error = make_unexpected(42);
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New expected contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New expected contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  } // T and E are both trivially move-constructible

  SECTION("T is move-constructible, but not trivial") {
    using sut_type = expected<move_only<std::string>,int>;

    SECTION("Expected is not trivially move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_trivially_move_constructible<sut_type>::value);
    }

    SECTION("Expected is move-constructible") {
      STATIC_REQUIRE(std::is_move_constructible<sut_type>::value);
    }

    SECTION("Move source contained a value") {
      const auto value = "hello world";
      sut_type source{value};

      const auto sut = std::move(source);

      SECTION("New expected contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("New expected contains a value equal to the source") {
        REQUIRE(sut.value() == value);
      }
    }

    SECTION("Move source contained an error") {
      const auto error = make_unexpected(42);
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New expected contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New expected contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  } // T is move-constructible, but not trivial

  SECTION("E is move-constructible, but not trivial") {
    using sut_type = expected<int,move_only<std::string>>;

    SECTION("Expected is not trivially move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_trivially_move_constructible<sut_type>::value);
    }

    SECTION("Expected is move-constructible") {
      STATIC_REQUIRE(std::is_move_constructible<sut_type>::value);
    }

    SECTION("Move source contained a value") {
      const auto value = 42;
      sut_type source{value};

      const auto sut = std::move(source);

      SECTION("New expected contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("New expected contains a value equal to the source") {
        REQUIRE(sut.value() == value);
      }
    }

    SECTION("Move source contained an error") {
      const auto error = make_unexpected("Hello world");
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New expected contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New expected contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  SECTION("T and E are both move-constructible, but not trivial") {
    using sut_type = expected<move_only<std::string>,move_only<std::string>>;

    SECTION("Expected is not trivially move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_trivially_move_constructible<sut_type>::value);
    }

    SECTION("Expected is move-constructible") {
      STATIC_REQUIRE(std::is_move_constructible<sut_type>::value);
    }

    SECTION("Move source contained a value") {
      const auto value = "Hello world";
      sut_type source{value};

      const auto sut = std::move(source);

      SECTION("New expected contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("New expected contains a value equal to the source") {
        REQUIRE(sut.value() == value);
      }
    }

    SECTION("Move source contained an error") {
      const auto error = make_unexpected("Goodbye world");
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New expected contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New expected contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  // Not constructible:

  SECTION("T is not move-constructible") {
    using sut_type = expected<not_copy_or_moveable,int>;

    SECTION("Expected is not move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_move_constructible<sut_type>::value);
    }
  }

  SECTION("E is not move-constructible") {
    using sut_type = expected<int,not_copy_or_moveable>;

    SECTION("Expected is not move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_move_constructible<sut_type>::value);
    }
  }

  SECTION("T and E are not move-constructible") {
    using sut_type = expected<not_copy_or_moveable,not_copy_or_moveable>;

    SECTION("Expected is not move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_move_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("expected<T,E>::expected(const expected<T2,E2>&)", "[ctor]") {
  // Constructible:

  SECTION("T and E are both constructible from T2 and E2") {
    using copy_type = expected<const char*, const char*>;
    using sut_type = expected<std::string,std::string>;

    SECTION("Expected is constructible") {
      STATIC_REQUIRE(std::is_constructible<sut_type,const copy_type&>::value);
    }

    SECTION("Expected is implicit constructible") {
      STATIC_REQUIRE(std::is_convertible<const copy_type&,sut_type>::value);
    }

    SECTION("Copy source contained a value") {
      const auto value = "Hello world";
      const copy_type source{value};

      const sut_type sut = source;

      SECTION("Copy contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("Copy contains a value equal to the source") {
        REQUIRE(sut.value() == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = make_unexpected("Goodbye world");
      const copy_type source{error};

      const sut_type sut = source;

      SECTION("Copy contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("Copy contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  // Not Constructible:

  SECTION("T is not constructible from T2") {
    using copy_type = expected<std::string,int>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }

  SECTION("E is not constructible from E2") {
    using copy_type = expected<int,std::string>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }

  SECTION("T and E are not constructible from T2 and E2") {
    using copy_type = expected<std::string,std::string>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }
}

TEST_CASE("expected<T,E>::expected(const expected<T2,E2>&) (explicit)", "[ctor]") {

  // Constructible:

  SECTION("T and E are both constructible from T2 and E2") {
    SECTION("T is explicit constructible from T2") {
      using copy_type = expected<std::string, const char*>;
      using sut_type = expected<explicit_type<std::string>,std::string>;

      SECTION("Expected is constructible") {
        STATIC_REQUIRE(std::is_constructible<sut_type,const copy_type&>::value);
      }

      SECTION("Expected is not implicit constructible") {
        STATIC_REQUIRE_FALSE(std::is_convertible<const copy_type&,sut_type>::value);
      }

      SECTION("Copy source contained a value") {
        const auto value = "Hello world";
        const copy_type source{value};

        const sut_type sut{source};

        SECTION("Copy contains a value") {
          REQUIRE(sut.has_value());
        }
        SECTION("Copy contains a value equal to the source") {
          REQUIRE(sut.value() == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = make_unexpected("Goodbye world");
        const copy_type source{error};

        const sut_type sut{source};

        SECTION("Copy contains an error") {
          REQUIRE(sut.has_error());
        }
        SECTION("Copy contains an error equal to the source") {
          REQUIRE(sut == error);
        }
      }
    }

    SECTION("E is explicit constructible from E2") {
      using copy_type = expected<const char*,std::string>;
      using sut_type = expected<std::string,explicit_type<std::string>>;

      SECTION("Expected is constructible") {
        STATIC_REQUIRE(std::is_constructible<sut_type,const copy_type&>::value);
      }

      SECTION("Expected is not implicit constructible") {
        STATIC_REQUIRE_FALSE(std::is_convertible<const copy_type&,sut_type>::value);
      }

      SECTION("Copy source contained a value") {
        const auto value = "Hello world";
        const copy_type source{value};

        const sut_type sut{source};

        SECTION("Copy contains a value") {
          REQUIRE(sut.has_value());
        }
        SECTION("Copy contains a value equal to the source") {
          REQUIRE(sut.value() == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = make_unexpected("Goodbye world");
        const copy_type source{error};

        const sut_type sut{source};

        SECTION("Copy contains an error") {
          REQUIRE(sut.has_error());
        }
        SECTION("Copy contains an error equal to the source") {
          REQUIRE(sut == error);
        }
      }
    }
    SECTION("T and E are explicit constructible from T2 and E2") {
      using copy_type = expected<std::string, std::string>;
      using sut_type = expected<explicit_type<std::string>,explicit_type<std::string>>;

      SECTION("Expected is constructible") {
        STATIC_REQUIRE(std::is_constructible<sut_type,const copy_type&>::value);
      }

      SECTION("Expected is not implicit constructible") {
        STATIC_REQUIRE_FALSE(std::is_convertible<const copy_type&,sut_type>::value);
      }

      SECTION("Copy source contained a value") {
        const auto value = "Hello world";
        const copy_type source{value};

        const sut_type sut{source};

        SECTION("Copy contains a value") {
          REQUIRE(sut.has_value());
        }
        SECTION("Copy contains a value equal to the source") {
          REQUIRE(sut.value() == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = make_unexpected("Goodbye world");
        const copy_type source{error};

        const sut_type sut{source};

        SECTION("Copy contains an error") {
          REQUIRE(sut.has_error());
        }
        SECTION("Copy contains an error equal to the source") {
          REQUIRE(sut == error);
        }
      }
    }
  }

  // Not Constructible:

  SECTION("T is not constructible from T2") {
    using copy_type = expected<std::string,int>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }

  SECTION("E is not constructible from E2") {
    using copy_type = expected<int,std::string>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }

  SECTION("T and E are not constructible from T2 and E2") {
    using copy_type = expected<std::string,std::string>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }
}

TEST_CASE("expected<T,E>::expected(expected<T2,E2>&&)", "[ctor]") {

  // Constructible Case:

  SECTION("T and E are both constructible from T2 and E2") {
    using copy_type = expected<std::string,std::string>;
    using sut_type = expected<move_only<std::string>,move_only<std::string>>;

    SECTION("Expected is constructible") {
      STATIC_REQUIRE(std::is_constructible<sut_type,copy_type&&>::value);
    }

    SECTION("Expected is implicit constructible") {
      STATIC_REQUIRE(std::is_convertible<copy_type&&,sut_type>::value);
    }

    SECTION("Copy source contained a value") {
      const auto value = "Hello world";
      copy_type source{value};

      const sut_type sut = std::move(source);

      SECTION("Copy contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("Copy contains a value equal to the source") {
        REQUIRE(sut.value() == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = make_unexpected("Goodbye world");
      copy_type source{error};

      const sut_type sut = std::move(source);

      SECTION("Copy contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("Copy contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  // Not Constructible:

  SECTION("T is not constructible from T2") {
    using copy_type = expected<std::string,int>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }

  SECTION("E is not constructible from E2") {
    using copy_type = expected<int,std::string>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }

  SECTION("T and E are not constructible from T2 and E2") {
    using copy_type = expected<std::string,std::string>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }
}

TEST_CASE("expected<T,E>::expected(expected<T2,E2>&&) (explicit)", "[ctor]") {

  // Constructors:

  SECTION("T and E are both constructible from T2 and E2") {
    SECTION("T is explicit constructible from T2") {
      using copy_type = expected<std::string, const char*>;
      using sut_type = expected<explicit_type<move_only<std::string>>,std::string>;

      SECTION("Expected is constructible") {
        STATIC_REQUIRE(std::is_constructible<sut_type,copy_type&&>::value);
      }

      SECTION("Expected is not implicit constructible") {
        STATIC_REQUIRE_FALSE(std::is_convertible<copy_type&&,sut_type>::value);
      }

      SECTION("Copy source contained a value") {
        const auto value = "Hello world";
        copy_type source{value};

        const sut_type sut{std::move(source)};

        SECTION("Copy contains a value") {
          REQUIRE(sut.has_value());
        }
        SECTION("Copy contains a value equal to the source") {
          REQUIRE(sut.value() == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = make_unexpected("Goodbye world");
        copy_type source{error};

        const sut_type sut{std::move(source)};

        SECTION("Copy contains an error") {
          REQUIRE(sut.has_error());
        }
        SECTION("Copy contains an error equal to the source") {
          REQUIRE(sut == error);
        }
      }
    }

    SECTION("E is explicit constructible from E2") {
      using copy_type = expected<const char*,std::string>;
      using sut_type = expected<std::string,explicit_type<move_only<std::string>>>;

      SECTION("Expected is constructible") {
        STATIC_REQUIRE(std::is_constructible<sut_type,copy_type&&>::value);
      }

      SECTION("Expected is not implicit constructible") {
        STATIC_REQUIRE_FALSE(std::is_convertible<copy_type&&,sut_type>::value);
      }

      SECTION("Copy source contained a value") {
        const auto value = "Hello world";
        copy_type source{value};

        const sut_type sut{std::move(source)};

        SECTION("Copy contains a value") {
          REQUIRE(sut.has_value());
        }
        SECTION("Copy contains a value equal to the source") {
          REQUIRE(sut.value() == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = make_unexpected("Goodbye world");
        copy_type source{error};

        const sut_type sut{std::move(source)};

        SECTION("Copy contains an error") {
          REQUIRE(sut.has_error());
        }
        SECTION("Copy contains an error equal to the source") {
          REQUIRE(sut == error);
        }
      }
    }
    SECTION("T and E are explicit constructible from T2 and E2") {
      using copy_type = expected<std::string, std::string>;
      using sut_type = expected<
        explicit_type<move_only<std::string>>,
        explicit_type<move_only<std::string>>
      >;

      SECTION("Expected is constructible") {
        STATIC_REQUIRE(std::is_constructible<sut_type,copy_type&&>::value);
      }

      SECTION("Expected is not implicit constructible") {
        STATIC_REQUIRE_FALSE(std::is_convertible<copy_type&&,sut_type>::value);
      }

      SECTION("Copy source contained a value") {
        const auto value = "Hello world";
        copy_type source{value};

        const sut_type sut{std::move(source)};

        SECTION("Copy contains a value") {
          REQUIRE(sut.has_value());
        }
        SECTION("Copy contains a value equal to the source") {
          REQUIRE(sut.value() == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = make_unexpected("Goodbye world");
        copy_type source{error};

        const sut_type sut{std::move(source)};

        SECTION("Copy contains an error") {
          REQUIRE(sut.has_error());
        }
        SECTION("Copy contains an error equal to the source") {
          REQUIRE(sut == error);
        }
      }
    }
  }

  // Not Constructible:

  SECTION("T is not constructible from T2") {
    using copy_type = expected<std::string,int>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }

  SECTION("E is not constructible from E2") {
    using copy_type = expected<int,std::string>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }

  SECTION("T and E are not constructible from T2 and E2") {
    using copy_type = expected<std::string,std::string>;
    using sut_type = expected<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }
}

TEST_CASE("expected<T,E>::expected(in_place_t, Args&&...)", "[ctor]") {
  using sut_type = expected<std::string,int>;

  const auto input = "hello world";
  const auto size = 5;
  const auto value = std::string{input, size};

  const sut_type sut(in_place, input, size);

  SECTION("Contains a value") {
    REQUIRE(sut.has_value());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == value);
  }
}

TEST_CASE("expected<T,E>::expected(in_place_t, std::initializer_list<U>, Args&&...)", "[ctor]") {
  using sut_type = expected<std::string,int>;

  const auto value = std::string{
    {'h','e','l','l','o'}, std::allocator<char>{}
  };

  const sut_type sut(in_place, {'h','e','l','l','o'}, std::allocator<char>{});

  SECTION("Contains a value") {
    REQUIRE(sut.has_value());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == value);
  }
}

TEST_CASE("expected<T,E>::expected(in_place_error_t, Args&&...)", "[ctor]") {
  using sut_type = expected<int,std::string>;

  const auto input = "hello world";
  const auto size = 5;
  const auto expected = std::string{input, size};

  const sut_type sut(in_place_error, input, size);

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == make_unexpected(std::ref(expected)));
  }
}

TEST_CASE("expected<T,E>::expected(in_place_error_t, std::initializer_list<U>, Args&&...)", "[ctor]") {
  using sut_type = expected<int,std::string>;

  const auto value = std::string{
    {'h','e','l','l','o'}, std::allocator<char>{}
  };

  const sut_type sut(in_place_error, {'h','e','l','l','o'}, std::allocator<char>{});

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == make_unexpected(std::ref(value)));
  }
}

TEST_CASE("expected<T,E>::expected(const unexpected<E2>&)", "[ctor]") {
  using sut_type = expected<int,std::string>;
  const auto source = make_unexpected<std::string>("hello world");

  const sut_type sut{source};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == source);
  }
}

TEST_CASE("expected<T,E>::expected(unexpected<E2>&&)", "[ctor]") {
  using sut_type = expected<int,move_only<std::string>>;
  auto source = make_unexpected<std::string>("hello world");
  const auto copy = source;

  const sut_type sut{std::move(source)};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from E2") {
    REQUIRE(sut == copy);
  }
}

TEST_CASE("expected<T,E>::expected(U&&)", "[ctor]") {
  using sut_type = expected<std::string,int>;
  auto source = "Hello world";
  const auto copy = std::string{source};

  const sut_type sut = source;

  SECTION("Contains a value") {
    REQUIRE(sut.has_value());
  }

  SECTION("Contains a value constructed from U") {
    REQUIRE(sut == copy);
  }

  SECTION("Is disabled for explicit constructors") {
    STATIC_REQUIRE_FALSE(std::is_convertible<const std::string&,explicit_type<std::string>>::value);
  }
}

TEST_CASE("expected<T,E>::expected(U&&) (explicit)", "[ctor]") {
  using sut_type = expected<explicit_type<std::string>,int>;
  auto source = "Hello world";
  const auto copy = std::string{source};

  const sut_type sut{source};

  SECTION("Contains a value") {
    REQUIRE(sut.has_value());
  }

  SECTION("Contains a value constructed from U") {
    REQUIRE(sut == copy);
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<T,E>::~expected()", "[dtor]") {
  SECTION("T and E are trivially destructible") {
    using sut_type = expected<int, int>;

    SECTION("expected's destructor is trivial") {
      STATIC_REQUIRE(std::is_trivially_destructible<sut_type>::value);
    }
  }
  SECTION("T is not trivially destructible") {
    using sut_type = expected<report_destructor, int>;

    auto invoked = false;
    {
      sut_type sut{in_place, &invoked};
      (void) sut;
    }

    SECTION("expected's destructor is not trivial") {
      STATIC_REQUIRE_FALSE(std::is_trivially_destructible<sut_type>::value);
    }

    SECTION("Invokes T's underlying destructor") {
      REQUIRE(invoked);
    }
  }
  SECTION("E is not trivially destructible") {
    using sut_type = expected<int, report_destructor>;

    auto invoked = false;
    {
      sut_type sut{in_place_error, &invoked};
      (void) sut;
    }

    SECTION("expected's destructor is not trivial") {
      STATIC_REQUIRE_FALSE(std::is_trivially_destructible<sut_type>::value);
    }

    SECTION("Invokes E's underlying destructor") {
      REQUIRE(invoked);
    }
  }
  SECTION("T and E are not trivially destructible") {
    using sut_type = expected<report_destructor, report_destructor>;

    SECTION("T is active") {
      SECTION("Invokes T's underlying destructor") {
        auto invoked = false;
        {
          sut_type sut{in_place, &invoked};
          (void) sut;
        }
        REQUIRE(invoked);
      }
    }
    SECTION("E is active") {
      SECTION("Invokes E's underlying destructor") {
        auto invoked = false;
        {
          sut_type sut{in_place_error, &invoked};
          (void) sut;
        }
        REQUIRE(invoked);
      }
    }

    SECTION("expected's destructor is not trivial") {
      STATIC_REQUIRE_FALSE(std::is_trivially_destructible<sut_type>::value);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<T,E>::operator=(const expected&)", "[assign]") {
  SECTION("T is not nothrow copy constructible") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = expected<std::string,int>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }
  SECTION("E is not nothrow copy constructible") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = expected<int,std::string>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }
  SECTION("T and E are not nothrow copy constructible") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = expected<std::string,std::string>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }

  SECTION("T is not copy-assignable") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = expected<not_copy_or_moveable,int>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }
  SECTION("E is not copy-assignable") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = expected<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }
  SECTION("T and E are not copy-assignable") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = expected<not_copy_or_moveable,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }

  SECTION("T and E are nothrow copy-constructible") {
    SECTION("Expected is copy-assignable") {
      using sut_type = expected<int,std::error_code>;

      STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
    }
    SECTION("expected contains a value") {
      SECTION("'other' contains a value") {
        using sut_type = expected<int,std::error_code>;

        const auto value = 42;
        const sut_type copy{value};
        sut_type sut{};

        sut = copy;
        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
        }
        SECTION("active element is value") {
          REQUIRE(sut.has_value());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
      SECTION("'other' contains an error") {
        using sut_type = expected<report_destructor,const char*>;

        const auto value = make_unexpected("42");
        auto is_invoked = false;
        const sut_type copy{value};
        sut_type sut{&is_invoked};

        sut = copy;
        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
        }
        SECTION("Calls 'T's destructor first") {
          REQUIRE(is_invoked);
        }
        SECTION("active element is error") {
          REQUIRE(sut.has_error());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
    }
    SECTION("expected contains an error") {
      SECTION("'other' contains a value") {
        using sut_type = expected<int,report_destructor>;

        const auto value = 42;
        auto is_invoked = false;
        const sut_type copy{value};
        sut_type sut{in_place_error, &is_invoked};

        sut = copy;

        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
        }
        SECTION("Calls 'T's destructor first") {
          REQUIRE(is_invoked);
        }
        SECTION("active element is value") {
          REQUIRE(sut.has_value());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
      SECTION("'other' contains an error") {
        using sut_type = expected<int,int>;

        const auto value = make_unexpected(42);
        const sut_type copy{value};
        sut_type sut{make_unexpected(0)};

        sut = copy;
        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
        }
        SECTION("active element is error") {
          REQUIRE(sut.has_error());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
    }
  }

  SECTION("T and E are trivial copy-constructible and copy-assignable") {
    using sut_type = expected<int, int>;

    SECTION("Expected is copy-assignable") {
      STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
    }
    SECTION("Expected is trivial copy-assignable") {
      STATIC_REQUIRE(std::is_trivially_copy_assignable<sut_type>::value);
    }
  }
}

TEST_CASE("expected<T,E>::operator=(expected&&)", "[assign]") {
  SECTION("T is not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using sut_type = expected<throwing<std::string>,int>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }
  SECTION("E is not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using sut_type = expected<int,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }
  SECTION("T and E are not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using sut_type = expected<throwing<std::string>,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }

  SECTION("T is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using sut_type = expected<not_copy_or_moveable,int>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }
  SECTION("E is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using sut_type = expected<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }
  SECTION("T and E are not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using sut_type = expected<not_copy_or_moveable,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }

  SECTION("T and E are nothrow copy-constructible") {
    SECTION("Expected is move-assignable") {
      using sut_type = expected<int,std::error_code>;

      STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
    }
    SECTION("expected contains a value") {
      SECTION("'other' contains a value") {
        using sut_type = expected<move_only<std::string>,std::error_code>;

        const auto value = "Hello world";
        sut_type original{value};
        sut_type sut{};

        sut = std::move(original);
        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
        }
        SECTION("active element is value") {
          REQUIRE(sut.has_value());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
      SECTION("'other' contains an error") {
        using sut_type = expected<move_only<report_destructor>,const char*>;

        const auto value = make_unexpected("42");
        auto is_invoked = false;
        sut_type original{value};
        sut_type sut{&is_invoked};

        sut = std::move(original);
        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
        }
        SECTION("Calls 'T's destructor first") {
          REQUIRE(is_invoked);
        }
        SECTION("active element is error") {
          REQUIRE(sut.has_error());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
    }
    SECTION("expected contains an error") {
      SECTION("'other' contains a value") {
        using sut_type = expected<int,move_only<report_destructor>>;

        const auto value = 42;
        auto is_invoked = false;
        sut_type original{value};
        sut_type sut{in_place_error, &is_invoked};

        sut = std::move(original);

        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
        }
        SECTION("Calls 'T's destructor first") {
          REQUIRE(is_invoked);
        }
        SECTION("active element is value") {
          REQUIRE(sut.has_value());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
      SECTION("'other' contains an error") {
        using sut_type = expected<int,move_only<std::string>>;

        const auto value = make_unexpected("hello world");
        sut_type copy{value};
        sut_type sut{make_unexpected("goodbye world")};

        sut = std::move(copy);
        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
        }
        SECTION("active element is error") {
          REQUIRE(sut.has_error());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
    }
  }

  SECTION("T and E are trivial copy-constructible and copy-assignable") {
    using sut_type = expected<int, int>;

    SECTION("Expected is move-assignable") {
      STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
    }
    SECTION("Expected is trivial move-assignable") {
      STATIC_REQUIRE(std::is_trivially_move_assignable<sut_type>::value);
    }
  }
}

TEST_CASE("expected<T,E>::operator=(const expected<T2,E2>&)", "[assign]") {
SECTION("T is not nothrow constructible from T2") {
    SECTION("Expected is not assignable") {
      using copy_type = expected<std::string, long>;
      using sut_type = expected<throwing<std::string>,int>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }
  SECTION("E is not nothrow constructible from E2") {
    SECTION("Expected is not assignable") {
      using copy_type = expected<int,std::string>;
      using sut_type = expected<int,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }
  SECTION("T and E are not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using copy_type = expected<std::string,std::string>;
      using sut_type = expected<throwing<std::string>,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }

  SECTION("T is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = expected<not_copy_or_moveable,long>;
      using sut_type = expected<not_copy_or_moveable,int>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }
  SECTION("E is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = expected<long,not_copy_or_moveable>;
      using sut_type = expected<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }
  SECTION("T and E are not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = expected<not_copy_or_moveable,not_copy_or_moveable>;
      using sut_type = expected<not_copy_or_moveable,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }

  SECTION("T and E are nothrow copy-constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = expected<long,std::io_errc>;
      using sut_type = expected<int,move_only<std::error_code>>;

      STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
    }
    SECTION("expected contains a value") {
      SECTION("'other' contains a value") {
        using copy_type = expected<const char*,std::error_code>;
        using sut_type = expected<nonthrowing<std::string>,std::error_code>;

        const auto value = "Hello world";
        copy_type original{value};
        sut_type sut{};

        sut = original;

        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
        }
        SECTION("active element is value") {
          REQUIRE(sut.has_value());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
      SECTION("'other' contains an error") {
        using copy_type = expected<report_destructor,const char*>;
        using sut_type = expected<report_destructor,const char*>;

        const auto value = make_unexpected("42");
        auto is_invoked = false;
        copy_type original{value};
        sut_type sut{&is_invoked};

        sut = original;

        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
        }
        SECTION("Calls 'T's destructor first") {
          REQUIRE(is_invoked);
        }
        SECTION("active element is error") {
          REQUIRE(sut.has_error());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
    }
    SECTION("expected contains an error") {
      SECTION("'other' contains a value") {
        using copy_type = expected<int,report_destructor>;
        using sut_type = expected<int,report_destructor>;

        const auto value = 42;
        auto is_invoked = false;
        copy_type original{value};
        sut_type sut{in_place_error, &is_invoked};

        sut = original;

        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
        }
        SECTION("Calls 'T's destructor first") {
          REQUIRE(is_invoked);
        }
        SECTION("active element is value") {
          REQUIRE(sut.has_value());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
      SECTION("'other' contains an error") {
        using copy_type = expected<int,const char*>;
        using sut_type = expected<int,nonthrowing<std::string>>;

        const auto value = make_unexpected("hello world");
        copy_type copy{value};
        sut_type sut{make_unexpected("goodbye world")};

        sut = copy;

        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
        }
        SECTION("active element is error") {
          REQUIRE(sut.has_error());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
    }
  }
}

TEST_CASE("expected<T,E>::operator=(expected<T2,E2>&&)", "[assign]") {
  SECTION("T is not nothrow constructible from T2") {
    SECTION("Expected is not assignable") {
      using copy_type = expected<std::string, long>;
      using sut_type = expected<throwing<std::string>,int>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, copy_type&&>::value);
    }
  }
  SECTION("E is not nothrow constructible from E2") {
    SECTION("Expected is not assignable") {
      using copy_type = expected<int,std::string>;
      using sut_type = expected<int,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, copy_type&&>::value);
    }
  }
  SECTION("T and E are not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using copy_type = expected<std::string,std::string>;
      using sut_type = expected<throwing<std::string>,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, copy_type&&>::value);
    }
  }

  SECTION("T is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = expected<not_copy_or_moveable,long>;
      using sut_type = expected<not_copy_or_moveable,int>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, copy_type&&>::value);
    }
  }
  SECTION("E is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = expected<long,not_copy_or_moveable>;
      using sut_type = expected<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, copy_type&&>::value);
    }
  }
  SECTION("T and E are not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = expected<not_copy_or_moveable,not_copy_or_moveable>;
      using sut_type = expected<not_copy_or_moveable,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("T and E are nothrow copy-constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = expected<long,std::io_errc>;
      using sut_type = expected<int,move_only<std::error_code>>;

      STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
    }
    SECTION("expected contains a value") {
      SECTION("'other' contains a value") {
        using copy_type = expected<std::string,std::error_code>;
        using sut_type = expected<move_only<std::string>,std::error_code>;

        const auto value = "Hello world";
        copy_type original{value};
        sut_type sut{};

        sut = std::move(original);
        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
        }
        SECTION("active element is value") {
          REQUIRE(sut.has_value());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
      SECTION("'other' contains an error") {
        using copy_type = expected<report_destructor,const char*>;
        using sut_type = expected<move_only<report_destructor>,const char*>;

        const auto value = make_unexpected("42");
        auto is_invoked = false;
        copy_type original{value};
        sut_type sut{&is_invoked};

        sut = std::move(original);

        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
        }
        SECTION("Calls 'T's destructor first") {
          REQUIRE(is_invoked);
        }
        SECTION("active element is error") {
          REQUIRE(sut.has_error());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
    }
    SECTION("expected contains an error") {
      SECTION("'other' contains a value") {
        using copy_type = expected<int,report_destructor>;
        using sut_type = expected<int,move_only<report_destructor>>;

        const auto value = 42;
        auto is_invoked = false;
        copy_type original{value};
        sut_type sut{in_place_error, &is_invoked};

        sut = std::move(original);

        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
        }
        SECTION("Calls 'T's destructor first") {
          REQUIRE(is_invoked);
        }
        SECTION("active element is value") {
          REQUIRE(sut.has_value());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
      SECTION("'other' contains an error") {
        using copy_type = expected<int,std::string>;
        using sut_type = expected<int,move_only<std::string>>;

        const auto value = make_unexpected("hello world");
        copy_type copy{value};
        sut_type sut{make_unexpected("goodbye world")};

        sut = std::move(copy);

        SECTION("Expected can be assigned") {
          STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
        }
        SECTION("active element is error") {
          REQUIRE(sut.has_error());
        }
        SECTION("error is copied") {
          REQUIRE(sut == value);
        }
      }
    }
  }
}

TEST_CASE("expected<T,E>::operator=(U&&)", "[assign]") {
  SECTION("T is not nothrow constructible from U") {
    SECTION("expected is not assignable from U") {
      STATIC_REQUIRE_FALSE(std::is_assignable<expected<throwing<std::string>,std::error_code>,const char*>::value);
    }
  }
  SECTION("T is constructible from U, and nothrow move constructible") {
    SECTION("expected is assignable from U") {
      // This works by creating an intermediate `expected` object which then
      // is moved through non-throwing move-assignment
      STATIC_REQUIRE(std::is_assignable<expected<std::string,std::error_code>,const char*>::value);
    }
  }
  SECTION("T is not assignable or constructible from U") {
    SECTION("expected is not assignable from U") {
      STATIC_REQUIRE_FALSE(std::is_assignable<expected<int,std::error_code>,const char*>::value);
    }
  }
  SECTION("expected contains a value") {
    using sut_type = expected<int,std::error_code>;

    const auto value = 42ll;
    sut_type sut{};

    sut = value;

    SECTION("Expected can be assigned") {
      STATIC_REQUIRE(std::is_assignable<sut_type,decltype(value)>::value);
    }
    SECTION("active element is value") {
      REQUIRE(sut.has_value());
    }
    SECTION("error is copied") {
      REQUIRE(sut == value);
    }
  }
  SECTION("expected contains an error") {
    using sut_type = expected<int, report_destructor>;

    const auto value = 42ll;
    auto is_invoked = false;
    sut_type sut{in_place_error, &is_invoked};

    sut = value;
    SECTION("Expected can be assigned") {
      STATIC_REQUIRE(std::is_assignable<sut_type,decltype(value)>::value);
    }
    SECTION("Calls 'T's destructor first") {
      REQUIRE(is_invoked);
    }
    SECTION("active element is value") {
      REQUIRE(sut.has_value());
    }
    SECTION("error is copied") {
      REQUIRE(sut == value);
    }
  }
}

TEST_CASE("expected<T,E>::operator=(const unexpected<E2>&)", "[assign]") {
  SECTION("E cant be constructed or assigned from E2") {
    SECTION("Expected cannot be assigned from E2") {
      using copy_type = unexpected<not_copy_or_moveable>;
      using sut_type  = expected<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }
  SECTION("E is not nothrow constructible from E2") {
    SECTION("Expected is not copy-assignable") {
      using copy_type = unexpected<const char*>;
      using sut_type = expected<int,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E is not nothrow move constructible from E2, but is nothrow-move constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = unexpected<const char*>;
      using sut_type = expected<int,std::string>;

      // This works because it generates an intermediate 'expected<int, E>' in
      // between. The expected is then move-constructed instead -- which is
      // non-throwing.

      STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E can be constructed and assigned from E2") {
    SECTION("active element is T") {
      using copy_type = unexpected<const char*>;
      using sut_type = expected<report_destructor, std::string>;

      const auto value = copy_type{"hello world"};

      auto is_invoked = false;
      sut_type sut{&is_invoked};

      sut = value;

      SECTION("Expected can be assigned") {
        STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
      }
      SECTION("Calls 'T's destructor first") {
        REQUIRE(is_invoked);
      }
      SECTION("active element is error") {
        REQUIRE(sut.has_error());
      }
      SECTION("error is copied") {
        REQUIRE(sut == value);
      }
    }

    SECTION("active element is E") {
      using copy_type = unexpected<const char*>;
      using sut_type = expected<int, std::string>;

      const auto value = copy_type{"goodbye world"};
      sut_type sut{copy_type{"hello world"}};

      sut = value;

      SECTION("Expected can be assigned") {
        STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
      }
      SECTION("active element is error") {
        REQUIRE(sut.has_error());
      }
      SECTION("error is copied") {
        REQUIRE(sut == value);
      }
    }
  }
}

TEST_CASE("expected<T,E>::operator=(unexpected<E2>&&)", "[assign]") {
  SECTION("E cant be constructed or assigned from E2") {
    SECTION("Expected cannot be assigned from E2") {
      using copy_type = unexpected<not_copy_or_moveable>;
      using sut_type  = expected<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }
  SECTION("E is not nothrow move constructible from E2") {
    SECTION("Expected is not move-assignable") {
      using copy_type = unexpected<const char*>;
      using sut_type = expected<int,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E is not nothrow move constructible from E2, but is nothrow-move constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = unexpected<const char*>;
      using sut_type = expected<int,std::string>;

      // This works because it generates an intermediate 'expected<int, E>' in
      // between. The expected is then move-constructed instead -- which is
      // non-throwing.

      STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E can be constructed and assigned from E2") {
    SECTION("active element is T") {
      using copy_type = unexpected<std::string>;
      using sut_type = expected<report_destructor, move_only<std::string>>;

      auto value = copy_type{"hello world"};
      const auto copy = value;

      auto is_invoked = false;
      sut_type sut{&is_invoked};

      sut = std::move(value);

      SECTION("Expected can be assigned") {
        STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
      }
      SECTION("Calls 'T's destructor first") {
        REQUIRE(is_invoked);
      }
      SECTION("active element is error") {
        REQUIRE(sut.has_error());
      }
      SECTION("error is copied") {
        REQUIRE(sut == copy);
      }
    }

    SECTION("active element is E") {
      using copy_type = unexpected<std::string>;
      using sut_type = expected<int, move_only<std::string>>;

      auto value = copy_type{"goodbye world"};
      const auto copy = value;
      sut_type sut{copy_type{"hello world"}};

      sut = std::move(value);

      SECTION("Expected can be assigned") {
        STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
      }
      SECTION("active element is error") {
        REQUIRE(sut.has_error());
      }
      SECTION("error is copied") {
        REQUIRE(sut == copy);
      }
    }
  }
}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

TEST_CASE("expected<T,E>::operator->()", "[observers]") {
  auto sut = expected<int,int>{42};

  SECTION("Returns pointer to internal structure") {
    REQUIRE(&*sut == sut.operator->());
  }

  SECTION("Returns a mutable pointer") {
    STATIC_REQUIRE(std::is_same<decltype(sut.operator->()),int*>::value);
  }
}

TEST_CASE("expected<T,E>::operator->() const", "[observers]") {
  const auto sut = expected<int,int>{42};

  SECTION("Returns pointer to internal structure") {
    REQUIRE(&*sut == sut.operator->());
  }

  SECTION("Returns a const pointer") {
    STATIC_REQUIRE(std::is_same<decltype(sut.operator->()),const int*>::value);
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<T,E>::operator*() &", "[observers]") {
  auto sut = expected<int,int>{42};

  SECTION("Returns mutable lvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*sut),int&>::value);
  }
  SECTION("Returns reference to internal structure") {
    auto& x = sut.operator*();

    (void) x;
    SUCCEED();
  }
}

TEST_CASE("expected<T,E>::operator*() const &", "[observers]") {
  const auto sut = expected<int,int>{42};

  SECTION("Returns const lvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*sut),const int&>::value);
  }
  SECTION("Returns reference to internal structure") {
    auto& x = sut.operator*();

    (void) x;
    SUCCEED();
  }
}

TEST_CASE("expected<T,E>::operator*() &&", "[observers]") {
  auto sut = expected<int,int>{42};

  SECTION("Returns mutable rvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*std::move(sut)),int&&>::value);
  }
  SECTION("Returns reference to internal structure") {
    auto&& x = std::move(sut).operator*();

    (void) x;
    SUCCEED();
  }
}

TEST_CASE("expected<T,E>::operator*() const &&", "[observers]") {
  const auto sut = expected<int,int>{42};

  SECTION("Returns const rvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*std::move(sut)),const int&&>::value);
  }
  SECTION("Returns reference to internal structure") {
    auto&& x = std::move(sut).operator*();

    (void) x;
    SUCCEED();
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<T,E>::operator bool()", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Returns true") {
      auto sut = expected<int, int>{};

      REQUIRE(static_cast<bool>(sut));
    }
  }
  SECTION("expected does not contain a value") {
    SECTION("Returns false") {
      auto sut = expected<int, int>{make_unexpected(42)};

      REQUIRE_FALSE(static_cast<bool>(sut));
    }
  }
}

TEST_CASE("expected<T,E>::has_value()", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Returns true") {
      auto sut = expected<int, int>{};

      REQUIRE(sut.has_value());
    }
  }
  SECTION("expected does not contain a value") {
    SECTION("Returns false") {
      auto sut = expected<int, int>{make_unexpected(42)};

      REQUIRE_FALSE(sut.has_value());
    }
  }
}

TEST_CASE("expected<T,E>::has_error()", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Returns false") {
      auto sut = expected<int, int>{};

      REQUIRE_FALSE(sut.has_error());
    }
  }
  SECTION("expected does not contain a value") {
    SECTION("Returns true") {
      auto sut = expected<int, int>{make_unexpected(42)};

      REQUIRE(sut.has_error());
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<T,E>::value() &", "[observers]") {
  SECTION("expected contains a value") {
    auto sut = expected<int,std::error_code>{42};
    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(sut.value());
    }
    SECTION("Returns mutable lvalue reference to internal storage") {
      STATIC_REQUIRE(std::is_same<decltype(sut.value()),int&>::value);
    }
  }
  SECTION("expected contains an error") {
    SECTION("throws bad_expected_access") {
      auto sut = expected<int,int>{
        make_unexpected(42)
      };

      REQUIRE_THROWS_AS(sut.value(), bad_expected_access);
    }
  }
}

TEST_CASE("expected<T,E>::value() const &", "[observers]") {
  SECTION("expected contains a value") {
    const auto sut = expected<int,std::error_code>{42};
    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(sut.value());
    }
    SECTION("Returns const lvalue reference to internal storage") {
      STATIC_REQUIRE(std::is_same<decltype(sut.value()),const int&>::value);
    }
  }
  SECTION("expected contains an error") {
    SECTION("throws bad_expected_access") {
      const auto sut = expected<int,int>{
        make_unexpected(42)
      };

      REQUIRE_THROWS_AS(sut.value(), bad_expected_access);
    }
  }
}

TEST_CASE("expected<T,E>::value() &&", "[observers]") {
  SECTION("expected contains a value") {
    auto sut = expected<int,std::error_code>{42};
    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(std::move(sut).value());
    }
    SECTION("Returns mutable rvalue reference to internal storage") {
      STATIC_REQUIRE(std::is_same<decltype(std::move(sut).value()),int&&>::value);
    }
  }
  SECTION("expected contains an error") {
    SECTION("throws bad_expected_access") {
      auto sut = expected<int,int>{
        make_unexpected(42)
      };

      REQUIRE_THROWS_AS(std::move(sut).value(), bad_expected_access);
    }
  }
}

TEST_CASE("expected<T,E>::value() const &&", "[observers]") {
  SECTION("expected contains a value") {
    const auto sut = expected<int,std::error_code>{42};
    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(std::move(sut).value());
    }
    SECTION("Returns const rvalue reference to internal storage") {
      STATIC_REQUIRE(std::is_same<decltype(std::move(sut).value()),const int&&>::value);
    }
  }
  SECTION("expected contains an error") {
    SECTION("throws bad_expected_access") {
      const auto sut = expected<int,int>{
        make_unexpected(42)
      };

      REQUIRE_THROWS_AS(std::move(sut).value(), bad_expected_access);
    }
  }
}

//-----------------------------------------------------------------------------


TEST_CASE("expected<T,E>::error() const &", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Returns default-constructed error") {
      auto sut = expected<int, int>{};

      const auto result = sut.error();

      REQUIRE(result == int{});
    }
  }
  SECTION("expected contains an error") {
    SECTION("Returns a copy of the exception") {
      const auto value = 42;
      auto sut = expected<int, int>{make_unexpected(value)};

      const auto result = sut.error();

      REQUIRE(result == value);
    }
  }
}

TEST_CASE("expected<T,E>::error() &&", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Returns default-constructed error") {
      auto sut = expected<int, move_only<std::error_code>>{};

      const auto result = std::move(sut).error();

      REQUIRE(result == std::error_code{});
    }
  }
  SECTION("expected contains an error") {
    SECTION("Returns a copy of the exception") {
      const auto value = std::io_errc::stream;
      auto sut = expected<int, move_only<std::error_code>>{make_unexpected(value)};

      const auto result = std::move(sut).error();

      REQUIRE(result == value);
    }
  }
}
//-----------------------------------------------------------------------------
// Monadic Functionalities
//-----------------------------------------------------------------------------

TEST_CASE("expected<T,E>::error_or(U&&) const &", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Returns the input") {
      const auto input = std::error_code{};
      auto sut = expected<int, std::error_code>{42};

      const auto result = sut.error_or(input);

      REQUIRE(result == input);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Returns the error") {
      const auto input = std::error_code{std::io_errc::stream};
      auto sut = expected<void, std::error_code>{
        make_unexpected(input)
      };

      const auto result = sut.error_or(input);

      REQUIRE(result == input);
    }
  }
}

TEST_CASE("expected<T,E>::error_or(U&&) &&", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Returns the input") {
      auto input = std::error_code{};
      const auto copy = input;
      auto sut = expected<int, move_only<std::error_code>>{42};

      const auto result = std::move(sut).error_or(std::move(input));

      REQUIRE(result == copy);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Returns the error") {
      auto input = std::error_code{std::io_errc::stream};
      const auto copy = input;
      auto sut = expected<int, move_only<std::error_code>>{
        make_unexpected(input)
      };

      const auto result = std::move(sut).error_or(std::move(input));

      REQUIRE(result == copy);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<T,E>::and_then(U&&) const", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Maps the input") {
      const auto input = 42;
      auto sut = expected<int, std::error_code>{};

      const auto result = sut.and_then(input);

      REQUIRE(result == input);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto input = 42;
      const auto error = make_unexpected(std::io_errc::stream);
      auto sut = expected<int, std::error_code>{error};

      const auto result = sut.and_then(input);

      REQUIRE(result == error);
    }
  }
}

TEST_CASE("expected<T,E>::flat_map(Fn&&) const &", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Maps the input") {
      const auto value = 42;
      const auto sut = expected<int,std::error_code>{value};

      const auto result = sut.flat_map([](int x){
        return expected<std::string,std::error_code>{std::to_string(x)};
      });

      REQUIRE(result == std::to_string(value));
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected(std::io_errc::stream);
      const auto sut = expected<int,std::error_code>{error};

      const auto result = sut.flat_map([](int x){
        return expected<std::string,std::error_code>{std::to_string(x)};
      });

      REQUIRE(result == error);
    }
  }
}

TEST_CASE("expected<T,E>::flat_map(Fn&&) &&", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Maps the input") {
      const auto value = 42;
      auto sut = expected<int,move_only<std::error_code>>{value};

      const auto result = std::move(sut).flat_map([](int x){
        return expected<std::string,std::error_code>{std::to_string(x)};
      });

      REQUIRE(result == std::to_string(value));
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected(std::io_errc::stream);
      auto sut = expected<int,move_only<std::error_code>>{error};

      const auto result = std::move(sut).flat_map([](int x){
        return expected<std::string,std::error_code>{std::to_string(x)};
      });

      REQUIRE(result == error);
    }
  }
}

TEST_CASE("expected<T,E>::map(Fn&&) const &", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Function returns non-void") {
      SECTION("Maps the input") {
        const auto value = 42;
        auto sut = expected<int,std::io_errc>{value};

        const auto result = sut.map([](int x){
          return std::to_string(x);
        });

        REQUIRE(result == std::to_string(value));
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto value = 42;
        auto sut = expected<int,std::io_errc>{value};

        const auto result = sut.map([](int){});

        SECTION("Result has value") {
          REQUIRE(result.has_value());
        }
        SECTION("Result is expected<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(result),const expected<void,std::io_errc>>::value);
        }
      }
    }
  }
  SECTION("expected contains an error") {
    SECTION("Function returns non-void") {
      SECTION("Maps the error") {
        const auto error = make_unexpected(std::io_errc::stream);
        auto sut = expected<int,std::io_errc>{error};

        const auto result = sut.map([](int x){
          return std::to_string(x);
        });

        REQUIRE(result == error);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto error = make_unexpected(std::io_errc::stream);
        auto sut = expected<int,std::io_errc>{error};

        const auto result = sut.map([](int) -> void{});

        SECTION("Result contains error") {
          REQUIRE(result == error);
        }
        SECTION("Result is expected<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(result),const expected<void,std::io_errc>>::value);
        }
      }
    }
  }
}

TEST_CASE("expected<T,E>::map(Fn&&) &&", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Function returns non-void") {
      SECTION("Maps the input") {
        const auto value = 42;
        auto sut = expected<int,move_only<std::error_code>>{value};

        const auto result = std::move(sut).map([](int x){
          return std::to_string(x);
        });

        REQUIRE(result == std::to_string(value));
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto value = 42;
        auto sut = expected<int,move_only<std::error_code>>{value};

        const auto result = std::move(sut).map([](int){});

        SECTION("Result has value") {
          REQUIRE(result.has_value());
        }
        SECTION("Result is expected<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(result),const expected<void,move_only<std::error_code>>>::value);
        }
      }
    }
  }
  SECTION("expected contains an error") {
    SECTION("Function returns non-void") {
      SECTION("Maps the error") {
        const auto error = make_unexpected(std::io_errc::stream);
        auto sut = expected<int,move_only<std::error_code>>{error};

        const auto result = std::move(sut).map([&](int x){
          return std::to_string(x);
        });

        REQUIRE(result == error);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto error = make_unexpected(std::io_errc::stream);
        auto sut = expected<int,move_only<std::error_code>>{error};

        const auto result = std::move(sut).map([](int) -> void{});

        SECTION("Result contains the error") {
          REQUIRE(result == error);
        }
        SECTION("Result is expected<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(result),const expected<void,move_only<std::error_code>>>::value);
        }
      }
    }
  }
}

TEST_CASE("expected<T,E>::map_error(Fn&&) const &", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Maps the input") {
      const auto value = 42;
      auto sut = expected<int,std::io_errc>{value};

      const auto result = sut.map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(result == value);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected(std::io_errc::stream);
      auto sut = expected<int,std::io_errc>{error};

      const auto result = sut.map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(result == error);
    }
  }
}

TEST_CASE("expected<T,E>::map_error(Fn&&) &&", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Maps the input") {
      const auto value = "hello world";
      auto sut = expected<move_only<std::string>,std::io_errc>{value};

      const auto result = std::move(sut).map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(result == value);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected(std::io_errc::stream);
      auto sut = expected<move_only<std::string>,std::io_errc>{error};

      const auto result = std::move(sut).map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(result == error);
    }
  }
}

TEST_CASE("expected<T,E>::flat_map_error(Fn&&) const &", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Forwards underlying value") {
      const auto value = 42;
      const auto sut = expected<int,int>{value};

      const auto result = sut.flat_map_error([&](int x){
        return expected<long, int>{x};
      });

      REQUIRE(result == value);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected(42);
      const auto sut = expected<int, long>{error};

      const auto result = sut.flat_map_error([&](long x){
        return expected<int, short>{x};
      });

      REQUIRE(result == error.error());
    }
  }
}

TEST_CASE("expected<T,E>::flat_map_error(Fn&&) &&", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Default-initializes T") {
      const auto value = "hello world";
      auto sut = expected<move_only<std::string>,move_only<std::string>>{value};

      const auto result = std::move(sut).flat_map_error([](move_only<std::string>&& x){
        return expected<std::string, std::string>{in_place_error, std::move(x)};
      });

      REQUIRE(result == value);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected("Hello world");
      auto sut = expected<long,move_only<std::string>>{error};

      const auto result = std::move(sut).flat_map_error([](move_only<std::string>&& x){
        return expected<int, std::string>{in_place_error, std::move(x)};
      });

      REQUIRE(result == error);
    }
  }
}

//=============================================================================
// class : expected<T, E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Destructor / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("expected<T&,E>::expected()", "[ctor]") {
  SECTION("Expected containing a reference is not default-constructible") {
    using sut_type = expected<int&,int>;

    STATIC_REQUIRE_FALSE(std::is_default_constructible<sut_type>::value);
  }
}

TEST_CASE("expected<T&,E>::expected(const expected&)", "[ctor]") {
  using sut_type = expected<int&,int>;

  SECTION("Expected containing a reference is copy-constructible") {
    STATIC_REQUIRE(std::is_copy_constructible<sut_type>::value);
  }

  SECTION("Constructs an expected that references the source value") {
    auto value = int{42};

    const auto source = sut_type{value};
    const auto sut = source;

    SECTION("Refers to underlying input") {
      REQUIRE(&value == &(*sut));
    }
  }
}

TEST_CASE("expected<T&,E>::expected(expected&&)", "[ctor]") {
  using sut_type = expected<int&,move_only<std::string>>;

  SECTION("Expected containing a reference is copy-constructible") {
    STATIC_REQUIRE(std::is_move_constructible<sut_type>::value);
  }

  SECTION("Constructs an expected that references the source value") {
    auto value = int{42};

    auto source = sut_type{value};
    const auto sut = std::move(source);

    SECTION("Refers to underlying input") {
      REQUIRE(&value == &(*sut));
    }
  }
}

TEST_CASE("expected<T&,E>::expected(const expected<T2,E2>&)", "[ctor]") {
  SECTION("other's type holds T by-value") {
    SECTION("Constructor is disabled") {
      using from_type = expected<int,std::error_code>;
      using sut_type = expected<int&, std::error_code>;

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const from_type&>::value);
    }
  }

  SECTION("other's type holds T by reference") {
    using from_type = expected<derived&,int>;
    using sut_type = expected<base&,int>;

    SECTION("Expected containing a reference is copy-constructible") {
      STATIC_REQUIRE(std::is_constructible<sut_type, const from_type&>::value);
    }

    SECTION("Constructs an expected that references the source value") {
      const auto input = 10;
      auto value = derived{input};

      const auto source = from_type{value};
      const auto sut    = sut_type{source};

      SECTION("Refers to underlying input") {
        REQUIRE(&value == &(*sut));
      }
      SECTION("Reference works with hierarchies") {
        REQUIRE(sut->get_value() == input);
      }
    }
  }
}

TEST_CASE("expected<T&,E>::expected(expected<T2,E2>&&)", "[ctor]") {

  SECTION("other's type holds T by-value") {
    SECTION("Constructor is disabled") {
      using from_type = expected<int,move_only<std::string>>;
      using sut_type = expected<int&,move_only<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, from_type&&>::value);
    }
  }
  SECTION("other's type holds T by reference") {
    using from_type = expected<derived&,move_only<std::string>>;
    using sut_type = expected<base&,move_only<std::string>>;

    SECTION("Expected containing a reference is copy-constructible") {
      STATIC_REQUIRE(std::is_constructible<sut_type, from_type&&>::value);
    }

    SECTION("Constructs an expected that references the source value") {
      const auto input = 10;
      auto value = derived{input};

      auto source = from_type{value};
      const auto sut = sut_type{std::move(source)};

      SECTION("Refers to underlying input") {
        REQUIRE(&value == &(*sut));
      }
      SECTION("Reference works with hierarchies") {
        REQUIRE(sut->get_value() == input);
      }
    }
  }
}

TEST_CASE("expected<T&,E>::expected(in_place_t, Args&&...)", "[ctor]") {
  using sut_type = expected<base&,int>;

  auto value = derived{42};
  const sut_type sut{in_place, value};

  SECTION("Contains a value") {
    REQUIRE(sut.has_value());
  }

  SECTION("References old input") {
    REQUIRE(&value == &(*sut));
  }
}

TEST_CASE("expected<T&,E>::expected(const unexpected<E2>&)", "[ctor]") {
  using sut_type = expected<int&,std::string>;
  const auto source = make_unexpected<std::string>("hello world");

  const sut_type sut{source};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == source);
  }
}

TEST_CASE("expected<T&,E>::expected(unexpected<E2>&&)", "[ctor]") {
  using sut_type = expected<int&,move_only<std::string>>;
  auto source = make_unexpected<std::string>("hello world");
  const auto copy = source;

  const sut_type sut{std::move(source)};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from E2") {
    REQUIRE(sut == copy);
  }
}

TEST_CASE("expected<T&,E>::expected(U&&)", "[ctor]") {
  SECTION("Input is lvalue") {
    using sut_type = expected<base&,int>;

    auto value = derived{42};
    const sut_type sut{value};

    SECTION("Constructor is enabled") {
      REQUIRE(std::is_constructible<sut_type,derived&>::value);
    }

    SECTION("Contains a value") {
      REQUIRE(sut.has_value());
    }

    SECTION("References old input") {
      REQUIRE(&value == &(*sut));
    }
  }
  SECTION("Input is rvalue") {
    using sut_type = expected<base&,int>;

    SECTION("Constructor is disabled") {
      REQUIRE_FALSE(std::is_constructible<sut_type,derived&&>::value);
    }
  }
  SECTION("Input is value") {
    using sut_type = expected<base&,int>;

    SECTION("Constructor is disabled") {
      REQUIRE_FALSE(std::is_constructible<sut_type,derived>::value);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<T&,E>::operator=(const expected&)", "[assign]") {
  SECTION("expected contains a value") {
    SECTION("other contains a value") {
      auto value = derived{42};
      auto next = derived{0};

      const auto source = expected<base&,std::error_code>{next};
      auto sut = expected<base&,std::error_code>{value};

      sut = source;

      SECTION("Underlying reference is rebound") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
  SECTION("expected contains an error") {
    SECTION("other contains a value") {
      auto next = derived{0};

      const auto source = expected<base&,int>{next};
      auto sut = expected<base&,int>{make_unexpected(42)};

      sut = source;

      SECTION("Binds the new reference") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
}

TEST_CASE("expected<T&,E>::operator=(expected&&)", "[assign]") {
  SECTION("expected contains a value") {
    SECTION("other contains a value") {
      auto value = derived{42};
      auto next = derived{0};

      auto source = expected<base&,move_only<std::string>>{next};
      auto sut = expected<base&,move_only<std::string>>{value};

      sut = std::move(source);

      SECTION("Underlying reference is rebound") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
  SECTION("expected contains an error") {
    SECTION("other contains a value") {
      auto next = derived{0};

      auto source = expected<base&,move_only<std::string>>{next};
      auto sut = expected<base&,move_only<std::string>>{make_unexpected("foo")};

      sut = std::move(source);

      SECTION("Binds the new reference") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
}

TEST_CASE("expected<T&,E>::operator=(const expected<T2,E2>&)", "[assign]") {
  SECTION("other's type holds T by-value") {
    SECTION("Assignment is disabled") {
      using source_type = expected<int,std::error_code>;
      using sut_type = expected<int&,std::error_code>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const source_type&>::value);
    }
  }
  SECTION("expected contains a value") {
    SECTION("other contains a value") {
      auto value = derived{42};
      auto next = derived{0};

      const auto source = expected<derived&,std::error_code>{next};
      auto sut = expected<base&,std::error_code>{value};

      sut = source;

      SECTION("Underlying reference is rebound") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
  SECTION("expected contains an error") {
    SECTION("other contains a value") {
      auto next = derived{0};

      const auto source = expected<derived&,int>{next};
      auto sut = expected<base&,int>{make_unexpected(42)};

      sut = source;

      SECTION("Binds the new reference") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
}

TEST_CASE("expected<T&,E>::operator=(expected<T2,E2>&&)", "[assign]") {
  SECTION("other's type holds T by-value") {
    SECTION("Assignment is disabled") {
      using source_type = expected<int,move_only<std::string>>;
      using sut_type = expected<int&,move_only<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, source_type&&>::value);
    }
  }
  SECTION("expected contains a value") {
    SECTION("other contains a value") {
      auto value = derived{42};
      auto next = derived{0};

      auto source = expected<derived&,move_only<std::string>>{next};
      auto sut = expected<base&,move_only<std::string>>{value};

      sut = std::move(source);

      SECTION("Underlying reference is rebound") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
  SECTION("expected contains an error") {
    SECTION("other contains a value") {
      auto next = derived{0};

      auto source = expected<derived&,move_only<std::string>>{next};
      auto sut = expected<base&,move_only<std::string>>{make_unexpected("foo")};

      sut = std::move(source);

      SECTION("Binds the new reference") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
}

TEST_CASE("expected<T&,E>::operator=(U&&)", "[assign]") {
  SECTION("Source contains a value") {
    auto value = int{42};
    auto next = int{0};

    auto sut = expected<int&,std::error_code>{value};

    sut = next;

    SECTION("Rebinds the referred-to value") {
      REQUIRE(&next == &(*sut));
    }
  }
  SECTION("Source contains an error") {
    auto next = int{0};

    auto sut = expected<int&,int>{
      make_unexpected(42)
    };

    sut = next;

    SECTION("Expected contains a value") {
      REQUIRE(sut.has_value());
    }
    SECTION("Binds to the new reference") {
      REQUIRE(&next == &(*sut));
    }
  }
}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

TEST_CASE("expected<T&,E>::operator*() &", "[observers]") {
  auto value = int{42};
  auto sut = expected<int&,int>{value};

  SECTION("Returns mutable lvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*sut),int&>::value);
  }
  SECTION("Returns reference to referred value") {
    REQUIRE(&(*sut) == &value);
  }
}

TEST_CASE("expected<T&,E>::operator*() const &", "[observers]") {
  auto value = int{42};
  const auto sut = expected<int&,int>{value};

  SECTION("Returns lvalue reference that does not propagate constness") {
    STATIC_REQUIRE(std::is_same<decltype(*sut),int&>::value);
  }
  SECTION("Returns reference to referred value") {
    REQUIRE(&(*sut) == &value);
  }
}

TEST_CASE("expected<T&,E>::operator*() &&", "[observers]") {
  auto value = int{42};
  auto sut = expected<int&,int>{value};

  SECTION("Returns mutable lvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*std::move(sut)),int&>::value);
  }
  SECTION("Returns reference to referred value") {
    int& x = (*std::move(sut));
    REQUIRE(&x == &value);
  }
}

TEST_CASE("expected<T&,E>::operator*() const &&", "[observers]") {
  auto value = int{42};
  const auto sut = expected<int&,int>{value};

  SECTION("Returns mutable Lvalue reference that does not propagate constness") {
    STATIC_REQUIRE(std::is_same<decltype(*std::move(sut)),int&>::value);
  }
  SECTION("Returns reference to referred value") {
    const int& x = (*std::move(sut));
    REQUIRE(&x == &value);
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<T&,E>::value() &", "[observers]") {
  SECTION("expected contains a value") {
    auto value = int{42};
    auto sut = expected<int&,int>{value};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(sut.value());
    }
    SECTION("Returns mutable lvalue reference to internal storage") {
      STATIC_REQUIRE(std::is_same<decltype(sut.value()),int&>::value);
    }
    SECTION("Refers to referenced value") {
      int& x = sut.value();
      REQUIRE(&value == &x);
    }
  }
}

TEST_CASE("expected<T&,E>::value() const &", "[observers]") {
  SECTION("expected contains a value") {
    auto value = int{42};
    const auto sut = expected<int&,int>{value};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(sut.value());
    }
    SECTION("Returns mutable lvalue that does not propagate constness") {
      STATIC_REQUIRE(std::is_same<decltype(sut.value()),int&>::value);
    }
    SECTION("Refers to referenced value") {
      int& x = sut.value();
      REQUIRE(&value == &x);
    }
  }
}

TEST_CASE("expected<T&,E>::value() &&", "[observers]") {
  SECTION("expected contains a value") {
    auto value = int{42};
    auto sut = expected<int&,int>{value};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(std::move(sut).value());
    }
    SECTION("Returns mutable lvalue") {
      STATIC_REQUIRE(std::is_same<decltype(std::move(sut).value()),int&>::value);
    }
    SECTION("Refers to referenced value") {
      int& x = std::move(sut).value();
      REQUIRE(&value == &x);
    }
  }
}

TEST_CASE("expected<T&,E>::value() const &&", "[observers]") {
  SECTION("expected contains a value") {
    auto value = int{42};
    const auto sut = expected<int&,int>{value};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(std::move(sut).value());
    }
    SECTION("Returns mutable lvalue that does not propagate constness") {
      STATIC_REQUIRE(std::is_same<decltype(std::move(sut).value()),int&>::value);
    }
    SECTION("Refers to referenced value") {
      int& x = std::move(sut).value();
      REQUIRE(&value == &x);
    }
  }
}

//=============================================================================
// class : expected<void, E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Destructor / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("expected<void,E>::expected()", "[ctor]") {
  SECTION("Expected is default-constructible") {
    using sut_type = expected<void,int>;

    STATIC_REQUIRE(std::is_default_constructible<sut_type>::value);
  }

  SECTION("Constructs an expected with a 'value' state") {
    using sut_type = expected<void,int>;

    auto sut = sut_type{};

    REQUIRE(sut.has_value());
  }
}

TEST_CASE("expected<void,E>::expected(const expected&)", "[ctor]") {

  // Constructible:

  SECTION("E is trivially copy-constructible") {
    using sut_type = expected<void,int>;

    SECTION("Expected is trivially copy-constructible") {
      STATIC_REQUIRE(std::is_trivially_copy_constructible<sut_type>::value);
    }

    SECTION("Copy source contained a value") {
      const sut_type source{};

      const auto sut = source;

      SECTION("Copy contains a value") {
        REQUIRE(sut.has_value());
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = make_unexpected(42);
      const sut_type source{error};

      const auto sut = source;

      SECTION("Copy contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("Copy contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  } // T is trivially copy-constructible

  SECTION("E is copy-constructible, but not trivial") {
    using sut_type = expected<void,std::string>;

    SECTION("Expected is not trivially copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_trivially_copy_constructible<sut_type>::value);
    }

    SECTION("Expected is copy-constructible") {
      STATIC_REQUIRE(std::is_copy_constructible<sut_type>::value);
    }

    SECTION("Copy source contained a value") {
      const sut_type source{};

      const auto sut = source;

      SECTION("Copy contains a value") {
        REQUIRE(sut.has_value());
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = make_unexpected("Hello world");
      const sut_type source{error};

      const auto sut = source;

      SECTION("Copy contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("Copy contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  // Not constructible:

  SECTION("E is not copy-constructible") {
    using sut_type = expected<void,not_copy_or_moveable>;

    SECTION("Expected is not copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_copy_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("expected<void,E>::expected(expected&&)", "[ctor]") {

  // Constructible:

  SECTION("E is trivially move-constructible") {
    using sut_type = expected<void,int>;

    SECTION("Expected is trivially move-constructible") {
      STATIC_REQUIRE(std::is_trivially_move_constructible<sut_type>::value);
    }

    SECTION("Move source contained a value") {
      sut_type source{};

      const auto sut = std::move(source);

      SECTION("New expected contains a value") {
        REQUIRE(sut.has_value());
      }
    }

    SECTION("Move source contained an error") {
      const auto error = make_unexpected(42);
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New expected contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New expected contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  } // E is trivially move-constructible

  SECTION("E is move-constructible, but not trivial") {
    using sut_type = expected<void,move_only<std::string>>;

    SECTION("Expected is not trivially move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_trivially_move_constructible<sut_type>::value);
    }

    SECTION("Expected is move-constructible") {
      STATIC_REQUIRE(std::is_move_constructible<sut_type>::value);
    }

    SECTION("Move source contained a value") {
      sut_type source{};

      const auto sut = std::move(source);

      SECTION("New expected contains a value") {
        REQUIRE(sut.has_value());
      }
    }

    SECTION("Move source contained an error") {
      const auto error = make_unexpected("Hello world");
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New expected contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New expected contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  // Not constructible:

  SECTION("E is not move-constructible") {
    using sut_type = expected<void,not_copy_or_moveable>;

    SECTION("Expected is not move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_move_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("expected<void,E>::expected(const expected<U,E2>&)", "[ctor]") {
  SECTION("E2 is convertible to E") {
    SECTION("Constructor is enabled") {
      STATIC_REQUIRE(std::is_constructible<expected<void,int>,const expected<void,long>&>::value);
    }
    SECTION("Other contains value") {
      const auto other = expected<int,long>{42};
      const auto sut = expected<void,int>{other};

      SECTION("Constructed expected is in a value state") {
        REQUIRE(sut.has_value());
      }
    }

    SECTION("Other contains error") {
      const auto other = expected<int,long>{
        make_unexpected(42)
      };
      const auto sut = expected<void,int>{other};
      SECTION("Constructed expected is in an error state") {
        REQUIRE(sut.has_error());
      }
      SECTION("Contained error is converted from other") {
        REQUIRE(sut.error() == other.error());
      }
    }
  }

  SECTION("E2 is not convertible to E") {
    SECTION("Constructor is disabled") {
      STATIC_REQUIRE_FALSE(std::is_constructible<expected<void,int>,const expected<void,std::string>&>::value);
    }
  }
}

TEST_CASE("expected<void,E>::expected(expected<U,E2>&&)", "[ctor]") {
  SECTION("E2 is convertible to E") {
    SECTION("Constructor is enabled") {
      STATIC_REQUIRE(std::is_constructible<expected<void,int>,expected<void,long>&&>::value);
    }
    SECTION("Other contains value") {
      auto other = expected<int,long>{42};
      auto sut = expected<void,int>{std::move(other)};

      SECTION("Constructed expected is in a value state") {
        REQUIRE(sut.has_value());
      }
    }

    SECTION("Other contains error") {
      auto error = make_unexpected("Hello world");
      auto other = expected<int,move_only<std::string>>{error};
      auto sut = expected<void,std::string>{
        std::move(other)
      };

      SECTION("Constructed expected is in an error state") {
        REQUIRE(sut.has_error());
      }
      SECTION("Contained error is converted from other") {
        REQUIRE(sut == error);
      }
    }
  }

  SECTION("E2 is not convertible to E") {
    SECTION("Constructor is disabled") {
      STATIC_REQUIRE_FALSE(std::is_constructible<expected<void,int>,expected<void,std::string>&&>::value);
    }
  }
}

TEST_CASE("expected<void,E>::expected(in_place_error_t, Args&&...)", "[ctor]") {
  using sut_type = expected<void,std::string>;

  const auto input = "hello world";
  const auto size = 5;
  const auto expected = std::string{input, size};

  const sut_type sut(in_place_error, input, size);

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == make_unexpected(std::ref(expected)));
  }
}

TEST_CASE("expected<void,E>::expected(in_place_error_t, std::initializer_list<U>, Args&&...)", "[ctor]") {
  using sut_type = expected<void,std::string>;

  const auto expected = std::string{
    {'h','e','l','l','o'}, std::allocator<char>{}
  };

  const sut_type sut(in_place_error, {'h','e','l','l','o'}, std::allocator<char>{});

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == make_unexpected(std::ref(expected)));
  }
}

TEST_CASE("expected<void,E>::expected(const unexpected<E2>&)", "[ctor]") {
  using sut_type = expected<void,std::string>;
  const auto source = make_unexpected<std::string>("hello world");

  const sut_type sut{source};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == source);
  }
}

TEST_CASE("expected<void,E>::expected(unexpected<E2>&&)", "[ctor]") {
  using sut_type = expected<void,move_only<std::string>>;
  auto source = make_unexpected<std::string>("hello world");
  const auto copy = source;

  const sut_type sut{std::move(source)};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from E2") {
    REQUIRE(sut == copy);
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<void,E>::~expected()", "[dtor]") {
  SECTION("E is trivially destructible") {
    using sut_type = expected<void, int>;

    SECTION("expected's destructor is trivial") {
      STATIC_REQUIRE(std::is_trivially_destructible<sut_type>::value);
    }
  }

  SECTION("E is not trivially destructible") {
    using sut_type = expected<void, report_destructor>;

    auto invoked = false;
    {
      sut_type sut{in_place_error, &invoked};
      (void) sut;
    }

    SECTION("expected's destructor is not trivial") {
      STATIC_REQUIRE_FALSE(std::is_trivially_destructible<sut_type>::value);
    }

    SECTION("Invokes E's underlying destructor") {
      REQUIRE(invoked);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<void,E>::operator=(const expected& other)", "[assign]") {
  SECTION("E is not nothrow copy constructible") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = expected<void,std::string>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }

  SECTION("E is not copy-assignable") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = expected<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }

  SECTION("E is nothrow copy-constructible") {
    using sut_type = expected<void,std::error_code>;

    SECTION("Expected is copy-assignable") {
      STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
    }
    SECTION("Copies the state of 'other'") {
      SECTION("Original contains value") {
        const auto result = make_unexpected(std::io_errc::stream);
        const sut_type copy{result};
        sut_type sut{};

        sut = copy;

        SECTION("Result changes to error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result constructs error") {
          REQUIRE(copy == sut);
        }
      }
      SECTION("Original contains error") {
        const auto result = make_unexpected(std::io_errc::stream);
        const sut_type copy{result};
        sut_type sut{make_unexpected(std::io_errc{})};

        sut = copy;

        SECTION("Result has error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result assigns error") {
          REQUIRE(copy == sut);
        }
      }
    }
  }

  SECTION("E is trivial copy-constructible and copy-assignable") {
    using sut_type = expected<void, int>;

    SECTION("Expected is copy-assignable") {
      STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
    }
    SECTION("Expected is trivial copy-assignable") {
      STATIC_REQUIRE(std::is_trivially_copy_assignable<sut_type>::value);
    }
  }
}

TEST_CASE("expected<void,E>::operator=(expected&& other)", "[assign]") {
  SECTION("E is not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using sut_type = expected<void,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }

  SECTION("E is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using sut_type = expected<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }

  SECTION("E is nothrow move-constructible") {
    using sut_type = expected<void,move_only<std::error_code>>;

    SECTION("Expected is move-assignable") {
      STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
    }
    SECTION("Copies the state of 'other'") {
      SECTION("Original contains value") {
        const auto result = make_unexpected(std::io_errc::stream);
        sut_type other{result};
        sut_type sut{};

        sut = std::move(other);

        SECTION("Result changes to error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result constructs error") {
          REQUIRE(result == sut);
        }
      }
      SECTION("Original contains error") {
        const auto result = make_unexpected(std::io_errc::stream);
        sut_type other{result};
        sut_type sut{make_unexpected(std::io_errc{})};

        sut = std::move(other);

        SECTION("Result has error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result assigns error") {
          REQUIRE(result == sut);
        }
      }
    }
  }

  SECTION("E is trivial move-constructible and move-assignable") {
    using sut_type = expected<void, int>;

    SECTION("Expected is move-assignable") {
      STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
    }
    SECTION("Expected is trivial move-assignable") {
      STATIC_REQUIRE(std::is_trivially_move_assignable<sut_type>::value);
    }
  }
}

TEST_CASE("expected<void,E>::operator=(const expected<T2,E2>&)", "[assign]") {
  SECTION("E is not nothrow copy constructible") {
    SECTION("Expected is not copy-assignable") {
      using copy_type = expected<int,const char*>;
      using sut_type = expected<void,std::string>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E is not copy-assignable") {
    SECTION("Expected is not copy-assignable") {
      using copy_type = expected<int,const char*>;
      using sut_type = expected<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E is nothrow copy-constructible") {
    using copy_type = expected<int,std::io_errc>;
    using sut_type = expected<void,std::error_code>;

    SECTION("Expected is copy-assignable") {
      STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
    }
    SECTION("Copies the state of 'other'") {
      SECTION("Original contains value") {
        const auto result = make_unexpected(std::io_errc::stream);
        const copy_type other{result};
        sut_type sut{};

        sut = other;

        SECTION("Result changes to error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result constructs error") {
          REQUIRE(result == sut);
        }
      }
      SECTION("Original contains error") {
        const auto result = make_unexpected(std::io_errc::stream);
        const sut_type other{result};
        sut_type sut{make_unexpected(std::io_errc{})};

        sut = other;

        SECTION("Result has error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result assigns error") {
          REQUIRE(result == sut);
        }
      }
    }
  }
}

TEST_CASE("expected<void,E>::operator=(expected<T2,E2>&&)", "[assign]") {
  SECTION("E is not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using copy_type = expected<int,const char*>;
      using sut_type = expected<void,std::string>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = expected<int,const char*>;
      using sut_type = expected<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E is nothrow move-constructible") {
    using copy_type = expected<int,std::io_errc>;
    using sut_type = expected<void,move_only<std::error_code>>;

    SECTION("Expected is move-assignable") {
      STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
    }
    SECTION("Copies the state of 'other'") {
      SECTION("Original contains value") {
        const auto result = make_unexpected(std::io_errc::stream);
        copy_type other{result};
        sut_type sut{};

        sut = std::move(other);

        SECTION("Result changes to error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result constructs error") {
          REQUIRE(result == sut);
        }
      }
      SECTION("Original contains error") {
        const auto result = make_unexpected(std::io_errc::stream);
        sut_type other{result};
        sut_type sut{make_unexpected(std::io_errc{})};

        sut = std::move(other);

        SECTION("Result has error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result assigns error") {
          REQUIRE(result == sut);
        }
      }
    }
  }
}

TEST_CASE("expected<void,E>::operator=(const unexpected<E2>&)", "[assign]") {
  SECTION("E cant be constructed or assigned from E2") {
    SECTION("Expected cannot be assigned from E2") {
      using copy_type = unexpected<not_copy_or_moveable>;
      using sut_type  = expected<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }
  SECTION("E is not nothrow move constructible from E2") {
    SECTION("Expected is not move-assignable") {
      using copy_type = unexpected<const char*>;
      using sut_type = expected<void,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E is not nothrow move constructible from E2, but is nothrow-move constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = unexpected<const char*>;
      using sut_type = expected<void,std::string>;

      // This works because it generates an intermediate 'expected<void, E>' in
      // between. The expected is then move-assigned instead

      STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E can be constructed and assigned from E2") {
    using copy_type = unexpected<long>;
    using sut_type = expected<void, int>;

    const auto original = copy_type{42};
    sut_type sut{};

    sut = original;

    SECTION("Expected can be assigned") {
      STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
    }
    SECTION("Contains an error") {
      REQUIRE(sut.has_error());
    }
    SECTION("Error is changed to input") {
      REQUIRE(sut == original);
    }
  }
}

TEST_CASE("expected<void,E>::operator=(unexpected<E2>&&)", "[assign]") {
  SECTION("E cant be constructed or assigned from E2") {
    SECTION("Expected cannot be assigned from E2") {
      using copy_type = unexpected<not_copy_or_moveable>;
      using sut_type  = expected<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }
  SECTION("E is not nothrow move constructible from E2") {
    SECTION("Expected is not move-assignable") {
      using copy_type = unexpected<const char*>;
      using sut_type = expected<void,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E is not nothrow move constructible from E2, but is nothrow-move constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = unexpected<const char*>;
      using sut_type = expected<void,std::string>;

      // This works because it generates an intermediate 'expected<void, E>' in
      // between. The expected is then move-assigned instead

      STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E can be constructed and assigned from E2") {
    using copy_type = unexpected<long>;
    using sut_type = expected<void, int>;

    auto other = copy_type{42};
    const auto original = other;
    sut_type sut{};

    sut = std::move(other);

    SECTION("Expected can be assigned") {
      STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
    }
    SECTION("Contains an error") {
      REQUIRE(sut.has_error());
    }
    SECTION("Error is changed to input") {
      REQUIRE(sut == original);
    }
  }
}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

TEST_CASE("expected<void,E>::operator bool()", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Returns true") {
      auto sut = expected<void, int>{};

      REQUIRE(static_cast<bool>(sut));
    }
  }
  SECTION("expected does not contain a value") {
    SECTION("Returns false") {
      auto sut = expected<void, int>{make_unexpected(42)};

      REQUIRE_FALSE(static_cast<bool>(sut));
    }
  }
}

TEST_CASE("expected<void,E>::has_value()", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Returns true") {
      auto sut = expected<void, int>{};

      REQUIRE(sut.has_value());
    }
  }
  SECTION("expected does not contain a value") {
    SECTION("Returns false") {
      auto sut = expected<void, int>{make_unexpected(42)};

      REQUIRE_FALSE(sut.has_value());
    }
  }
}

TEST_CASE("expected<void,E>::has_error()", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Returns false") {
      auto sut = expected<void, int>{};

      REQUIRE_FALSE(sut.has_error());
    }
  }
  SECTION("expected does not contain a value") {
    SECTION("Returns true") {
      auto sut = expected<void, int>{make_unexpected(42)};

      REQUIRE(sut.has_error());
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<void,E>::value()", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Does nothing") {
      auto sut = expected<void, int>{};

      sut.value();

      SUCCEED();
    }
  }
  SECTION("expected contains an error") {
    SECTION("Throws bad_expected_access") {
      auto sut = expected<void, int>{make_unexpected(42)};

      REQUIRE_THROWS_AS(sut.value(), bad_expected_access);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<void,E>::error() const &", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Returns default-constructed error") {
      auto sut = expected<void, int>{};

      const auto result = sut.error();

      REQUIRE(result == int{});
    }
  }
  SECTION("expected contains an error") {
    SECTION("Returns a copy of the exception") {
      const auto value = 42;
      auto sut = expected<void, int>{make_unexpected(value)};

      const auto result = sut.error();

      REQUIRE(result == value);
    }
  }
}

TEST_CASE("expected<void,E>::error() &&", "[observers]") {
  SECTION("expected contains a value") {
    SECTION("Returns default-constructed error") {
      auto sut = expected<void, move_only<std::error_code>>{};

      const auto result = std::move(sut).error();

      REQUIRE(result == std::error_code{});
    }
  }
  SECTION("expected contains an error") {
    SECTION("Returns a copy of the exception") {
      const auto value = std::io_errc::stream;
      auto sut = expected<void, move_only<std::error_code>>{make_unexpected(value)};

      const auto result = std::move(sut).error();

      REQUIRE(result == value);
    }
  }
}

//-----------------------------------------------------------------------------
// Monadic Functionalities
//-----------------------------------------------------------------------------

TEST_CASE("expected<void,E>::error_or(U&&) const &", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Returns the input") {
      const auto input = std::error_code{};
      auto sut = expected<void, std::error_code>{};

      const auto result = sut.error_or(input);

      REQUIRE(result == input);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Returns the error") {
      const auto input = std::error_code{std::io_errc::stream};
      auto sut = expected<void, std::error_code>{
        make_unexpected(input)
      };

      const auto result = sut.error_or(input);

      REQUIRE(result == input);
    }
  }
}

TEST_CASE("expected<void,E>::error_or(U&&) &&", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Returns the input") {
      auto input = std::error_code{};
      const auto copy = input;
      auto sut = expected<void, move_only<std::error_code>>{};

      const auto result = std::move(sut).error_or(std::move(input));

      REQUIRE(result == copy);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Returns the error") {
      auto input = std::error_code{std::io_errc::stream};
      const auto copy = input;
      auto sut = expected<void, move_only<std::error_code>>{
        make_unexpected(input)
      };

      const auto result = std::move(sut).error_or(std::move(input));

      REQUIRE(result == copy);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("expected<void,E>::and_then(U&&) const", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Maps the input") {
      const auto input = 42;
      auto sut = expected<void, std::error_code>{};

      const auto result = sut.and_then(input);

      REQUIRE(result == input);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto input = 42;
      const auto error = make_unexpected(std::io_errc::stream);
      auto sut = expected<void, std::error_code>{error};

      const auto result = sut.and_then(input);

      REQUIRE(result == error);
    }
  }
}

TEST_CASE("expected<void,E>::flat_map(Fn&&) const &", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Maps the input") {
      const auto value = 42;
      const auto sut = expected<void,std::error_code>{};

      const auto result = sut.flat_map([&]{
        return expected<int,std::error_code>{value};
      });

      REQUIRE(result == value);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected(std::io_errc::stream);
      const auto value = 42;
      const auto sut = expected<void,std::error_code>{error};

      const auto result = sut.flat_map([&]{
        return expected<int,std::error_code>{value};
      });

      REQUIRE(result == error);
    }
  }
}

TEST_CASE("expected<void,E>::flat_map(Fn&&) &&", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Maps the input") {
      const auto value = 42;
      auto sut = expected<void,move_only<std::error_code>>{};

      const auto result = std::move(sut).flat_map([&]{
        return expected<int,std::error_code>{value};
      });

      REQUIRE(result == value);
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected(std::io_errc::stream);
      const auto value = 42;
      auto sut = expected<void,move_only<std::error_code>>{error};

      const auto result = std::move(sut).flat_map([&]{
        return expected<int,std::error_code>{value};
      });

      REQUIRE(result == error);
    }
  }
}

TEST_CASE("expected<void,E>::map(Fn&&) const &", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Function returns non-void") {
      SECTION("Maps the input") {
        const auto value = 42;
        auto sut = expected<void,std::io_errc>{};

        const auto result = sut.map([&]{
          return value;
        });

        REQUIRE(result == value);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        auto sut = expected<void,std::io_errc>{};

        const auto result = sut.map([]{});

        SECTION("Result has value") {
          REQUIRE(result.has_value());
        }
        SECTION("Result is expected<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(result),const expected<void,std::io_errc>>::value);
        }
      }
    }
  }
  SECTION("expected contains an error") {
    SECTION("Function returns non-void") {
      SECTION("Maps the error") {
        const auto value = 42;
        const auto error = make_unexpected(std::io_errc::stream);
        auto sut = expected<void,std::io_errc>{error};

        const auto result = sut.map([&]{
          return value;
        });

        REQUIRE(result == error);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto error = make_unexpected(std::io_errc::stream);
        auto sut = expected<void,std::io_errc>{error};

        const auto result = sut.map([]{});

        SECTION("Result contains the error") {
          REQUIRE(result == error);
        }
        SECTION("Result is expected<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(result),const expected<void,std::io_errc>>::value);
        }
      }
    }
  }
}

TEST_CASE("expected<void,E>::map(Fn&&) &&", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Function returns non-void") {
      SECTION("Maps the input") {
        const auto value = 42;
        auto sut = expected<void,move_only<std::error_code>>{};

        const auto result = std::move(sut).map([&]{
          return value;
        });

        REQUIRE(result == value);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        auto sut = expected<void,move_only<std::error_code>>{};

        const auto result = std::move(sut).map([]{});

        SECTION("Result has value") {
          REQUIRE(result.has_value());
        }
        SECTION("Result is expected<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(result),const expected<void,move_only<std::error_code>>>::value);
        }
      }
    }
  }
  SECTION("expected contains an error") {
    SECTION("Function returns non-void") {
      SECTION("Maps the error") {
        const auto value = 42;
        const auto error = make_unexpected(std::io_errc::stream);
        auto sut = expected<void,move_only<std::error_code>>{error};

        const auto result = std::move(sut).map([&]{
          return value;
        });

        REQUIRE(result == error);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto error = make_unexpected(std::io_errc::stream);
        auto sut = expected<void,move_only<std::error_code>>{error};

        const auto result = std::move(sut).map([]{});

        SECTION("Result contains the error") {
          REQUIRE(result == error);
        }
        SECTION("Result is expected<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(result),const expected<void,move_only<std::error_code>>>::value);
        }
      }
    }
  }
}

TEST_CASE("expected<void,E>::map_error(Fn&&) const", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Maps the input") {
      auto sut = expected<void,std::io_errc>{};

      const auto result = sut.map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(result.has_value());
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected(std::io_errc::stream);
      auto sut = expected<void,std::io_errc>{error};

      const auto result = sut.map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(result == error);
    }
  }
}

TEST_CASE("expected<void,E>::flat_map_error(Fn&&) const &", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Default-initializes T") {
      const auto sut = expected<void,int>{};

      const auto result = sut.flat_map_error([&](int x){
        return expected<long, int>{x};
      });

      REQUIRE(result == long{});
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected(42);
      const auto sut = expected<void, long>{error};

      const auto result = sut.flat_map_error([&](long x){
        return expected<int, short>{x};
      });

      REQUIRE(result == error.error());
    }
  }
}

TEST_CASE("expected<void,E>::flat_map_error(Fn&&) &&", "[monadic]") {
  SECTION("expected contains a value") {
    SECTION("Default-initializes T") {
      auto sut = expected<void,move_only<std::string>>{};

      const auto result = std::move(sut).flat_map_error([](move_only<std::string>&& x){
        return expected<int, std::string>{in_place_error, std::move(x)};
      });

      REQUIRE(result == int{});
    }
  }
  SECTION("expected contains an error") {
    SECTION("Maps the error") {
      const auto error = make_unexpected("Hello world");
      auto sut = expected<void,move_only<std::string>>{error};

      const auto result = std::move(sut).flat_map_error([](move_only<std::string>&& x){
        return expected<int, std::string>{in_place_error, std::move(x)};
      });

      REQUIRE(result == error);
    }
  }
}

//=============================================================================
// non-member functions : class : expected<void, E>
//=============================================================================

//-----------------------------------------------------------------------------
// Comparisons
//-----------------------------------------------------------------------------

TEST_CASE("operator==(const expected<T1,E1>&, const expected<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left equal to right") {
      const auto lhs = expected<int,int>{42};
      const auto rhs = expected<long,long>{42};

      SECTION("returns true") {
        REQUIRE(lhs == rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = expected<int,int>{42};
      const auto rhs = expected<long,long>{0};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left equal to right") {
      const auto lhs = expected<int,int>{
        make_unexpected(42)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs == rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = expected<int,int>{
        make_unexpected(42)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(0)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<int,int>{};
    const auto rhs = expected<long,long>{
      make_unexpected(42)
    };
    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<int,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<long,long>{};
    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
}

TEST_CASE("operator!=(const expected<T1,E1>&, const expected<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left equal to right") {
      const auto lhs = expected<int,int>{42};
      const auto rhs = expected<long,long>{42};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = expected<int,int>{42};
      const auto rhs = expected<long,long>{0};

      SECTION("returns true") {
        REQUIRE(lhs != rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left equal to right") {
      const auto lhs = expected<int,int>{
        make_unexpected(42)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = expected<int,int>{
        make_unexpected(42)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(0)
      };
      SECTION("returns true") {
        REQUIRE(lhs != rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<int,int>{};
    const auto rhs = expected<long,long>{
      make_unexpected(42)
    };
    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<int,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<long,long>{};
    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
}

TEST_CASE("operator>=(const expected<T1,E1>&, const expected<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left greater than right") {
      const auto lhs = expected<int,int>{100};
      const auto rhs = expected<long,long>{42};

      SECTION("returns true") {
        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<int,int>{0};
      const auto rhs = expected<long,long>{42};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = expected<int,int>{
        make_unexpected(100)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(42)
      };

      SECTION("returns true") {
        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<int,int>{
        make_unexpected(0)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(42)
      };

      SECTION("returns false") {
        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<int,int>{0};
    const auto rhs = expected<long,long>{
      make_unexpected(42)
    };

    SECTION("returns true") {
      REQUIRE(lhs >= rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<int,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<long,long>{0};

    SECTION("returns false") {
      REQUIRE_FALSE(lhs >= rhs);
    }
  }
}

TEST_CASE("operator<=(const expected<T1,E1>&, const expected<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left greater than right") {
      const auto lhs = expected<int,int>{100};
      const auto rhs = expected<long,long>{42};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs <= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<int,int>{0};
      const auto rhs = expected<long,long>{42};

      SECTION("returns true") {
        REQUIRE(lhs <= rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = expected<int,int>{
        make_unexpected(100)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(42)
      };

      SECTION("returns false") {
        REQUIRE_FALSE(lhs <= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<int,int>{
        make_unexpected(0)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(42)
      };

      SECTION("returns true") {
        REQUIRE(lhs <= rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<int,int>{0};
    const auto rhs = expected<long,long>{
      make_unexpected(42)
    };

    SECTION("returns false") {
      REQUIRE_FALSE(lhs <= rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<int,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<long,long>{0};

    SECTION("returns true") {
      REQUIRE(lhs <= rhs);
    }
  }
}

TEST_CASE("operator>(const expected<T1,E1>&, const expected<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left greater than right") {
      const auto lhs = expected<int,int>{100};
      const auto rhs = expected<long,long>{42};

      SECTION("returns true") {
        REQUIRE(lhs > rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<int,int>{0};
      const auto rhs = expected<long,long>{42};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = expected<int,int>{
        make_unexpected(100)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(42)
      };

      SECTION("returns true") {
        REQUIRE(lhs > rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<int,int>{
        make_unexpected(0)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(42)
      };

      SECTION("returns false") {
        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<int,int>{0};
    const auto rhs = expected<long,long>{
      make_unexpected(42)
    };

    SECTION("returns true") {
      REQUIRE(lhs > rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<int,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<long,long>{0};

    SECTION("returns false") {
      REQUIRE_FALSE(lhs > rhs);
    }
  }
}

TEST_CASE("operator<(const expected<T1,E1>&, const expected<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left greater than right") {
      const auto lhs = expected<int,int>{100};
      const auto rhs = expected<long,long>{42};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs < rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<int,int>{0};
      const auto rhs = expected<long,long>{42};

      SECTION("returns true") {
        REQUIRE(lhs < rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = expected<int,int>{
        make_unexpected(100)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(42)
      };

      SECTION("returns false") {
        REQUIRE_FALSE(lhs < rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<int,int>{
        make_unexpected(0)
      };
      const auto rhs = expected<long,long>{
        make_unexpected(42)
      };

      SECTION("returns true") {
        REQUIRE(lhs < rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<int,int>{0};
    const auto rhs = expected<long,long>{
      make_unexpected(42)
    };

    SECTION("returns false") {
      REQUIRE_FALSE(lhs < rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<int,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<long,long>{0};

    SECTION("returns true") {
      REQUIRE(lhs < rhs);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("operator==(const expected<void,E1>&, const expected<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{};

    SECTION("returns true") {
      REQUIRE(lhs == rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left equal to right") {
      const auto lhs = expected<void,int>{
        make_unexpected(42)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs == rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = expected<void,int>{
        make_unexpected(42)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(0)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{
      make_unexpected(42)
    };
    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<void,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<void,long>{};
    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
}

TEST_CASE("operator!=(const expected<void,E1>&, const expected<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{};

    SECTION("returns false") {
      REQUIRE_FALSE(lhs != rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left equal to right") {
      const auto lhs = expected<void,int>{
        make_unexpected(42)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = expected<void,int>{
        make_unexpected(42)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(0)
      };
      SECTION("returns true") {
        REQUIRE(lhs != rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{
      make_unexpected(42)
    };
    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<void,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<void,long>{};
    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
}

TEST_CASE("operator>=(const expected<void,E1>&, const expected<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{};

    SECTION("returns true") {
      REQUIRE(lhs >= rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = expected<void,int>{
        make_unexpected(100)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<void,int>{
        make_unexpected(0)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{
      make_unexpected(42)
    };
    SECTION("returns true") {
      REQUIRE(lhs >= rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<void,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<void,long>{};
    SECTION("returns false") {
      REQUIRE_FALSE(lhs >= rhs);
    }
  }
}

TEST_CASE("operator<=(const expected<void,E1>&, const expected<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{};

    SECTION("returns true") {
      REQUIRE(lhs <= rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = expected<void,int>{
        make_unexpected(100)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs <= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<void,int>{
        make_unexpected(0)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs <= rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{
      make_unexpected(42)
    };
    SECTION("returns false") {
      REQUIRE_FALSE(lhs <= rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<void,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<void,long>{};
    SECTION("returns true") {
      REQUIRE(lhs <= rhs);
    }
  }
}

TEST_CASE("operator>(const expected<void,E1>&, const expected<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{};

    SECTION("returns false") {
      REQUIRE_FALSE(lhs > rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = expected<void,int>{
        make_unexpected(100)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs > rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<void,int>{
        make_unexpected(0)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{
      make_unexpected(42)
    };
    SECTION("returns true") {
      REQUIRE(lhs > rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<void,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<void,long>{};
    SECTION("returns false") {
      REQUIRE_FALSE(lhs > rhs);
    }
  }
}

TEST_CASE("operator<(const expected<void,E1>&, const expected<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{};

    SECTION("returns false") {
      REQUIRE_FALSE(lhs < rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = expected<void,int>{
        make_unexpected(100)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs < rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = expected<void,int>{
        make_unexpected(0)
      };
      const auto rhs = expected<void,long>{
        make_unexpected(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs < rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = expected<void,int>{};
    const auto rhs = expected<void,long>{
      make_unexpected(42)
    };
    SECTION("returns false") {
      REQUIRE_FALSE(lhs < rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = expected<void,int>{
      make_unexpected(42)
    };
    const auto rhs = expected<void,long>{};
    SECTION("returns true") {
      REQUIRE(lhs < rhs);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("operator==(const expected<T1,E1>&, const U&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto lhs = expected<int,std::string>{0};

    SECTION("value compares equal") {
      const auto rhs = 0l;

      SECTION("returns true") {
        REQUIRE(lhs == rhs);
      }
    }
    SECTION("value compares unequal") {
      const auto rhs = 42l;

      SECTION("returns false") {
        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
  SECTION("expected contains an error") {
    const auto lhs = expected<int,std::string>{
      make_unexpected("0")
    };
    const auto rhs = 0l;

    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
}

TEST_CASE("operator==(const U&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto rhs = expected<int,std::string>{0};

    SECTION("value compares equal") {
      const auto lhs = 0l;

      SECTION("returns true") {
        REQUIRE(lhs == rhs);
      }
    }
    SECTION("value compares unequal") {
      const auto lhs = 42l;

      SECTION("returns false") {
        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
  SECTION("expected contains an error") {
    const auto rhs = expected<int,std::string>{
      make_unexpected("0")
    };
    const auto lhs = 0l;

    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
}

TEST_CASE("operator!=(const expected<T1,E1>&, const U&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto lhs = expected<int,std::string>{0};

    SECTION("value compares equal") {
      const auto rhs = 0l;

      SECTION("returns false") {
        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("value compares unequal") {
      const auto rhs = 42l;

      SECTION("returns true") {
        REQUIRE(lhs != rhs);
      }
    }
  }
  SECTION("expected contains an error") {
    const auto lhs = expected<int,std::string>{
      make_unexpected("0")
    };
    const auto rhs = 0l;

    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
}

TEST_CASE("operator!=(const U&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto rhs = expected<int,std::string>{0};

    SECTION("value compares equal") {
      const auto lhs = 0l;

      SECTION("returns false") {
        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("value compares unequal") {
      const auto lhs = 42l;

      SECTION("returns true") {
        REQUIRE(lhs != rhs);
      }
    }
  }
  SECTION("expected contains an error") {
    const auto rhs = expected<int,std::string>{
      make_unexpected("0")
    };
    const auto lhs = 0l;

    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
}

TEST_CASE("operator>=(const expected<T1,E1>&, const U&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto lhs = expected<int,std::string>{5};
    SECTION("value is greater or equal to expected") {
      SECTION("returns true") {
        const auto rhs = 0l;

        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("value is not greater or equal to expected") {
      SECTION("returns false") {
        const auto rhs = 9l;

        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns true") {
      const auto lhs = expected<int,std::string>{make_unexpected("hello world")};
      const auto rhs = 0l;

      REQUIRE(lhs >= rhs);
    }
  }
}

TEST_CASE("operator>=(const U&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto rhs = expected<int,std::string>{5};
    SECTION("value is greater or equal to expected") {
      SECTION("returns true") {
        const auto lhs = 0l;

        REQUIRE_FALSE(lhs >= rhs);
      }
    }
    SECTION("value is not greater or equal to expected") {
      SECTION("returns false") {
        const auto lhs = 9l;

        REQUIRE(lhs >= rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns true") {
      const auto rhs = expected<int,std::string>{make_unexpected("hello world")};
      const auto lhs = 0l;

      REQUIRE_FALSE(lhs >= rhs);
    }
  }
}

TEST_CASE("operator<=(const expected<T1,E1>&, const U&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto lhs = expected<int,std::string>{5};
    SECTION("value is less or equal to expected") {
      SECTION("returns true") {
        const auto rhs = 9l;

        REQUIRE(lhs <= rhs);
      }
    }
    SECTION("value is not less or equal to expected") {
      SECTION("returns false") {
        const auto rhs = 0l;

        REQUIRE_FALSE(lhs <= rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns false") {
      const auto lhs = expected<int,std::string>{make_unexpected("hello world")};
      const auto rhs = 9l;

      REQUIRE_FALSE(lhs <= rhs);
    }
  }
}

TEST_CASE("operator<=(const U&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto rhs = expected<int,std::string>{5};
    SECTION("value is less or equal to expected") {
      SECTION("returns true") {
        const auto lhs = 0l;

        REQUIRE(lhs <= rhs);
      }
    }
    SECTION("value is not less or equal to expected") {
      SECTION("returns false") {
        const auto lhs = 9l;

        REQUIRE_FALSE(lhs <= rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns true") {
      const auto rhs = expected<int,std::string>{make_unexpected("hello world")};
      const auto lhs = 9l;

      REQUIRE(lhs <= rhs);
    }
  }
}

TEST_CASE("operator>(const expected<T1,E1>&, const U&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto lhs = expected<int,std::string>{5};
    SECTION("value is greater or equal to expected") {
      SECTION("returns true") {
        const auto rhs = 0l;

        REQUIRE(lhs > rhs);
      }
    }
    SECTION("value is not greater or equal to expected") {
      SECTION("returns false") {
        const auto rhs = 9l;

        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns false") {
      const auto lhs = expected<int,std::string>{make_unexpected("hello world")};
      const auto rhs = 0l;

      REQUIRE_FALSE(lhs > rhs);
    }
  }
}

TEST_CASE("operator>(const U&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto rhs = expected<int,std::string>{5};
    SECTION("value is greater or equal to expected") {
      SECTION("returns false") {
        const auto lhs = 0l;

        REQUIRE_FALSE(lhs > rhs);
      }
    }
    SECTION("value is not greater or equal to expected") {
      SECTION("returns false") {
        const auto lhs = 9l;

        REQUIRE(lhs > rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns true") {
      const auto rhs = expected<int,std::string>{make_unexpected("hello world")};
      const auto lhs = 0l;

      REQUIRE(lhs > rhs);
    }
  }
}

TEST_CASE("operator<(const expected<T1,E1>&, const U&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto lhs = expected<int,std::string>{5};
    SECTION("value is less or equal to expected") {
      SECTION("returns true") {
        const auto rhs = 9l;

        REQUIRE(lhs < rhs);
      }
    }
    SECTION("value is not less or equal to expected") {
      SECTION("returns false") {
        const auto rhs = 0l;

        REQUIRE_FALSE(lhs < rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns false") {
      const auto lhs = expected<int,std::string>{make_unexpected("hello world")};
      const auto rhs = 9l;

      REQUIRE_FALSE(lhs < rhs);
    }
  }
}

TEST_CASE("operator<(const U&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains a value") {
    const auto rhs = expected<int,std::string>{5};
    SECTION("value is less or equal to expected") {
      SECTION("returns true") {
        const auto lhs = 0l;

        REQUIRE(lhs < rhs);
      }
    }
    SECTION("value is not less or equal to expected") {
      SECTION("returns false") {
        const auto lhs = 9l;

        REQUIRE_FALSE(lhs < rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns false") {
      const auto rhs = expected<int,std::string>{make_unexpected("hello world")};
      const auto lhs = 9l;

      REQUIRE(lhs < rhs);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("operator==(const expected<T1,E1>&, const unexpected<E>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns false") {
      const auto lhs = expected<int,std::string>{0};
      const auto rhs = make_unexpected("hello world");

      REQUIRE_FALSE(lhs == rhs);
    }
  }
  SECTION("expected contains error") {
    const auto lhs = expected<int,std::string>{
      make_unexpected("0")
    };
    SECTION("unexpected compares equal") {
      SECTION("Returns true") {
        const auto rhs = make_unexpected("0");

        REQUIRE(lhs == rhs);
      }
    }
    SECTION("unexpected compares unequal") {
      SECTION("Returns false") {
        const auto rhs = make_unexpected("hello");

        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
}

TEST_CASE("operator==(const unexpected<E>&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns false") {
      const auto rhs = expected<int,std::string>{0};
      const auto lhs = make_unexpected("hello world");

      REQUIRE_FALSE(lhs == rhs);
    }
  }
  SECTION("expected contains error") {
    const auto rhs = expected<int,std::string>{
      make_unexpected("0")
    };
    SECTION("unexpected compares equal") {
      SECTION("Returns true") {
        const auto lhs = make_unexpected("0");

        REQUIRE(lhs == rhs);
      }
    }
    SECTION("unexpected compares unequal") {
      SECTION("Returns false") {
        const auto lhs = make_unexpected("hello");

        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
}

TEST_CASE("operator!=(const expected<T1,E1>&, const unexpected<E>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns true") {
      const auto lhs = expected<int,std::string>{0};
      const auto rhs = make_unexpected("hello world");

      REQUIRE(lhs != rhs);
    }
  }
  SECTION("expected contains error") {
    const auto lhs = expected<int,std::string>{
      make_unexpected("0")
    };
    SECTION("unexpected compares equal") {
      SECTION("Returns false") {
        const auto rhs = make_unexpected("0");

        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("unexpected compares unequal") {
      SECTION("Returns true") {
        const auto rhs = make_unexpected("hello");

        REQUIRE(lhs != rhs);
      }
    }
  }
}

TEST_CASE("operator!=(const unexpected<E>&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns true") {
      const auto rhs = expected<int,std::string>{0};
      const auto lhs = make_unexpected("hello world");

      REQUIRE(lhs != rhs);
    }
  }
  SECTION("expected contains error") {
    const auto rhs = expected<int,std::string>{
      make_unexpected("0")
    };
    SECTION("unexpected compares equal") {
      SECTION("Returns false") {
        const auto lhs = make_unexpected("0");

        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("unexpected compares unequal") {
      SECTION("Returns true") {
        const auto lhs = make_unexpected("hello");

        REQUIRE(lhs != rhs);
      }
    }
  }
}

TEST_CASE("operator>=(const expected<T1,E1>&, const unexpected<E>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns false") {
      const auto lhs = expected<int,std::string>{0};
      const auto rhs = make_unexpected("hello world");

      REQUIRE_FALSE(lhs >= rhs);
    }
  }
  SECTION("expected contains error") {
    const auto lhs = expected<int,std::string>{
      make_unexpected("5")
    };
    SECTION("expected is greater or equal to unexpected") {
      SECTION("Returns true") {
        const auto rhs = make_unexpected("0");

        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("expected is not greater or equal to unexpected") {
      SECTION("Returns false") {
        const auto rhs = make_unexpected("9");

        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
}

TEST_CASE("operator>=(const unexpected<E>&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns false") {
      const auto lhs = make_unexpected("hello world");
      const auto rhs = expected<int,std::string>{0};

      REQUIRE(lhs >= rhs);
    }
  }
  SECTION("expected contains error") {
    const auto rhs = expected<int,std::string>{
      make_unexpected("3")
    };
    SECTION("unexpected is greater or equal to expected") {
      SECTION("Returns true") {
        const auto lhs = make_unexpected("5");

        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("unexpected is not greater or equal to expected") {
      SECTION("Returns false") {
        const auto lhs = make_unexpected("0");

        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
}

TEST_CASE("operator<=(const expected<T1,E1>&, const unexpected<E>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns true") {
      const auto lhs = expected<int,std::string>{0};
      const auto rhs = make_unexpected("hello world");

      REQUIRE(lhs <= rhs);
    }
  }
  SECTION("expected contains error") {
    const auto lhs = expected<int,std::string>{
      make_unexpected("5")
    };
    SECTION("expected is less or equal to unexpected") {
      SECTION("Returns true") {
        const auto rhs = make_unexpected("9");

        REQUIRE(lhs <= rhs);
      }
    }
    SECTION("expected is not less or equal to unexpected") {
      SECTION("Returns false") {
        const auto rhs = make_unexpected("0");

        REQUIRE_FALSE(lhs <= rhs);
      }
    }
  }
}

TEST_CASE("operator<=(const unexpected<E>&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns true") {
      const auto lhs = make_unexpected("hello world");
      const auto rhs = expected<int,std::string>{0};

      REQUIRE_FALSE(lhs <= rhs);
    }
  }
  SECTION("expected contains error") {
    const auto rhs = expected<int,std::string>{
      make_unexpected("5")
    };
    SECTION("expected is less or equal to unexpected") {
      SECTION("Returns true") {
        const auto lhs = make_unexpected("0");

        REQUIRE(lhs <= rhs);
      }
    }
    SECTION("expected is not less or equal to unexpected") {
      SECTION("Returns false") {
        const auto lhs = make_unexpected("9");

        REQUIRE_FALSE(lhs <= rhs);
      }
    }
  }
}

TEST_CASE("operator>(const expected<T1,E1>&, const unexpected<E>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns false") {
      const auto lhs = expected<int,std::string>{0};
      const auto rhs = make_unexpected("hello world");

      REQUIRE_FALSE(lhs > rhs);
    }
  }
  SECTION("expected contains error") {
    const auto lhs = expected<int,std::string>{
      make_unexpected("5")
    };
    SECTION("expected is greater or equal to unexpected") {
      SECTION("Returns true") {
        const auto rhs = make_unexpected("0");

        REQUIRE(lhs > rhs);
      }
    }
    SECTION("expected is not greater or equal to unexpected") {
      SECTION("Returns false") {
        const auto rhs = make_unexpected("9");

        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
}

TEST_CASE("operator>(const unexpected<E>&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns false") {
      const auto lhs = make_unexpected("hello world");
      const auto rhs = expected<int,std::string>{0};

      REQUIRE(lhs > rhs);
    }
  }
  SECTION("expected contains error") {
    const auto rhs = expected<int,std::string>{
      make_unexpected("3")
    };
    SECTION("unexpected is greater or equal to expected") {
      SECTION("Returns true") {
        const auto lhs = make_unexpected("5");

        REQUIRE(lhs > rhs);
      }
    }
    SECTION("unexpected is not greater or equal to expected") {
      SECTION("Returns false") {
        const auto lhs = make_unexpected("0");

        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
}

TEST_CASE("operator<(const expected<T1,E1>&, const unexpected<E>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns true") {
      const auto lhs = expected<int,std::string>{0};
      const auto rhs = make_unexpected("hello world");

      REQUIRE(lhs < rhs);
    }
  }
  SECTION("expected contains error") {
    const auto lhs = expected<int,std::string>{
      make_unexpected("5")
    };
    SECTION("expected is less or equal to unexpected") {
      SECTION("Returns true") {
        const auto rhs = make_unexpected("9");

        REQUIRE(lhs < rhs);
      }
    }
    SECTION("expected is not less or equal to unexpected") {
      SECTION("Returns false") {
        const auto rhs = make_unexpected("0");

        REQUIRE_FALSE(lhs < rhs);
      }
    }
  }
}

TEST_CASE("operator<(const unexpected<E>&, const expected<T1,E1>&)", "[compare]") {
  SECTION("expected contains value") {
    SECTION("Returns true") {
      const auto lhs = make_unexpected("hello world");
      const auto rhs = expected<int,std::string>{0};

      REQUIRE_FALSE(lhs < rhs);
    }
  }
  SECTION("expected contains error") {
    const auto rhs = expected<int,std::string>{
      make_unexpected("5")
    };
    SECTION("expected is less or equal to unexpected") {
      SECTION("Returns true") {
        const auto lhs = make_unexpected("0");

        REQUIRE(lhs < rhs);
      }
    }
    SECTION("expected is not less or equal to unexpected") {
      SECTION("Returns false") {
        const auto lhs = make_unexpected("9");

        REQUIRE_FALSE(lhs < rhs);
      }
    }
  }
}

//-----------------------------------------------------------------------------
// Utilities
//-----------------------------------------------------------------------------

TEST_CASE("swap(expected<T,E>&, expected<T,E>&)", "[utility]") {
  SECTION("lhs and rhs contain a value") {
    auto lhs_sut = expected<int, int>{42};
    auto rhs_sut = expected<int, int>{100};
    const auto lhs_old = lhs_sut;
    const auto rhs_old = rhs_sut;

    swap(lhs_sut, rhs_sut);

    SECTION("lhs contains value") {
      REQUIRE(lhs_sut.has_value());
    }
    SECTION("rhs contains value") {
      REQUIRE(rhs_sut.has_value());
    }
    SECTION("lhs contains rhs's old value") {
      REQUIRE(lhs_sut == rhs_old);
    }
    SECTION("rhs contains lhs's old value") {
      REQUIRE(rhs_sut == lhs_old);
    }
  }
  SECTION("lhs and rhs contain an error") {
    auto lhs_sut = expected<int, int>{make_unexpected(42)};
    auto rhs_sut = expected<int, int>{make_unexpected(100)};
    const auto lhs_old = lhs_sut;
    const auto rhs_old = rhs_sut;

    swap(lhs_sut, rhs_sut);

    SECTION("lhs contains error") {
      REQUIRE(lhs_sut.has_error());
    }
    SECTION("rhs contains error") {
      REQUIRE(rhs_sut.has_error());
    }
    SECTION("lhs contains rhs's old value") {
      REQUIRE(lhs_sut == rhs_old);
    }
    SECTION("rhs contains lhs's old value") {
      REQUIRE(rhs_sut == lhs_old);
    }
  }
  SECTION("lhs contains a value, rhs contains an error") {
    auto lhs_sut = expected<int, int>{42};
    auto rhs_sut = expected<int, int>{make_unexpected(42)};
    const auto lhs_old = lhs_sut;
    const auto rhs_old = rhs_sut;

    swap(lhs_sut, rhs_sut);

    SECTION("lhs contains error") {
      REQUIRE(lhs_sut.has_error());
    }
    SECTION("rhs contains value") {
      REQUIRE(rhs_sut.has_value());
    }
    SECTION("lhs contains rhs's old value") {
      REQUIRE(lhs_sut == rhs_old);
    }
    SECTION("rhs contains lhs's old value") {
      REQUIRE(rhs_sut == lhs_old);
    }
  }
  SECTION("lhs contains an error, rhs contains a value") {
    auto lhs_sut = expected<int, int>{make_unexpected(42)};
    auto rhs_sut = expected<int, int>{};
    const auto lhs_old = lhs_sut;
    const auto rhs_old = rhs_sut;

    swap(lhs_sut, rhs_sut);

    SECTION("lhs contains value") {
      REQUIRE(lhs_sut.has_value());
    }
    SECTION("rhs contains error") {
      REQUIRE(rhs_sut.has_error());
    }
    SECTION("lhs contains rhs's old value") {
      REQUIRE(lhs_sut == rhs_old);
    }
    SECTION("rhs contains lhs's old value") {
      REQUIRE(rhs_sut == lhs_old);
    }
  }
}

TEST_CASE("swap(expected<void,E>&, expected<void,E>&)", "[utility]") {
  SECTION("lhs and rhs contain a value") {
    auto lhs_sut = expected<void, int>{};
    auto rhs_sut = expected<void, int>{};
    const auto lhs_old = lhs_sut;
    const auto rhs_old = rhs_sut;

    swap(lhs_sut, rhs_sut);

    SECTION("lhs contains value") {
      REQUIRE(lhs_sut.has_value());
    }
    SECTION("rhs contains value") {
      REQUIRE(rhs_sut.has_value());
    }
    SECTION("lhs is unchanged") {
      REQUIRE(lhs_sut == lhs_old);
    }
    SECTION("rhs is unchanged") {
      REQUIRE(rhs_sut == rhs_old);
    }
  }
  SECTION("lhs and rhs contain an error") {
    auto lhs_sut = expected<void, int>{make_unexpected(42)};
    auto rhs_sut = expected<void, int>{make_unexpected(100)};
    const auto lhs_old = lhs_sut;
    const auto rhs_old = rhs_sut;

    swap(lhs_sut, rhs_sut);

    SECTION("lhs contains error") {
      REQUIRE(lhs_sut.has_error());
    }
    SECTION("rhs contains error") {
      REQUIRE(rhs_sut.has_error());
    }
    SECTION("lhs contains rhs's old value") {
      REQUIRE(lhs_sut == rhs_old);
    }
    SECTION("rhs contains lhs's old value") {
      REQUIRE(rhs_sut == lhs_old);
    }
  }
  SECTION("lhs contains a value, rhs contains an error") {
    auto lhs_sut = expected<void, int>{};
    auto rhs_sut = expected<void, int>{make_unexpected(42)};
    const auto lhs_old = lhs_sut;
    const auto rhs_old = rhs_sut;

    swap(lhs_sut, rhs_sut);

    SECTION("lhs contains error") {
      REQUIRE(lhs_sut.has_error());
    }
    SECTION("rhs contains value") {
      REQUIRE(rhs_sut.has_value());
    }
    SECTION("lhs contains rhs's old value") {
      REQUIRE(lhs_sut == rhs_old);
    }
    SECTION("rhs contains lhs's old value") {
      REQUIRE(rhs_sut == lhs_old);
    }
  }
  SECTION("lhs contains an error, rhs contains a value") {
    auto lhs_sut = expected<void, int>{make_unexpected(42)};
    auto rhs_sut = expected<void, int>{};
    const auto lhs_old = lhs_sut;
    const auto rhs_old = rhs_sut;

    swap(lhs_sut, rhs_sut);

    SECTION("lhs contains value") {
      REQUIRE(lhs_sut.has_value());
    }
    SECTION("rhs contains error") {
      REQUIRE(rhs_sut.has_error());
    }
    SECTION("lhs contains rhs's old value") {
      REQUIRE(lhs_sut == rhs_old);
    }
    SECTION("rhs contains lhs's old value") {
      REQUIRE(rhs_sut == lhs_old);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("std::hash<expected<T,E>>::operator()", "[utility]") {
  SECTION("Value is active") {
    SECTION("Hashes value") {
      const auto sut = expected<int,int>{42};

      const auto result = std::hash<expected<int,int>>{}(sut);
      static_cast<void>(result);

      SUCCEED();
    }
  }
  SECTION("Error is active") {
    SECTION("Hashes error") {
      const auto sut = expected<int,int>{make_unexpected(42)};

      const auto result = std::hash<expected<int,int>>{}(sut);
      static_cast<void>(result);

      SUCCEED();
    }
  }

  SECTION("Hash of T and E are different for the same values for T and E") {
    const auto value_sut = expected<int,int>{42};
    const auto error_sut = expected<int,int>{make_unexpected(42)};

    const auto value_hash = std::hash<expected<int,int>>{}(value_sut);
    const auto error_hash = std::hash<expected<int,int>>{}(error_sut);

    REQUIRE(value_hash != error_hash);
  }
}

TEST_CASE("std::hash<expected<void,E>>::operator()", "[utility]") {
  SECTION("Value is active") {
    SECTION("Produces '0' hash") {
      const auto sut = expected<void,int>{};

      const auto result = std::hash<expected<void,int>>{}(sut);

      REQUIRE(result == 0u);
    }
  }

  SECTION("Error is active") {
    SECTION("Hashes error") {
      const auto sut = expected<void,int>{make_unexpected(42)};

      const auto result = std::hash<expected<void,int>>{}(sut);
      static_cast<void>(result);

      SUCCEED();
    }
  }
}

} // namespace test
} // namespace expect

#if defined(_MSC_VER)
# pragma warning(pop)
#endif
