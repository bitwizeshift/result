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

#include <string>
#include <type_traits>
#include <ios> // std::ios_errc

#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4244) // Disable warnings about implicit conversions
#endif

namespace cpp {
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

template <typename T>
auto suppress_unused(const T&) -> void{}

} // namespace <anonymous>

//=============================================================================
// class : result<T, E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Destructor / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("result<T,E>::result()", "[ctor]") {
  SECTION("T is default-constructible") {
    SECTION("Expected is default-constructible") {
      using sut_type = result<int,int>;

      STATIC_REQUIRE(std::is_default_constructible<sut_type>::value);
    }
    // Note: 'E' must be default-constructible for use in 'error()', but
    //       otherwise does not contribute to this.
    SECTION("Default constructs the underlying T") {
      using value_type = std::string;
      using sut_type = result<value_type,int>;

      auto sut = sut_type{};

      REQUIRE(*sut == value_type());
    }
    SECTION("Invokes T's default-construction") {
      // TODO: Use a static mock
    }
  }

  SECTION("T is not default-constructible") {
    SECTION("Expected is not default-constructible") {
      using sut_type = result<not_default_constructible,int>;

      STATIC_REQUIRE_FALSE(std::is_default_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("result<T,E>::result(const result&)", "[ctor]") {

  // Constructible:

  SECTION("T and E are both trivially copy-constructible") {
    using sut_type = result<int,int>;

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
        REQUIRE(*sut == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = fail(42);
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
    using sut_type = result<std::string,int>;

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
        REQUIRE(*sut == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = fail(42);
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
    using sut_type = result<int,std::string>;

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
        REQUIRE(*sut == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = fail("Hello world");
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
    using sut_type = result<std::string,std::string>;

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
        REQUIRE(*sut == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = fail("Goodbye world");
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
    using sut_type = result<not_copy_or_moveable,int>;

    SECTION("Expected is not copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_copy_constructible<sut_type>::value);
    }
  }

  SECTION("E is not copy-constructible") {
    using sut_type = result<int,not_copy_or_moveable>;

    SECTION("Expected is not copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_copy_constructible<sut_type>::value);
    }
  }

  SECTION("T and E are not copy-constructible") {
    using sut_type = result<not_copy_or_moveable,not_copy_or_moveable>;

    SECTION("Expected is not copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_copy_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("result<T,E>::result(result&&)", "[ctor]") {

  // Constructible:

  SECTION("T and E are both trivially move-constructible") {
    using sut_type = result<int,int>;

    SECTION("Expected is trivially move-constructible") {
      STATIC_REQUIRE(std::is_trivially_move_constructible<sut_type>::value);
    }

    SECTION("Move source contained a value") {
      const auto value = 42;
      sut_type source{value};

      const auto sut = std::move(source);

      SECTION("New result contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("New result contains a value equal to the source") {
        REQUIRE(*sut == value);
      }
    }

    SECTION("Move source contained an error") {
      const auto error = fail(42);
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New result contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New result contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  } // T and E are both trivially move-constructible

  SECTION("T is move-constructible, but not trivial") {
    using sut_type = result<move_only<std::string>,int>;

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

      SECTION("New result contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("New result contains a value equal to the source") {
        REQUIRE(*sut == value);
      }
    }

    SECTION("Move source contained an error") {
      const auto error = fail(42);
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New result contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New result contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  } // T is move-constructible, but not trivial

  SECTION("E is move-constructible, but not trivial") {
    using sut_type = result<int,move_only<std::string>>;

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

      SECTION("New result contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("New result contains a value equal to the source") {
        REQUIRE(*sut == value);
      }
    }

    SECTION("Move source contained an error") {
      const auto error = fail("Hello world");
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New result contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New result contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  SECTION("T and E are both move-constructible, but not trivial") {
    using sut_type = result<move_only<std::string>,move_only<std::string>>;

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

      SECTION("New result contains a value") {
        REQUIRE(sut.has_value());
      }
      SECTION("New result contains a value equal to the source") {
        REQUIRE(*sut == value);
      }
    }

    SECTION("Move source contained an error") {
      const auto error = fail("Goodbye world");
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New result contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New result contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  // Not constructible:

  SECTION("T is not move-constructible") {
    using sut_type = result<not_copy_or_moveable,int>;

    SECTION("Expected is not move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_move_constructible<sut_type>::value);
    }
  }

  SECTION("E is not move-constructible") {
    using sut_type = result<int,not_copy_or_moveable>;

    SECTION("Expected is not move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_move_constructible<sut_type>::value);
    }
  }

  SECTION("T and E are not move-constructible") {
    using sut_type = result<not_copy_or_moveable,not_copy_or_moveable>;

    SECTION("Expected is not move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_move_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("result<T,E>::result(const result<T2,E2>&)", "[ctor]") {
  // Constructible:

  SECTION("T and E are both constructible from T2 and E2") {
    using copy_type = result<const char*, const char*>;
    using sut_type = result<std::string,std::string>;

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
        REQUIRE(*sut == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = fail("Goodbye world");
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
    using copy_type = result<std::string,int>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }

  SECTION("E is not constructible from E2") {
    using copy_type = result<int,std::string>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }

  SECTION("T and E are not constructible from T2 and E2") {
    using copy_type = result<std::string,std::string>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }
}

TEST_CASE("result<T,E>::result(const result<T2,E2>&) (explicit)", "[ctor]") {

  // Constructible:

  SECTION("T and E are both constructible from T2 and E2") {
    SECTION("T is explicit constructible from T2") {
      using copy_type = result<std::string, const char*>;
      using sut_type = result<explicit_type<std::string>,std::string>;

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
          REQUIRE(*sut == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = fail("Goodbye world");
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
      using copy_type = result<const char*,std::string>;
      using sut_type = result<std::string,explicit_type<std::string>>;

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
          REQUIRE(*sut == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = fail("Goodbye world");
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
      using copy_type = result<std::string, std::string>;
      using sut_type = result<explicit_type<std::string>,explicit_type<std::string>>;

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
          REQUIRE(*sut == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = fail("Goodbye world");
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
    using copy_type = result<std::string,int>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }

  SECTION("E is not constructible from E2") {
    using copy_type = result<int,std::string>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }

  SECTION("T and E are not constructible from T2 and E2") {
    using copy_type = result<std::string,std::string>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const copy_type&>::value);
    }
  }
}

TEST_CASE("result<T,E>::result(result<T2,E2>&&)", "[ctor]") {

  // Constructible Case:

  SECTION("T and E are both constructible from T2 and E2") {
    using copy_type = result<std::string,std::string>;
    using sut_type = result<move_only<std::string>,move_only<std::string>>;

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
        REQUIRE(*sut == value);
      }
    }

    SECTION("Copy source contained an error") {
      const auto error = fail("Goodbye world");
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
    using copy_type = result<std::string,int>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }

  SECTION("E is not constructible from E2") {
    using copy_type = result<int,std::string>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }

  SECTION("T and E are not constructible from T2 and E2") {
    using copy_type = result<std::string,std::string>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }
}

TEST_CASE("result<T,E>::result(result<T2,E2>&&) (explicit)", "[ctor]") {

  // Constructors:

  SECTION("T and E are both constructible from T2 and E2") {
    SECTION("T is explicit constructible from T2") {
      using copy_type = result<std::string, const char*>;
      using sut_type = result<explicit_type<move_only<std::string>>,std::string>;

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
          REQUIRE(*sut == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = fail("Goodbye world");
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
      using copy_type = result<const char*,std::string>;
      using sut_type = result<std::string,explicit_type<move_only<std::string>>>;

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
          REQUIRE(*sut == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = fail("Goodbye world");
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
      using copy_type = result<std::string, std::string>;
      using sut_type = result<
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
          REQUIRE(*sut == value);
        }
      }

      SECTION("Copy source contained an error") {
        const auto error = fail("Goodbye world");
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
    using copy_type = result<std::string,int>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }

  SECTION("E is not constructible from E2") {
    using copy_type = result<int,std::string>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }

  SECTION("T and E are not constructible from T2 and E2") {
    using copy_type = result<std::string,std::string>;
    using sut_type = result<int,int>;

    SECTION("Expected is not constructible") {
      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, copy_type&&>::value);
    }
  }
}

TEST_CASE("result<T,E>::result(in_place_t, Args&&...)", "[ctor]") {
  using sut_type = result<std::string,int>;

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

TEST_CASE("result<T,E>::result(in_place_t, std::initializer_list<U>, Args&&...)", "[ctor]") {
  using sut_type = result<std::string,int>;

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

TEST_CASE("result<T,E>::result(in_place_error_t, Args&&...)", "[ctor]") {
  using sut_type = result<int,std::string>;

  const auto input = "hello world";
  const auto size = 5;
  const auto result = std::string{input, size};

  const sut_type sut(in_place_error, input, size);

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == fail(std::ref(result)));
  }
}

TEST_CASE("result<T,E>::result(in_place_error_t, std::initializer_list<U>, Args&&...)", "[ctor]") {
  using sut_type = result<int,std::string>;

  const auto value = std::string{
    {'h','e','l','l','o'}, std::allocator<char>{}
  };

  const sut_type sut(in_place_error, {'h','e','l','l','o'}, std::allocator<char>{});

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == fail(std::ref(value)));
  }
}

TEST_CASE("result<T,E>::result(const failure<E2>&)", "[ctor]") {
  using sut_type = result<int,std::string>;
  const auto source = fail<std::string>("hello world");

  const sut_type sut{source};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == source);
  }
}

TEST_CASE("result<T,E>::result(failure<E2>&&)", "[ctor]") {
  using sut_type = result<int,move_only<std::string>>;
  auto source = fail<std::string>("hello world");
  const auto copy = source;

  const sut_type sut{std::move(source)};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from E2") {
    REQUIRE(sut == copy);
  }
}

TEST_CASE("result<T,E>::result(U&&)", "[ctor]") {
  using sut_type = result<std::string,int>;
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

TEST_CASE("result<T,E>::result(U&&) (explicit)", "[ctor]") {
  using sut_type = result<explicit_type<std::string>,int>;
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

TEST_CASE("result<T,E>::~result()", "[dtor]") {
  SECTION("T and E are trivially destructible") {
    using sut_type = result<int, int>;

    SECTION("result's destructor is trivial") {
      STATIC_REQUIRE(std::is_trivially_destructible<sut_type>::value);
    }
  }
  SECTION("T is not trivially destructible") {
    using sut_type = result<report_destructor, int>;

    auto invoked = false;
    {
      sut_type sut{in_place, &invoked};
      (void) sut;
    }

    SECTION("result's destructor is not trivial") {
      STATIC_REQUIRE_FALSE(std::is_trivially_destructible<sut_type>::value);
    }

    SECTION("Invokes T's underlying destructor") {
      REQUIRE(invoked);
    }
  }
  SECTION("E is not trivially destructible") {
    using sut_type = result<int, report_destructor>;

    auto invoked = false;
    {
      sut_type sut{in_place_error, &invoked};
      (void) sut;
    }

    SECTION("result's destructor is not trivial") {
      STATIC_REQUIRE_FALSE(std::is_trivially_destructible<sut_type>::value);
    }

    SECTION("Invokes E's underlying destructor") {
      REQUIRE(invoked);
    }
  }
  SECTION("T and E are not trivially destructible") {
    using sut_type = result<report_destructor, report_destructor>;

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

    SECTION("result's destructor is not trivial") {
      STATIC_REQUIRE_FALSE(std::is_trivially_destructible<sut_type>::value);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("result<T,E>::operator=(const result&)", "[assign]") {
  SECTION("T is not nothrow copy constructible") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = result<std::string,int>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }
  SECTION("E is not nothrow copy constructible") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = result<int,std::string>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }
  SECTION("T and E are not nothrow copy constructible") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = result<std::string,std::string>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }

  SECTION("T is not copy-assignable") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = result<not_copy_or_moveable,int>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }
  SECTION("E is not copy-assignable") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = result<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }
  SECTION("T and E are not copy-assignable") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = result<not_copy_or_moveable,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }

  SECTION("T and E are nothrow copy-constructible") {
    SECTION("Expected is copy-assignable") {
      using sut_type = result<int,std::error_code>;

      STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
    }
    SECTION("result contains a value") {
      SECTION("'other' contains a value") {
        using sut_type = result<int,std::error_code>;

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
        using sut_type = result<report_destructor,const char*>;

        const auto value = fail("42");
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
    SECTION("result contains an error") {
      SECTION("'other' contains a value") {
        using sut_type = result<int,report_destructor>;

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
        using sut_type = result<int,int>;

        const auto value = fail(42);
        const sut_type copy{value};
        sut_type sut{fail(0)};

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
    using sut_type = result<int, int>;

    SECTION("Expected is copy-assignable") {
      STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
    }
    SECTION("Expected is trivial copy-assignable") {
      STATIC_REQUIRE(std::is_trivially_copy_assignable<sut_type>::value);
    }
  }
}

TEST_CASE("result<T,E>::operator=(result&&)", "[assign]") {
  SECTION("T is not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using sut_type = result<throwing<std::string>,int>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }
  SECTION("E is not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using sut_type = result<int,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }
  SECTION("T and E are not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using sut_type = result<throwing<std::string>,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }

  SECTION("T is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using sut_type = result<not_copy_or_moveable,int>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }
  SECTION("E is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using sut_type = result<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }
  SECTION("T and E are not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using sut_type = result<not_copy_or_moveable,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }

  SECTION("T and E are nothrow copy-constructible") {
    SECTION("Expected is move-assignable") {
      using sut_type = result<int,std::error_code>;

      STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
    }
    SECTION("result contains a value") {
      SECTION("'other' contains a value") {
        using sut_type = result<move_only<std::string>,std::error_code>;

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
        using sut_type = result<move_only<report_destructor>,const char*>;

        const auto value = fail("42");
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
    SECTION("result contains an error") {
      SECTION("'other' contains a value") {
        using sut_type = result<int,move_only<report_destructor>>;

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
        using sut_type = result<int,move_only<std::string>>;

        const auto value = fail("hello world");
        sut_type copy{value};
        sut_type sut{fail("goodbye world")};

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
    using sut_type = result<int, int>;

    SECTION("Expected is move-assignable") {
      STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
    }
    SECTION("Expected is trivial move-assignable") {
      STATIC_REQUIRE(std::is_trivially_move_assignable<sut_type>::value);
    }
  }
}

TEST_CASE("result<T,E>::operator=(const result<T2,E2>&)", "[assign]") {
SECTION("T is not nothrow constructible from T2") {
    SECTION("Expected is not assignable") {
      using copy_type = result<std::string, long>;
      using sut_type = result<throwing<std::string>,int>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }
  SECTION("E is not nothrow constructible from E2") {
    SECTION("Expected is not assignable") {
      using copy_type = result<int,std::string>;
      using sut_type = result<int,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }
  SECTION("T and E are not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using copy_type = result<std::string,std::string>;
      using sut_type = result<throwing<std::string>,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }

  SECTION("T is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = result<not_copy_or_moveable,long>;
      using sut_type = result<not_copy_or_moveable,int>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }
  SECTION("E is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = result<long,not_copy_or_moveable>;
      using sut_type = result<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }
  SECTION("T and E are not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = result<not_copy_or_moveable,not_copy_or_moveable>;
      using sut_type = result<not_copy_or_moveable,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const copy_type&>::value);
    }
  }

  SECTION("T and E are nothrow copy-constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = result<long,std::io_errc>;
      using sut_type = result<int,move_only<std::error_code>>;

      STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
    }
    SECTION("result contains a value") {
      SECTION("'other' contains a value") {
        using copy_type = result<const char*,std::error_code>;
        using sut_type = result<nonthrowing<std::string>,std::error_code>;

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
        using copy_type = result<report_destructor,const char*>;
        using sut_type = result<report_destructor,const char*>;

        const auto value = fail("42");
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
    SECTION("result contains an error") {
      SECTION("'other' contains a value") {
        using copy_type = result<int,report_destructor>;
        using sut_type = result<int,report_destructor>;

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
        using copy_type = result<int,const char*>;
        using sut_type = result<int,nonthrowing<std::string>>;

        const auto value = fail("hello world");
        copy_type copy{value};
        sut_type sut{fail("goodbye world")};

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

TEST_CASE("result<T,E>::operator=(result<T2,E2>&&)", "[assign]") {
  SECTION("T is not nothrow constructible from T2") {
    SECTION("Expected is not assignable") {
      using copy_type = result<std::string, long>;
      using sut_type = result<throwing<std::string>,int>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, copy_type&&>::value);
    }
  }
  SECTION("E is not nothrow constructible from E2") {
    SECTION("Expected is not assignable") {
      using copy_type = result<int,std::string>;
      using sut_type = result<int,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, copy_type&&>::value);
    }
  }
  SECTION("T and E are not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using copy_type = result<std::string,std::string>;
      using sut_type = result<throwing<std::string>,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, copy_type&&>::value);
    }
  }

  SECTION("T is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = result<not_copy_or_moveable,long>;
      using sut_type = result<not_copy_or_moveable,int>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, copy_type&&>::value);
    }
  }
  SECTION("E is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = result<long,not_copy_or_moveable>;
      using sut_type = result<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, copy_type&&>::value);
    }
  }
  SECTION("T and E are not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = result<not_copy_or_moveable,not_copy_or_moveable>;
      using sut_type = result<not_copy_or_moveable,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("T and E are nothrow copy-constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = result<long,std::io_errc>;
      using sut_type = result<int,move_only<std::error_code>>;

      STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
    }
    SECTION("result contains a value") {
      SECTION("'other' contains a value") {
        using copy_type = result<std::string,std::error_code>;
        using sut_type = result<move_only<std::string>,std::error_code>;

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
        using copy_type = result<report_destructor,const char*>;
        using sut_type = result<move_only<report_destructor>,const char*>;

        const auto value = fail("42");
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
    SECTION("result contains an error") {
      SECTION("'other' contains a value") {
        using copy_type = result<int,report_destructor>;
        using sut_type = result<int,move_only<report_destructor>>;

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
        using copy_type = result<int,std::string>;
        using sut_type = result<int,move_only<std::string>>;

        const auto value = fail("hello world");
        copy_type copy{value};
        sut_type sut{fail("goodbye world")};

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

TEST_CASE("result<T,E>::operator=(U&&)", "[assign]") {
  SECTION("T is not nothrow constructible from U") {
    SECTION("result is not assignable from U") {
      STATIC_REQUIRE_FALSE(std::is_assignable<result<throwing<std::string>,std::error_code>,const char*>::value);
    }
  }
  SECTION("T is constructible from U, and nothrow move constructible") {
    SECTION("result is assignable from U") {
      // This works by creating an intermediate `result` object which then
      // is moved through non-throwing move-assignment
      STATIC_REQUIRE(std::is_assignable<result<std::string,std::error_code>,const char*>::value);
    }
  }
  SECTION("T is not assignable or constructible from U") {
    SECTION("result is not assignable from U") {
      STATIC_REQUIRE_FALSE(std::is_assignable<result<int,std::error_code>,const char*>::value);
    }
  }
  SECTION("result contains a value") {
    using sut_type = result<int,std::error_code>;

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
  SECTION("result contains an error") {
    using sut_type = result<int, report_destructor>;

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

TEST_CASE("result<T,E>::operator=(const failure<E2>&)", "[assign]") {
  SECTION("E cant be constructed or assigned from E2") {
    SECTION("Expected cannot be assigned from E2") {
      using copy_type = failure<not_copy_or_moveable>;
      using sut_type  = result<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }
  SECTION("E is not nothrow constructible from E2") {
    SECTION("Expected is not copy-assignable") {
      using copy_type = failure<const char*>;
      using sut_type = result<int,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E is not nothrow move constructible from E2, but is nothrow-move constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = failure<const char*>;
      using sut_type = result<int,std::string>;

      // This works because it generates an intermediate 'result<int, E>' in
      // between. The result is then move-constructed instead -- which is
      // non-throwing.

      STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E can be constructed and assigned from E2") {
    SECTION("active element is T") {
      using copy_type = failure<const char*>;
      using sut_type = result<report_destructor, std::string>;

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
      using copy_type = failure<const char*>;
      using sut_type = result<int, std::string>;

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

TEST_CASE("result<T,E>::operator=(failure<E2>&&)", "[assign]") {
  SECTION("E cant be constructed or assigned from E2") {
    SECTION("Expected cannot be assigned from E2") {
      using copy_type = failure<not_copy_or_moveable>;
      using sut_type  = result<int,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }
  SECTION("E is not nothrow move constructible from E2") {
    SECTION("Expected is not move-assignable") {
      using copy_type = failure<const char*>;
      using sut_type = result<int,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E is not nothrow move constructible from E2, but is nothrow-move constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = failure<const char*>;
      using sut_type = result<int,std::string>;

      // This works because it generates an intermediate 'result<int, E>' in
      // between. The result is then move-constructed instead -- which is
      // non-throwing.

      STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E can be constructed and assigned from E2") {
    SECTION("active element is T") {
      using copy_type = failure<std::string>;
      using sut_type = result<report_destructor, move_only<std::string>>;

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
      using copy_type = failure<std::string>;
      using sut_type = result<int, move_only<std::string>>;

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

TEST_CASE("result<T,E>::operator->()", "[observers]") {
  auto sut = result<int,int>{42};

  SECTION("Returns pointer to internal structure") {
    REQUIRE(&*sut == sut.operator->());
  }

  SECTION("Returns a mutable pointer") {
    STATIC_REQUIRE(std::is_same<decltype(sut.operator->()),int*>::value);
  }
}

TEST_CASE("result<T,E>::operator->() const", "[observers]") {
  const auto sut = result<int,int>{42};

  SECTION("Returns pointer to internal structure") {
    REQUIRE(&*sut == sut.operator->());
  }

  SECTION("Returns a const pointer") {
    STATIC_REQUIRE(std::is_same<decltype(sut.operator->()),const int*>::value);
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("result<T,E>::operator*() &", "[observers]") {
  auto sut = result<int,int>{42};

  SECTION("Returns mutable lvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*sut),int&>::value);
  }
  SECTION("Returns reference to internal structure") {
    auto& x = sut.operator*();

    (void) x;
    SUCCEED();
  }
}

TEST_CASE("result<T,E>::operator*() const &", "[observers]") {
  const auto sut = result<int,int>{42};

  SECTION("Returns const lvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*sut),const int&>::value);
  }
  SECTION("Returns reference to internal structure") {
    auto& x = sut.operator*();

    (void) x;
    SUCCEED();
  }
}

TEST_CASE("result<T,E>::operator*() &&", "[observers]") {
  auto sut = result<int,int>{42};

  SECTION("Returns mutable rvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*std::move(sut)),int&&>::value);
  }
  SECTION("Returns reference to internal structure") {
    auto&& x = std::move(sut).operator*();

    (void) x;
    SUCCEED();
  }
}

TEST_CASE("result<T,E>::operator*() const &&", "[observers]") {
  const auto sut = result<int,int>{42};

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

TEST_CASE("result<T,E>::operator bool()", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Returns true") {
      auto sut = result<int, int>{};

      REQUIRE(static_cast<bool>(sut));
    }
  }
  SECTION("result does not contain a value") {
    SECTION("Returns false") {
      auto sut = result<int, int>{fail(42)};

      REQUIRE_FALSE(static_cast<bool>(sut));
    }
  }
}

TEST_CASE("result<T,E>::has_value()", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Returns true") {
      auto sut = result<int, int>{};

      REQUIRE(sut.has_value());
    }
  }
  SECTION("result does not contain a value") {
    SECTION("Returns false") {
      auto sut = result<int, int>{fail(42)};

      REQUIRE_FALSE(sut.has_value());
    }
  }
}

TEST_CASE("result<T,E>::has_error()", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Returns false") {
      auto sut = result<int, int>{};

      REQUIRE_FALSE(sut.has_error());
    }
  }
  SECTION("result does not contain a value") {
    SECTION("Returns true") {
      auto sut = result<int, int>{fail(42)};

      REQUIRE(sut.has_error());
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("result<T,E>::value() &", "[observers]") {
  SECTION("result contains a value") {
    auto sut = result<int,std::error_code>{42};
    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(suppress_unused(sut.value()));
    }
    SECTION("Returns mutable lvalue reference to internal storage") {
      STATIC_REQUIRE(std::is_same<decltype(sut.value()),int&>::value);
    }
  }
  SECTION("result contains an error") {
    SECTION("throws bad_result_access") {
      auto sut = result<int,int>{
        fail(42)
      };

      REQUIRE_THROWS_AS(suppress_unused(sut.value()), bad_result_access<int>);
    }
  }
}

TEST_CASE("result<T,E>::value() const &", "[observers]") {
  SECTION("result contains a value") {
    const auto sut = result<int,std::error_code>{42};
    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(suppress_unused(sut.value()));
    }
    SECTION("Returns const lvalue reference to internal storage") {
      STATIC_REQUIRE(std::is_same<decltype(sut.value()),const int&>::value);
    }
  }
  SECTION("result contains an error") {
    SECTION("throws bad_result_access") {
      const auto sut = result<int,int>{
        fail(42)
      };

      REQUIRE_THROWS_AS(suppress_unused(sut.value()), bad_result_access<int>);
    }
  }
}

TEST_CASE("result<T,E>::value() &&", "[observers]") {
  SECTION("result contains a value") {
    auto sut = result<int,std::error_code>{42};
    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(suppress_unused(std::move(sut).value()));
    }
    SECTION("Returns mutable rvalue reference to internal storage") {
      STATIC_REQUIRE(std::is_same<decltype(std::move(sut).value()),int&&>::value);
    }
  }
  SECTION("result contains an error") {
    SECTION("throws bad_result_access") {
      auto sut = result<int,move_only<std::string>>{
        fail("hello world")
      };

      REQUIRE_THROWS_AS(suppress_unused(std::move(sut).value()), bad_result_access<move_only<std::string>>);
    }
  }
}

TEST_CASE("result<T,E>::value() const &&", "[observers]") {
  SECTION("result contains a value") {
    const auto sut = result<int,std::error_code>{42};
    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(suppress_unused(std::move(sut).value()));
    }
    SECTION("Returns const rvalue reference to internal storage") {
      STATIC_REQUIRE(std::is_same<decltype(std::move(sut).value()),const int&&>::value);
    }
  }
  SECTION("result contains an error") {
    SECTION("throws bad_result_access") {
      const auto sut = result<int,int>{
        fail(42)
      };

      REQUIRE_THROWS_AS(suppress_unused(std::move(sut).value()), bad_result_access<int>);
    }
  }
}

//-----------------------------------------------------------------------------


TEST_CASE("result<T,E>::error() const &", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Returns default-constructed error") {
      auto sut = result<int, int>{};

      const auto output = sut.error();

      REQUIRE(output == int{});
    }
  }
  SECTION("result contains an error") {
    SECTION("Returns a copy of the exception") {
      const auto value = 42;
      auto sut = result<int, int>{fail(value)};

      const auto output = sut.error();

      REQUIRE(output == value);
    }
  }
}

TEST_CASE("result<T,E>::error() &&", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Returns default-constructed error") {
      auto sut = result<int, move_only<std::error_code>>{};

      const auto output = std::move(sut).error();

      REQUIRE(output == std::error_code{});
    }
  }
  SECTION("result contains an error") {
    SECTION("Returns a copy of the exception") {
      const auto value = std::io_errc::stream;
      auto sut = result<int, move_only<std::error_code>>{fail(value)};

      const auto output = std::move(sut).error();

      REQUIRE(output == value);
    }
  }
}

TEST_CASE("result<T,E>::expect(String&&) const &", "[observers]") {
  SECTION("result contains value") {
    const auto sut = result<int,int>{42};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(sut.expect("test"));
    }
  }
  SECTION("result contains error") {
    SECTION("throws exception") {
      const auto sut = result<int,int>{fail(42)};

      REQUIRE_THROWS_AS(sut.expect("test"), bad_result_access<int>);
    }
  }
}

TEST_CASE("result<T,E>::expect(String&&) &&", "[observers]") {
  SECTION("result contains value") {
    auto sut = result<int,move_only<std::string>>{42};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(std::move(sut).expect("test"));
    }
  }
  SECTION("result contains error") {
    SECTION("throws exception") {
      auto sut = result<int,move_only<std::string>>{fail("hello world")};

      REQUIRE_THROWS_AS(std::move(sut).expect("test"), bad_result_access<move_only<std::string>>);
    }
  }
}

//-----------------------------------------------------------------------------
// Monadic Functionalities
//-----------------------------------------------------------------------------

TEST_CASE("result<T,E>::error_or(U&&) const &", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Returns the input") {
      const auto input = std::error_code{};
      auto sut = result<int, std::error_code>{42};

      const auto output = sut.error_or(input);

      REQUIRE(output == input);
    }
  }
  SECTION("result contains an error") {
    SECTION("Returns the error") {
      const auto input = std::error_code{std::io_errc::stream};
      auto sut = result<void, std::error_code>{
        fail(input)
      };

      const auto output = sut.error_or(input);

      REQUIRE(output == input);
    }
  }
}

TEST_CASE("result<T,E>::error_or(U&&) &&", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Returns the input") {
      auto input = std::error_code{};
      const auto copy = input;
      auto sut = result<int, move_only<std::error_code>>{42};

      const auto output = std::move(sut).error_or(std::move(input));

      REQUIRE(output == copy);
    }
  }
  SECTION("result contains an error") {
    SECTION("Returns the error") {
      auto input = std::error_code{std::io_errc::stream};
      const auto copy = input;
      auto sut = result<int, move_only<std::error_code>>{
        fail(input)
      };

      const auto output = std::move(sut).error_or(std::move(input));

      REQUIRE(output == copy);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("result<T,E>::and_then(U&&) const", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Maps the input") {
      const auto input = 42;
      auto sut = result<int, std::error_code>{};

      const auto output = sut.and_then(input);

      REQUIRE(output == input);
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto input = 42;
      const auto error = fail(std::io_errc::stream);
      auto sut = result<int, std::error_code>{error};

      const auto output = sut.and_then(input);

      REQUIRE(output == error);
    }
  }
}

TEST_CASE("result<T,E>::flat_map(Fn&&) const &", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Maps the input") {
      const auto value = 42;
      const auto sut = result<int,std::error_code>{value};

      const auto output = sut.flat_map([](int x){
        return result<std::string,std::error_code>{std::to_string(x)};
      });

      REQUIRE(output == std::to_string(value));
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail(std::io_errc::stream);
      const auto sut = result<int,std::error_code>{error};

      const auto output = sut.flat_map([](int x){
        return result<std::string,std::error_code>{std::to_string(x)};
      });

      REQUIRE(output == error);
    }
  }
}

TEST_CASE("result<T,E>::flat_map(Fn&&) &&", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Maps the input") {
      const auto value = 42;
      auto sut = result<int,move_only<std::error_code>>{value};

      const auto output = std::move(sut).flat_map([](int x){
        return result<std::string,std::error_code>{std::to_string(x)};
      });

      REQUIRE(output == std::to_string(value));
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail(std::io_errc::stream);
      auto sut = result<int,move_only<std::error_code>>{error};

      const auto output = std::move(sut).flat_map([](int x){
        return result<std::string,std::error_code>{std::to_string(x)};
      });

      REQUIRE(output == error);
    }
  }
}

TEST_CASE("result<T,E>::map(Fn&&) const &", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Function returns non-void") {
      SECTION("Maps the input") {
        const auto value = 42;
        auto sut = result<int,std::io_errc>{value};

        const auto output = sut.map([](int x){
          return std::to_string(x);
        });

        REQUIRE(output == std::to_string(value));
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto value = 42;
        auto sut = result<int,std::io_errc>{value};

        const auto output = sut.map([](int){});

        SECTION("Result has value") {
          REQUIRE(output.has_value());
        }
        SECTION("Result is result<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(output),const result<void,std::io_errc>>::value);
        }
      }
    }
  }
  SECTION("result contains an error") {
    SECTION("Function returns non-void") {
      SECTION("Maps the error") {
        const auto error = fail(std::io_errc::stream);
        auto sut = result<int,std::io_errc>{error};

        const auto output = sut.map([](int x){
          return std::to_string(x);
        });

        REQUIRE(output == error);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto error = fail(std::io_errc::stream);
        auto sut = result<int,std::io_errc>{error};

        const auto output = sut.map([](int) -> void{});

        SECTION("Result contains error") {
          REQUIRE(output == error);
        }
        SECTION("Result is result<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(output),const result<void,std::io_errc>>::value);
        }
      }
    }
  }
}

TEST_CASE("result<T,E>::map(Fn&&) &&", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Function returns non-void") {
      SECTION("Maps the input") {
        const auto value = 42;
        auto sut = result<int,move_only<std::error_code>>{value};

        const auto output = std::move(sut).map([](int x){
          return std::to_string(x);
        });

        REQUIRE(output == std::to_string(value));
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto value = 42;
        auto sut = result<int,move_only<std::error_code>>{value};

        const auto output = std::move(sut).map([](int){});

        SECTION("Result has value") {
          REQUIRE(output.has_value());
        }
        SECTION("Result is result<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(output),const result<void,move_only<std::error_code>>>::value);
        }
      }
    }
  }
  SECTION("result contains an error") {
    SECTION("Function returns non-void") {
      SECTION("Maps the error") {
        const auto error = fail(std::io_errc::stream);
        auto sut = result<int,move_only<std::error_code>>{error};

        const auto output = std::move(sut).map([&](int x){
          return std::to_string(x);
        });

        REQUIRE(output == error);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto error = fail(std::io_errc::stream);
        auto sut = result<int,move_only<std::error_code>>{error};

        const auto output = std::move(sut).map([](int) -> void{});

        SECTION("Result contains the error") {
          REQUIRE(output == error);
        }
        SECTION("Result is result<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(output),const result<void,move_only<std::error_code>>>::value);
        }
      }
    }
  }
}

TEST_CASE("result<T,E>::map_error(Fn&&) const &", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Maps the input") {
      const auto value = 42;
      auto sut = result<int,std::io_errc>{value};

      const auto output = sut.map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(output == value);
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail(std::io_errc::stream);
      auto sut = result<int,std::io_errc>{error};

      const auto output = sut.map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(output == error);
    }
  }
}

TEST_CASE("result<T,E>::map_error(Fn&&) &&", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Maps the input") {
      const auto value = "hello world";
      auto sut = result<move_only<std::string>,std::io_errc>{value};

      const auto output = std::move(sut).map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(output == value);
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail(std::io_errc::stream);
      auto sut = result<move_only<std::string>,std::io_errc>{error};

      const auto output = std::move(sut).map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(output == error);
    }
  }
}

TEST_CASE("result<T,E>::flat_map_error(Fn&&) const &", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Forwards underlying value") {
      const auto value = 42;
      const auto sut = result<int,int>{value};

      const auto output = sut.flat_map_error([&](int x){
        return result<long, int>{x};
      });

      REQUIRE(output == value);
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail(42);
      const auto sut = result<int, long>{error};

      const auto output = sut.flat_map_error([&](long x){
        return result<int, short>{x};
      });

      REQUIRE(output == error.error());
    }
  }
}

TEST_CASE("result<T,E>::flat_map_error(Fn&&) &&", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Default-initializes T") {
      const auto value = "hello world";
      auto sut = result<move_only<std::string>,move_only<std::string>>{value};

      const auto output = std::move(sut).flat_map_error([](move_only<std::string>&& x){
        return result<std::string, std::string>{in_place_error, std::move(x)};
      });

      REQUIRE(output == value);
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail("Hello world");
      auto sut = result<long,move_only<std::string>>{error};

      const auto output = std::move(sut).flat_map_error([](move_only<std::string>&& x){
        return result<int, std::string>{in_place_error, std::move(x)};
      });

      REQUIRE(output == error);
    }
  }
}

//=============================================================================
// class : result<T, E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Destructor / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("result<T&,E>::result()", "[ctor]") {
  SECTION("Expected containing a reference is not default-constructible") {
    using sut_type = result<int&,int>;

    STATIC_REQUIRE_FALSE(std::is_default_constructible<sut_type>::value);
  }
}

TEST_CASE("result<T&,E>::result(const result&)", "[ctor]") {
  using sut_type = result<int&,int>;

  SECTION("Expected containing a reference is copy-constructible") {
    STATIC_REQUIRE(std::is_copy_constructible<sut_type>::value);
  }

  SECTION("Constructs an result that references the source value") {
    auto value = int{42};

    const auto source = sut_type{value};
    const auto sut = source;

    SECTION("Refers to underlying input") {
      REQUIRE(&value == &(*sut));
    }
  }
}

TEST_CASE("result<T&,E>::result(result&&)", "[ctor]") {
  using sut_type = result<int&,move_only<std::string>>;

  SECTION("Expected containing a reference is copy-constructible") {
    STATIC_REQUIRE(std::is_move_constructible<sut_type>::value);
  }

  SECTION("Constructs an result that references the source value") {
    auto value = int{42};

    auto source = sut_type{value};
    const auto sut = std::move(source);

    SECTION("Refers to underlying input") {
      REQUIRE(&value == &(*sut));
    }
  }
}

TEST_CASE("result<T&,E>::result(const result<T2,E2>&)", "[ctor]") {
  SECTION("other's type holds T by-value") {
    SECTION("Constructor is disabled") {
      using from_type = result<int,std::error_code>;
      using sut_type = result<int&, std::error_code>;

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, const from_type&>::value);
    }
  }

  SECTION("other's type holds T by reference") {
    using from_type = result<derived&,int>;
    using sut_type = result<base&,int>;

    SECTION("Expected containing a reference is copy-constructible") {
      STATIC_REQUIRE(std::is_constructible<sut_type, const from_type&>::value);
    }

    SECTION("Constructs an result that references the source value") {
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

TEST_CASE("result<T&,E>::result(result<T2,E2>&&)", "[ctor]") {

  SECTION("other's type holds T by-value") {
    SECTION("Constructor is disabled") {
      using from_type = result<int,move_only<std::string>>;
      using sut_type = result<int&,move_only<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_constructible<sut_type, from_type&&>::value);
    }
  }
  SECTION("other's type holds T by reference") {
    using from_type = result<derived&,move_only<std::string>>;
    using sut_type = result<base&,move_only<std::string>>;

    SECTION("Expected containing a reference is copy-constructible") {
      STATIC_REQUIRE(std::is_constructible<sut_type, from_type&&>::value);
    }

    SECTION("Constructs an result that references the source value") {
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

TEST_CASE("result<T&,E>::result(in_place_t, Args&&...)", "[ctor]") {
  using sut_type = result<base&,int>;

  auto value = derived{42};
  const sut_type sut{in_place, value};

  SECTION("Contains a value") {
    REQUIRE(sut.has_value());
  }

  SECTION("References old input") {
    REQUIRE(&value == &(*sut));
  }
}

TEST_CASE("result<T&,E>::result(const failure<E2>&)", "[ctor]") {
  using sut_type = result<int&,std::string>;
  const auto source = fail<std::string>("hello world");

  const sut_type sut{source};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == source);
  }
}

TEST_CASE("result<T&,E>::result(failure<E2>&&)", "[ctor]") {
  using sut_type = result<int&,move_only<std::string>>;
  auto source = fail<std::string>("hello world");
  const auto copy = source;

  const sut_type sut{std::move(source)};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from E2") {
    REQUIRE(sut == copy);
  }
}

TEST_CASE("result<T&,E>::result(U&&)", "[ctor]") {
  SECTION("Input is lvalue") {
    using sut_type = result<base&,int>;

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
    using sut_type = result<base&,int>;

    SECTION("Constructor is disabled") {
      REQUIRE_FALSE(std::is_constructible<sut_type,derived&&>::value);
    }
  }
  SECTION("Input is value") {
    using sut_type = result<base&,int>;

    SECTION("Constructor is disabled") {
      REQUIRE_FALSE(std::is_constructible<sut_type,derived>::value);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("result<T&,E>::operator=(const result&)", "[assign]") {
  SECTION("result contains a value") {
    SECTION("other contains a value") {
      auto value = derived{42};
      auto next = derived{0};

      const auto source = result<base&,std::error_code>{next};
      auto sut = result<base&,std::error_code>{value};

      sut = source;

      SECTION("Underlying reference is rebound") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
  SECTION("result contains an error") {
    SECTION("other contains a value") {
      auto next = derived{0};

      const auto source = result<base&,int>{next};
      auto sut = result<base&,int>{fail(42)};

      sut = source;

      SECTION("Binds the new reference") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
}

TEST_CASE("result<T&,E>::operator=(result&&)", "[assign]") {
  SECTION("result contains a value") {
    SECTION("other contains a value") {
      auto value = derived{42};
      auto next = derived{0};

      auto source = result<base&,move_only<std::string>>{next};
      auto sut = result<base&,move_only<std::string>>{value};

      sut = std::move(source);

      SECTION("Underlying reference is rebound") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
  SECTION("result contains an error") {
    SECTION("other contains a value") {
      auto next = derived{0};

      auto source = result<base&,move_only<std::string>>{next};
      auto sut = result<base&,move_only<std::string>>{fail("foo")};

      sut = std::move(source);

      SECTION("Binds the new reference") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
}

TEST_CASE("result<T&,E>::operator=(const result<T2,E2>&)", "[assign]") {
  SECTION("other's type holds T by-value") {
    SECTION("Assignment is disabled") {
      using source_type = result<int,std::error_code>;
      using sut_type = result<int&,std::error_code>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, const source_type&>::value);
    }
  }
  SECTION("result contains a value") {
    SECTION("other contains a value") {
      auto value = derived{42};
      auto next = derived{0};

      const auto source = result<derived&,std::error_code>{next};
      auto sut = result<base&,std::error_code>{value};

      sut = source;

      SECTION("Underlying reference is rebound") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
  SECTION("result contains an error") {
    SECTION("other contains a value") {
      auto next = derived{0};

      const auto source = result<derived&,int>{next};
      auto sut = result<base&,int>{fail(42)};

      sut = source;

      SECTION("Binds the new reference") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
}

TEST_CASE("result<T&,E>::operator=(result<T2,E2>&&)", "[assign]") {
  SECTION("other's type holds T by-value") {
    SECTION("Assignment is disabled") {
      using source_type = result<int,move_only<std::string>>;
      using sut_type = result<int&,move_only<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type, source_type&&>::value);
    }
  }
  SECTION("result contains a value") {
    SECTION("other contains a value") {
      auto value = derived{42};
      auto next = derived{0};

      auto source = result<derived&,move_only<std::string>>{next};
      auto sut = result<base&,move_only<std::string>>{value};

      sut = std::move(source);

      SECTION("Underlying reference is rebound") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
  SECTION("result contains an error") {
    SECTION("other contains a value") {
      auto next = derived{0};

      auto source = result<derived&,move_only<std::string>>{next};
      auto sut = result<base&,move_only<std::string>>{fail("foo")};

      sut = std::move(source);

      SECTION("Binds the new reference") {
        REQUIRE(&next == &(*sut));
      }
    }
  }
}

TEST_CASE("result<T&,E>::operator=(U&&)", "[assign]") {
  SECTION("Source contains a value") {
    auto value = int{42};
    auto next = int{0};

    auto sut = result<int&,std::error_code>{value};

    sut = next;

    SECTION("Rebinds the referred-to value") {
      REQUIRE(&next == &(*sut));
    }
  }
  SECTION("Source contains an error") {
    auto next = int{0};

    auto sut = result<int&,int>{
      fail(42)
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

TEST_CASE("result<T&,E>::operator*() &", "[observers]") {
  auto value = int{42};
  auto sut = result<int&,int>{value};

  SECTION("Returns mutable lvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*sut),int&>::value);
  }
  SECTION("Returns reference to referred value") {
    REQUIRE(&(*sut) == &value);
  }
}

TEST_CASE("result<T&,E>::operator*() const &", "[observers]") {
  auto value = int{42};
  const auto sut = result<int&,int>{value};

  SECTION("Returns lvalue reference that does not propagate constness") {
    STATIC_REQUIRE(std::is_same<decltype(*sut),int&>::value);
  }
  SECTION("Returns reference to referred value") {
    REQUIRE(&(*sut) == &value);
  }
}

TEST_CASE("result<T&,E>::operator*() &&", "[observers]") {
  auto value = int{42};
  auto sut = result<int&,int>{value};

  SECTION("Returns mutable lvalue reference") {
    STATIC_REQUIRE(std::is_same<decltype(*std::move(sut)),int&>::value);
  }
  SECTION("Returns reference to referred value") {
    int& x = (*std::move(sut));
    REQUIRE(&x == &value);
  }
}

TEST_CASE("result<T&,E>::operator*() const &&", "[observers]") {
  auto value = int{42};
  const auto sut = result<int&,int>{value};

  SECTION("Returns mutable Lvalue reference that does not propagate constness") {
    STATIC_REQUIRE(std::is_same<decltype(*std::move(sut)),int&>::value);
  }
  SECTION("Returns reference to referred value") {
    const int& x = (*std::move(sut));
    REQUIRE(&x == &value);
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("result<T&,E>::value() &", "[observers]") {
  SECTION("result contains a value") {
    auto value = int{42};
    auto sut = result<int&,int>{value};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(suppress_unused(sut.value()));
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

TEST_CASE("result<T&,E>::value() const &", "[observers]") {
  SECTION("result contains a value") {
    auto value = int{42};
    const auto sut = result<int&,int>{value};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(suppress_unused(sut.value()));
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

TEST_CASE("result<T&,E>::value() &&", "[observers]") {
  SECTION("result contains a value") {
    auto value = int{42};
    auto sut = result<int&,int>{value};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(suppress_unused(std::move(sut).value()));
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

TEST_CASE("result<T&,E>::value() const &&", "[observers]") {
  SECTION("result contains a value") {
    auto value = int{42};
    const auto sut = result<int&,int>{value};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(suppress_unused(std::move(sut).value()));
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
// class : result<void, E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Destructor / Assignment
//-----------------------------------------------------------------------------

TEST_CASE("result<void,E>::result()", "[ctor]") {
  SECTION("Expected is default-constructible") {
    using sut_type = result<void,int>;

    STATIC_REQUIRE(std::is_default_constructible<sut_type>::value);
  }

  SECTION("Constructs an result with a 'value' state") {
    using sut_type = result<void,int>;

    auto sut = sut_type{};

    REQUIRE(sut.has_value());
  }
}

TEST_CASE("result<void,E>::result(const result&)", "[ctor]") {

  // Constructible:

  SECTION("E is trivially copy-constructible") {
    using sut_type = result<void,int>;

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
      const auto error = fail(42);
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
    using sut_type = result<void,std::string>;

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
      const auto error = fail("Hello world");
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
    using sut_type = result<void,not_copy_or_moveable>;

    SECTION("Expected is not copy-constructible") {
      STATIC_REQUIRE_FALSE(std::is_copy_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("result<void,E>::result(result&&)", "[ctor]") {

  // Constructible:

  SECTION("E is trivially move-constructible") {
    using sut_type = result<void,int>;

    SECTION("Expected is trivially move-constructible") {
      STATIC_REQUIRE(std::is_trivially_move_constructible<sut_type>::value);
    }

    SECTION("Move source contained a value") {
      sut_type source{};

      const auto sut = std::move(source);

      SECTION("New result contains a value") {
        REQUIRE(sut.has_value());
      }
    }

    SECTION("Move source contained an error") {
      const auto error = fail(42);
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New result contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New result contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  } // E is trivially move-constructible

  SECTION("E is move-constructible, but not trivial") {
    using sut_type = result<void,move_only<std::string>>;

    SECTION("Expected is not trivially move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_trivially_move_constructible<sut_type>::value);
    }

    SECTION("Expected is move-constructible") {
      STATIC_REQUIRE(std::is_move_constructible<sut_type>::value);
    }

    SECTION("Move source contained a value") {
      sut_type source{};

      const auto sut = std::move(source);

      SECTION("New result contains a value") {
        REQUIRE(sut.has_value());
      }
    }

    SECTION("Move source contained an error") {
      const auto error = fail("Hello world");
      sut_type source{error};

      const auto sut = std::move(source);

      SECTION("New result contains an error") {
        REQUIRE(sut.has_error());
      }
      SECTION("New result contains an error equal to the source") {
        REQUIRE(sut == error);
      }
    }
  }

  // Not constructible:

  SECTION("E is not move-constructible") {
    using sut_type = result<void,not_copy_or_moveable>;

    SECTION("Expected is not move-constructible") {
      STATIC_REQUIRE_FALSE(std::is_move_constructible<sut_type>::value);
    }
  }
}

TEST_CASE("result<void,E>::result(const result<U,E2>&)", "[ctor]") {
  SECTION("E2 is convertible to E") {
    SECTION("Constructor is enabled") {
      STATIC_REQUIRE(std::is_constructible<result<void,int>,const result<void,long>&>::value);
    }
    SECTION("Other contains value") {
      const auto other = result<int,long>{42};
      const auto sut = result<void,int>{other};

      SECTION("Constructed result is in a value state") {
        REQUIRE(sut.has_value());
      }
    }

    SECTION("Other contains error") {
      const auto other = result<int,long>{
        fail(42)
      };
      const auto sut = result<void,int>{other};
      SECTION("Constructed result is in an error state") {
        REQUIRE(sut.has_error());
      }
      SECTION("Contained error is converted from other") {
        REQUIRE(sut.error() == other.error());
      }
    }
  }

  SECTION("E2 is not convertible to E") {
    SECTION("Constructor is disabled") {
      STATIC_REQUIRE_FALSE(std::is_constructible<result<void,int>,const result<void,std::string>&>::value);
    }
  }
}

TEST_CASE("result<void,E>::result(result<U,E2>&&)", "[ctor]") {
  SECTION("E2 is convertible to E") {
    SECTION("Constructor is enabled") {
      STATIC_REQUIRE(std::is_constructible<result<void,int>,result<void,long>&&>::value);
    }
    SECTION("Other contains value") {
      auto other = result<int,long>{42};
      auto sut = result<void,int>{std::move(other)};

      SECTION("Constructed result is in a value state") {
        REQUIRE(sut.has_value());
      }
    }

    SECTION("Other contains error") {
      auto error = fail("Hello world");
      auto other = result<int,move_only<std::string>>{error};
      auto sut = result<void,std::string>{
        std::move(other)
      };

      SECTION("Constructed result is in an error state") {
        REQUIRE(sut.has_error());
      }
      SECTION("Contained error is converted from other") {
        REQUIRE(sut == error);
      }
    }
  }

  SECTION("E2 is not convertible to E") {
    SECTION("Constructor is disabled") {
      STATIC_REQUIRE_FALSE(std::is_constructible<result<void,int>,result<void,std::string>&&>::value);
    }
  }
}

TEST_CASE("result<void,E>::result(in_place_t)", "[ctor]") {
  using sut_type = result<void,int>;

  const sut_type sut(in_place);

  SECTION("Contains a value") {
    REQUIRE(sut.has_value());
  }
}

TEST_CASE("result<void,E>::result(in_place_error_t, Args&&...)", "[ctor]") {
  using sut_type = result<void,std::string>;

  const auto input = "hello world";
  const auto size = 5;
  const auto result = std::string{input, size};

  const sut_type sut(in_place_error, input, size);

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == fail(std::ref(result)));
  }
}

TEST_CASE("result<void,E>::result(in_place_error_t, std::initializer_list<U>, Args&&...)", "[ctor]") {
  using sut_type = result<void,std::string>;

  const auto result = std::string{
    {'h','e','l','l','o'}, std::allocator<char>{}
  };

  const sut_type sut(in_place_error, {'h','e','l','l','o'}, std::allocator<char>{});

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == fail(std::ref(result)));
  }
}

TEST_CASE("result<void,E>::result(const failure<E2>&)", "[ctor]") {
  using sut_type = result<void,std::string>;
  const auto source = fail<std::string>("hello world");

  const sut_type sut{source};

  SECTION("Contains an error") {
    REQUIRE(sut.has_error());
  }

  SECTION("Contains an error constructed from a copy of E2") {
    REQUIRE(sut == source);
  }
}

TEST_CASE("result<void,E>::result(failure<E2>&&)", "[ctor]") {
  using sut_type = result<void,move_only<std::string>>;
  auto source = fail<std::string>("hello world");
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

TEST_CASE("result<void,E>::~result()", "[dtor]") {
  SECTION("E is trivially destructible") {
    using sut_type = result<void, int>;

    SECTION("result's destructor is trivial") {
      STATIC_REQUIRE(std::is_trivially_destructible<sut_type>::value);
    }
  }

  SECTION("E is not trivially destructible") {
    using sut_type = result<void, report_destructor>;

    auto invoked = false;
    {
      sut_type sut{in_place_error, &invoked};
      (void) sut;
    }

    SECTION("result's destructor is not trivial") {
      STATIC_REQUIRE_FALSE(std::is_trivially_destructible<sut_type>::value);
    }

    SECTION("Invokes E's underlying destructor") {
      REQUIRE(invoked);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("result<void,E>::operator=(const result& other)", "[assign]") {
  SECTION("E is not nothrow copy constructible") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = result<void,std::string>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }

  SECTION("E is not copy-assignable") {
    SECTION("Expected is not copy-assignable") {
      using sut_type = result<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_copy_assignable<sut_type>::value);
    }
  }

  SECTION("E is nothrow copy-constructible") {
    using sut_type = result<void,std::error_code>;

    SECTION("Expected is copy-assignable") {
      STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
    }
    SECTION("Copies the state of 'other'") {
      SECTION("Original contains value") {
        const auto output = fail(std::io_errc::stream);
        const sut_type copy{output};
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
        const auto output = fail(std::io_errc::stream);
        const sut_type copy{output};
        sut_type sut{fail(std::io_errc{})};

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
    using sut_type = result<void, int>;

    SECTION("Expected is copy-assignable") {
      STATIC_REQUIRE(std::is_copy_assignable<sut_type>::value);
    }
    SECTION("Expected is trivial copy-assignable") {
      STATIC_REQUIRE(std::is_trivially_copy_assignable<sut_type>::value);
    }
  }
}

TEST_CASE("result<void,E>::operator=(result&& other)", "[assign]") {
  SECTION("E is not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using sut_type = result<void,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }

  SECTION("E is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using sut_type = result<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_move_assignable<sut_type>::value);
    }
  }

  SECTION("E is nothrow move-constructible") {
    using sut_type = result<void,move_only<std::error_code>>;

    SECTION("Expected is move-assignable") {
      STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
    }
    SECTION("Copies the state of 'other'") {
      SECTION("Original contains value") {
        const auto output = fail(std::io_errc::stream);
        sut_type other{output};
        sut_type sut{};

        sut = std::move(other);

        SECTION("Result changes to error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result constructs error") {
          REQUIRE(output == sut);
        }
      }
      SECTION("Original contains error") {
        const auto output = fail(std::io_errc::stream);
        sut_type other{output};
        sut_type sut{fail(std::io_errc{})};

        sut = std::move(other);

        SECTION("Result has error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result assigns error") {
          REQUIRE(output == sut);
        }
      }
    }
  }

  SECTION("E is trivial move-constructible and move-assignable") {
    using sut_type = result<void, int>;

    SECTION("Expected is move-assignable") {
      STATIC_REQUIRE(std::is_move_assignable<sut_type>::value);
    }
    SECTION("Expected is trivial move-assignable") {
      STATIC_REQUIRE(std::is_trivially_move_assignable<sut_type>::value);
    }
  }
}

TEST_CASE("result<void,E>::operator=(const result<T2,E2>&)", "[assign]") {
  SECTION("E is not nothrow copy constructible") {
    SECTION("Expected is not copy-assignable") {
      using copy_type = result<int,const char*>;
      using sut_type = result<void,std::string>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E is not copy-assignable") {
    SECTION("Expected is not copy-assignable") {
      using copy_type = result<int,const char*>;
      using sut_type = result<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E is nothrow copy-constructible") {
    using copy_type = result<int,std::io_errc>;
    using sut_type = result<void,std::error_code>;

    SECTION("Expected is copy-assignable") {
      STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
    }
    SECTION("Copies the state of 'other'") {
      SECTION("Original contains value") {
        const auto output = fail(std::io_errc::stream);
        const copy_type other{output};
        sut_type sut{};

        sut = other;

        SECTION("Result changes to error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result constructs error") {
          REQUIRE(output == sut);
        }
      }
      SECTION("Original contains error") {
        const auto output = fail(std::io_errc::stream);
        const sut_type other{output};
        sut_type sut{fail(std::io_errc{})};

        sut = other;

        SECTION("Result has error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result assigns error") {
          REQUIRE(output == sut);
        }
      }
    }
  }
}

TEST_CASE("result<void,E>::operator=(result<T2,E2>&&)", "[assign]") {
  SECTION("E is not nothrow move constructible") {
    SECTION("Expected is not move-assignable") {
      using copy_type = result<int,const char*>;
      using sut_type = result<void,std::string>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E is not move-assignable") {
    SECTION("Expected is not move-assignable") {
      using copy_type = result<int,const char*>;
      using sut_type = result<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E is nothrow move-constructible") {
    using copy_type = result<int,std::io_errc>;
    using sut_type = result<void,move_only<std::error_code>>;

    SECTION("Expected is move-assignable") {
      STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
    }
    SECTION("Copies the state of 'other'") {
      SECTION("Original contains value") {
        const auto output = fail(std::io_errc::stream);
        copy_type other{output};
        sut_type sut{};

        sut = std::move(other);

        SECTION("Result changes to error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result constructs error") {
          REQUIRE(output == sut);
        }
      }
      SECTION("Original contains error") {
        const auto output = fail(std::io_errc::stream);
        sut_type other{output};
        sut_type sut{fail(std::io_errc{})};

        sut = std::move(other);

        SECTION("Result has error type") {
          REQUIRE(sut.has_error());
        }
        SECTION("Result assigns error") {
          REQUIRE(output == sut);
        }
      }
    }
  }
}

TEST_CASE("result<void,E>::operator=(const failure<E2>&)", "[assign]") {
  SECTION("E cant be constructed or assigned from E2") {
    SECTION("Expected cannot be assigned from E2") {
      using copy_type = failure<not_copy_or_moveable>;
      using sut_type  = result<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }
  SECTION("E is not nothrow move constructible from E2") {
    SECTION("Expected is not move-assignable") {
      using copy_type = failure<const char*>;
      using sut_type = result<void,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E is not nothrow move constructible from E2, but is nothrow-move constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = failure<const char*>;
      using sut_type = result<void,std::string>;

      // This works because it generates an intermediate 'result<void, E>' in
      // between. The result is then move-assigned instead

      STATIC_REQUIRE(std::is_assignable<sut_type,const copy_type&>::value);
    }
  }

  SECTION("E can be constructed and assigned from E2") {
    using copy_type = failure<long>;
    using sut_type = result<void, int>;

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

TEST_CASE("result<void,E>::operator=(failure<E2>&&)", "[assign]") {
  SECTION("E cant be constructed or assigned from E2") {
    SECTION("Expected cannot be assigned from E2") {
      using copy_type = failure<not_copy_or_moveable>;
      using sut_type  = result<void,not_copy_or_moveable>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }
  SECTION("E is not nothrow move constructible from E2") {
    SECTION("Expected is not move-assignable") {
      using copy_type = failure<const char*>;
      using sut_type = result<void,throwing<std::string>>;

      STATIC_REQUIRE_FALSE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E is not nothrow move constructible from E2, but is nothrow-move constructible") {
    SECTION("Expected is move-assignable") {
      using copy_type = failure<const char*>;
      using sut_type = result<void,std::string>;

      // This works because it generates an intermediate 'result<void, E>' in
      // between. The result is then move-assigned instead

      STATIC_REQUIRE(std::is_assignable<sut_type,copy_type&&>::value);
    }
  }

  SECTION("E can be constructed and assigned from E2") {
    using copy_type = failure<long>;
    using sut_type = result<void, int>;

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

TEST_CASE("result<void,E>::operator bool()", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Returns true") {
      auto sut = result<void, int>{};

      REQUIRE(static_cast<bool>(sut));
    }
  }
  SECTION("result does not contain a value") {
    SECTION("Returns false") {
      auto sut = result<void, int>{fail(42)};

      REQUIRE_FALSE(static_cast<bool>(sut));
    }
  }
}

TEST_CASE("result<void,E>::has_value()", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Returns true") {
      auto sut = result<void, int>{};

      REQUIRE(sut.has_value());
    }
  }
  SECTION("result does not contain a value") {
    SECTION("Returns false") {
      auto sut = result<void, int>{fail(42)};

      REQUIRE_FALSE(sut.has_value());
    }
  }
}

TEST_CASE("result<void,E>::has_error()", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Returns false") {
      auto sut = result<void, int>{};

      REQUIRE_FALSE(sut.has_error());
    }
  }
  SECTION("result does not contain a value") {
    SECTION("Returns true") {
      auto sut = result<void, int>{fail(42)};

      REQUIRE(sut.has_error());
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("result<void,E>::value()", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Does nothing") {
      auto sut = result<void, int>{};

      sut.value();

      SUCCEED();
    }
  }
  SECTION("result contains an error") {
    SECTION("Throws bad_result_access") {
      auto sut = result<void, int>{fail(42)};

      REQUIRE_THROWS_AS(sut.value(), bad_result_access<int>);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("result<void,E>::error() const &", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Returns default-constructed error") {
      auto sut = result<void, int>{};

      const auto output = sut.error();

      REQUIRE(output == int{});
    }
  }
  SECTION("result contains an error") {
    SECTION("Returns a copy of the exception") {
      const auto value = 42;
      auto sut = result<void, int>{fail(value)};

      const auto output = sut.error();

      REQUIRE(output == value);
    }
  }
}

TEST_CASE("result<void,E>::error() &&", "[observers]") {
  SECTION("result contains a value") {
    SECTION("Returns default-constructed error") {
      auto sut = result<void, move_only<std::error_code>>{};

      const auto output = std::move(sut).error();

      REQUIRE(output == std::error_code{});
    }
  }
  SECTION("result contains an error") {
    SECTION("Returns a copy of the exception") {
      const auto value = std::io_errc::stream;
      auto sut = result<void, move_only<std::error_code>>{fail(value)};

      const auto output = std::move(sut).error();

      REQUIRE(output == value);
    }
  }
}

TEST_CASE("result<void,E>::expect(String&&) const &", "[observers]") {
  SECTION("result contains value") {
    const auto sut = result<void,int>{};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(sut.expect("test"));
    }
  }
  SECTION("result contains error") {
    SECTION("throws exception") {
      const auto sut = result<void,int>{fail(42)};

      REQUIRE_THROWS_AS(sut.expect("test"), bad_result_access<int>);
    }
  }
}

TEST_CASE("result<void,E>::expect(String&&) &&", "[observers]") {
  SECTION("result contains value") {
    auto sut = result<void,move_only<std::string>>{};

    SECTION("Does not throw exception") {
      REQUIRE_NOTHROW(std::move(sut).expect("test"));
    }
  }
  SECTION("result contains error") {
    SECTION("throws exception") {
      auto sut = result<void,move_only<std::string>>{fail("hello world")};

      REQUIRE_THROWS_AS(std::move(sut).expect("test"), bad_result_access<move_only<std::string>>);
    }
  }
}

//-----------------------------------------------------------------------------
// Monadic Functionalities
//-----------------------------------------------------------------------------

TEST_CASE("result<void,E>::error_or(U&&) const &", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Returns the input") {
      const auto input = std::error_code{};
      auto sut = result<void, std::error_code>{};

      const auto output = sut.error_or(input);

      REQUIRE(output == input);
    }
  }
  SECTION("result contains an error") {
    SECTION("Returns the error") {
      const auto input = std::error_code{std::io_errc::stream};
      auto sut = result<void, std::error_code>{
        fail(input)
      };

      const auto output = sut.error_or(input);

      REQUIRE(output == input);
    }
  }
}

TEST_CASE("result<void,E>::error_or(U&&) &&", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Returns the input") {
      auto input = std::error_code{};
      const auto copy = input;
      auto sut = result<void, move_only<std::error_code>>{};

      const auto output = std::move(sut).error_or(std::move(input));

      REQUIRE(output == copy);
    }
  }
  SECTION("result contains an error") {
    SECTION("Returns the error") {
      auto input = std::error_code{std::io_errc::stream};
      const auto copy = input;
      auto sut = result<void, move_only<std::error_code>>{
        fail(input)
      };

      const auto output = std::move(sut).error_or(std::move(input));

      REQUIRE(output == copy);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("result<void,E>::and_then(U&&) const", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Maps the input") {
      const auto input = 42;
      auto sut = result<void, std::error_code>{};

      const auto output = sut.and_then(input);

      REQUIRE(output == input);
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto input = 42;
      const auto error = fail(std::io_errc::stream);
      auto sut = result<void, std::error_code>{error};

      const auto output = sut.and_then(input);

      REQUIRE(output == error);
    }
  }
}

TEST_CASE("result<void,E>::flat_map(Fn&&) const &", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Maps the input") {
      const auto value = 42;
      const auto sut = result<void,std::error_code>{};

      const auto output = sut.flat_map([&]{
        return result<int,std::error_code>{value};
      });

      REQUIRE(output == value);
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail(std::io_errc::stream);
      const auto value = 42;
      const auto sut = result<void,std::error_code>{error};

      const auto output = sut.flat_map([&]{
        return result<int,std::error_code>{value};
      });

      REQUIRE(output == error);
    }
  }
}

TEST_CASE("result<void,E>::flat_map(Fn&&) &&", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Maps the input") {
      const auto value = 42;
      auto sut = result<void,move_only<std::error_code>>{};

      const auto output = std::move(sut).flat_map([&]{
        return result<int,std::error_code>{value};
      });

      REQUIRE(output == value);
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail(std::io_errc::stream);
      const auto value = 42;
      auto sut = result<void,move_only<std::error_code>>{error};

      const auto output = std::move(sut).flat_map([&]{
        return result<int,std::error_code>{value};
      });

      REQUIRE(output == error);
    }
  }
}

TEST_CASE("result<void,E>::map(Fn&&) const &", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Function returns non-void") {
      SECTION("Maps the input") {
        const auto value = 42;
        auto sut = result<void,std::io_errc>{};

        const auto output = sut.map([&]{
          return value;
        });

        REQUIRE(output == value);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        auto sut = result<void,std::io_errc>{};

        const auto output = sut.map([]{});

        SECTION("Result has value") {
          REQUIRE(output.has_value());
        }
        SECTION("Result is result<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(output),const result<void,std::io_errc>>::value);
        }
      }
    }
  }
  SECTION("result contains an error") {
    SECTION("Function returns non-void") {
      SECTION("Maps the error") {
        const auto value = 42;
        const auto error = fail(std::io_errc::stream);
        auto sut = result<void,std::io_errc>{error};

        const auto output = sut.map([&]{
          return value;
        });

        REQUIRE(output == error);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto error = fail(std::io_errc::stream);
        auto sut = result<void,std::io_errc>{error};

        const auto output = sut.map([]{});

        SECTION("Result contains the error") {
          REQUIRE(output == error);
        }
        SECTION("Result is result<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(output),const result<void,std::io_errc>>::value);
        }
      }
    }
  }
}

TEST_CASE("result<void,E>::map(Fn&&) &&", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Function returns non-void") {
      SECTION("Maps the input") {
        const auto value = 42;
        auto sut = result<void,move_only<std::error_code>>{};

        const auto output = std::move(sut).map([&]{
          return value;
        });

        REQUIRE(output == value);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        auto sut = result<void,move_only<std::error_code>>{};

        const auto output = std::move(sut).map([]{});

        SECTION("Result has value") {
          REQUIRE(output.has_value());
        }
        SECTION("Result is result<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(output),const result<void,move_only<std::error_code>>>::value);
        }
      }
    }
  }
  SECTION("result contains an error") {
    SECTION("Function returns non-void") {
      SECTION("Maps the error") {
        const auto value = 42;
        const auto error = fail(std::io_errc::stream);
        auto sut = result<void,move_only<std::error_code>>{error};

        const auto output = std::move(sut).map([&]{
          return value;
        });

        REQUIRE(output == error);
      }
    }
    SECTION("Function returns void") {
      SECTION("Maps input to void") {
        const auto error = fail(std::io_errc::stream);
        auto sut = result<void,move_only<std::error_code>>{error};

        const auto output = std::move(sut).map([]{});

        SECTION("Result contains the error") {
          REQUIRE(output == error);
        }
        SECTION("Result is result<void,E>") {
          STATIC_REQUIRE(std::is_same<decltype(output),const result<void,move_only<std::error_code>>>::value);
        }
      }
    }
  }
}

TEST_CASE("result<void,E>::map_error(Fn&&) const", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Maps the input") {
      auto sut = result<void,std::io_errc>{};

      const auto output = sut.map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(output.has_value());
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail(std::io_errc::stream);
      auto sut = result<void,std::io_errc>{error};

      const auto output = sut.map_error([](std::io_errc e){
        return std::error_code{e};
      });

      REQUIRE(output == error);
    }
  }
}

TEST_CASE("result<void,E>::flat_map_error(Fn&&) const &", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Default-initializes T") {
      const auto sut = result<void,int>{};

      const auto output = sut.flat_map_error([&](int x){
        return result<long, int>{x};
      });

      REQUIRE(output == long{});
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail(42);
      const auto sut = result<void, long>{error};

      const auto output = sut.flat_map_error([&](long x){
        return result<int, short>{x};
      });

      REQUIRE(output == error.error());
    }
  }
}

TEST_CASE("result<void,E>::flat_map_error(Fn&&) &&", "[monadic]") {
  SECTION("result contains a value") {
    SECTION("Default-initializes T") {
      auto sut = result<void,move_only<std::string>>{};

      const auto output = std::move(sut).flat_map_error([](move_only<std::string>&& x){
        return result<int, std::string>{in_place_error, std::move(x)};
      });

      REQUIRE(output == int{});
    }
  }
  SECTION("result contains an error") {
    SECTION("Maps the error") {
      const auto error = fail("Hello world");
      auto sut = result<void,move_only<std::string>>{error};

      const auto output = std::move(sut).flat_map_error([](move_only<std::string>&& x){
        return result<int, std::string>{in_place_error, std::move(x)};
      });

      REQUIRE(output == error);
    }
  }
}

//=============================================================================
// non-member functions : class : result<void, E>
//=============================================================================

//-----------------------------------------------------------------------------
// Comparisons
//-----------------------------------------------------------------------------

TEST_CASE("operator==(const result<T1,E1>&, const result<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left equal to right") {
      const auto lhs = result<int,int>{42};
      const auto rhs = result<long,long>{42};

      SECTION("returns true") {
        REQUIRE(lhs == rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = result<int,int>{42};
      const auto rhs = result<long,long>{0};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left equal to right") {
      const auto lhs = result<int,int>{
        fail(42)
      };
      const auto rhs = result<long,long>{
        fail(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs == rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = result<int,int>{
        fail(42)
      };
      const auto rhs = result<long,long>{
        fail(0)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<int,int>{};
    const auto rhs = result<long,long>{
      fail(42)
    };
    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<int,int>{
      fail(42)
    };
    const auto rhs = result<long,long>{};
    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
}

TEST_CASE("operator!=(const result<T1,E1>&, const result<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left equal to right") {
      const auto lhs = result<int,int>{42};
      const auto rhs = result<long,long>{42};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = result<int,int>{42};
      const auto rhs = result<long,long>{0};

      SECTION("returns true") {
        REQUIRE(lhs != rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left equal to right") {
      const auto lhs = result<int,int>{
        fail(42)
      };
      const auto rhs = result<long,long>{
        fail(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = result<int,int>{
        fail(42)
      };
      const auto rhs = result<long,long>{
        fail(0)
      };
      SECTION("returns true") {
        REQUIRE(lhs != rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<int,int>{};
    const auto rhs = result<long,long>{
      fail(42)
    };
    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<int,int>{
      fail(42)
    };
    const auto rhs = result<long,long>{};
    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
}

TEST_CASE("operator>=(const result<T1,E1>&, const result<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left greater than right") {
      const auto lhs = result<int,int>{100};
      const auto rhs = result<long,long>{42};

      SECTION("returns true") {
        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<int,int>{0};
      const auto rhs = result<long,long>{42};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = result<int,int>{
        fail(100)
      };
      const auto rhs = result<long,long>{
        fail(42)
      };

      SECTION("returns true") {
        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<int,int>{
        fail(0)
      };
      const auto rhs = result<long,long>{
        fail(42)
      };

      SECTION("returns false") {
        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<int,int>{0};
    const auto rhs = result<long,long>{
      fail(42)
    };

    SECTION("returns true") {
      REQUIRE(lhs >= rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<int,int>{
      fail(42)
    };
    const auto rhs = result<long,long>{0};

    SECTION("returns false") {
      REQUIRE_FALSE(lhs >= rhs);
    }
  }
}

TEST_CASE("operator<=(const result<T1,E1>&, const result<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left greater than right") {
      const auto lhs = result<int,int>{100};
      const auto rhs = result<long,long>{42};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs <= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<int,int>{0};
      const auto rhs = result<long,long>{42};

      SECTION("returns true") {
        REQUIRE(lhs <= rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = result<int,int>{
        fail(100)
      };
      const auto rhs = result<long,long>{
        fail(42)
      };

      SECTION("returns false") {
        REQUIRE_FALSE(lhs <= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<int,int>{
        fail(0)
      };
      const auto rhs = result<long,long>{
        fail(42)
      };

      SECTION("returns true") {
        REQUIRE(lhs <= rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<int,int>{0};
    const auto rhs = result<long,long>{
      fail(42)
    };

    SECTION("returns false") {
      REQUIRE_FALSE(lhs <= rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<int,int>{
      fail(42)
    };
    const auto rhs = result<long,long>{0};

    SECTION("returns true") {
      REQUIRE(lhs <= rhs);
    }
  }
}

TEST_CASE("operator>(const result<T1,E1>&, const result<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left greater than right") {
      const auto lhs = result<int,int>{100};
      const auto rhs = result<long,long>{42};

      SECTION("returns true") {
        REQUIRE(lhs > rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<int,int>{0};
      const auto rhs = result<long,long>{42};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = result<int,int>{
        fail(100)
      };
      const auto rhs = result<long,long>{
        fail(42)
      };

      SECTION("returns true") {
        REQUIRE(lhs > rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<int,int>{
        fail(0)
      };
      const auto rhs = result<long,long>{
        fail(42)
      };

      SECTION("returns false") {
        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<int,int>{0};
    const auto rhs = result<long,long>{
      fail(42)
    };

    SECTION("returns true") {
      REQUIRE(lhs > rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<int,int>{
      fail(42)
    };
    const auto rhs = result<long,long>{0};

    SECTION("returns false") {
      REQUIRE_FALSE(lhs > rhs);
    }
  }
}

TEST_CASE("operator<(const result<T1,E1>&, const result<T2,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    SECTION("Left greater than right") {
      const auto lhs = result<int,int>{100};
      const auto rhs = result<long,long>{42};

      SECTION("returns false") {
        REQUIRE_FALSE(lhs < rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<int,int>{0};
      const auto rhs = result<long,long>{42};

      SECTION("returns true") {
        REQUIRE(lhs < rhs);
      }
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = result<int,int>{
        fail(100)
      };
      const auto rhs = result<long,long>{
        fail(42)
      };

      SECTION("returns false") {
        REQUIRE_FALSE(lhs < rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<int,int>{
        fail(0)
      };
      const auto rhs = result<long,long>{
        fail(42)
      };

      SECTION("returns true") {
        REQUIRE(lhs < rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<int,int>{0};
    const auto rhs = result<long,long>{
      fail(42)
    };

    SECTION("returns false") {
      REQUIRE_FALSE(lhs < rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<int,int>{
      fail(42)
    };
    const auto rhs = result<long,long>{0};

    SECTION("returns true") {
      REQUIRE(lhs < rhs);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("operator==(const result<void,E1>&, const result<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{};

    SECTION("returns true") {
      REQUIRE(lhs == rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left equal to right") {
      const auto lhs = result<void,int>{
        fail(42)
      };
      const auto rhs = result<void,long>{
        fail(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs == rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = result<void,int>{
        fail(42)
      };
      const auto rhs = result<void,long>{
        fail(0)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{
      fail(42)
    };
    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<void,int>{
      fail(42)
    };
    const auto rhs = result<void,long>{};
    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
}

TEST_CASE("operator!=(const result<void,E1>&, const result<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{};

    SECTION("returns false") {
      REQUIRE_FALSE(lhs != rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left equal to right") {
      const auto lhs = result<void,int>{
        fail(42)
      };
      const auto rhs = result<void,long>{
        fail(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("Left not equal to right") {
      const auto lhs = result<void,int>{
        fail(42)
      };
      const auto rhs = result<void,long>{
        fail(0)
      };
      SECTION("returns true") {
        REQUIRE(lhs != rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{
      fail(42)
    };
    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<void,int>{
      fail(42)
    };
    const auto rhs = result<void,long>{};
    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
}

TEST_CASE("operator>=(const result<void,E1>&, const result<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{};

    SECTION("returns true") {
      REQUIRE(lhs >= rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = result<void,int>{
        fail(100)
      };
      const auto rhs = result<void,long>{
        fail(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<void,int>{
        fail(0)
      };
      const auto rhs = result<void,long>{
        fail(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{
      fail(42)
    };
    SECTION("returns true") {
      REQUIRE(lhs >= rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<void,int>{
      fail(42)
    };
    const auto rhs = result<void,long>{};
    SECTION("returns false") {
      REQUIRE_FALSE(lhs >= rhs);
    }
  }
}

TEST_CASE("operator<=(const result<void,E1>&, const result<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{};

    SECTION("returns true") {
      REQUIRE(lhs <= rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = result<void,int>{
        fail(100)
      };
      const auto rhs = result<void,long>{
        fail(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs <= rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<void,int>{
        fail(0)
      };
      const auto rhs = result<void,long>{
        fail(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs <= rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{
      fail(42)
    };
    SECTION("returns false") {
      REQUIRE_FALSE(lhs <= rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<void,int>{
      fail(42)
    };
    const auto rhs = result<void,long>{};
    SECTION("returns true") {
      REQUIRE(lhs <= rhs);
    }
  }
}

TEST_CASE("operator>(const result<void,E1>&, const result<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{};

    SECTION("returns false") {
      REQUIRE_FALSE(lhs > rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = result<void,int>{
        fail(100)
      };
      const auto rhs = result<void,long>{
        fail(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs > rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<void,int>{
        fail(0)
      };
      const auto rhs = result<void,long>{
        fail(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{
      fail(42)
    };
    SECTION("returns true") {
      REQUIRE(lhs > rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<void,int>{
      fail(42)
    };
    const auto rhs = result<void,long>{};
    SECTION("returns false") {
      REQUIRE_FALSE(lhs > rhs);
    }
  }
}

TEST_CASE("operator<(const result<void,E1>&, const result<void,E2>&)", "[compare]") {
  SECTION("Both left and right contain value") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{};

    SECTION("returns false") {
      REQUIRE_FALSE(lhs < rhs);
    }
  }
  SECTION("Left and right contain errors") {
    SECTION("Left greater than right") {
      const auto lhs = result<void,int>{
        fail(100)
      };
      const auto rhs = result<void,long>{
        fail(42)
      };
      SECTION("returns false") {
        REQUIRE_FALSE(lhs < rhs);
      }
    }
    SECTION("Left less than right") {
      const auto lhs = result<void,int>{
        fail(0)
      };
      const auto rhs = result<void,long>{
        fail(42)
      };
      SECTION("returns true") {
        REQUIRE(lhs < rhs);
      }
    }
  }
  SECTION("Left contains a value, right contains an error") {
    const auto lhs = result<void,int>{};
    const auto rhs = result<void,long>{
      fail(42)
    };
    SECTION("returns false") {
      REQUIRE_FALSE(lhs < rhs);
    }
  }
  SECTION("Left contains an error, right contains a value") {
    const auto lhs = result<void,int>{
      fail(42)
    };
    const auto rhs = result<void,long>{};
    SECTION("returns true") {
      REQUIRE(lhs < rhs);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("operator==(const result<T1,E1>&, const U&)", "[compare]") {
  SECTION("result contains a value") {
    const auto lhs = result<int,std::string>{0};

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
  SECTION("result contains an error") {
    const auto lhs = result<int,std::string>{
      fail("0")
    };
    const auto rhs = 0l;

    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
}

TEST_CASE("operator==(const U&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains a value") {
    const auto rhs = result<int,std::string>{0};

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
  SECTION("result contains an error") {
    const auto rhs = result<int,std::string>{
      fail("0")
    };
    const auto lhs = 0l;

    SECTION("returns false") {
      REQUIRE_FALSE(lhs == rhs);
    }
  }
}

TEST_CASE("operator!=(const result<T1,E1>&, const U&)", "[compare]") {
  SECTION("result contains a value") {
    const auto lhs = result<int,std::string>{0};

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
  SECTION("result contains an error") {
    const auto lhs = result<int,std::string>{
      fail("0")
    };
    const auto rhs = 0l;

    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
}

TEST_CASE("operator!=(const U&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains a value") {
    const auto rhs = result<int,std::string>{0};

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
  SECTION("result contains an error") {
    const auto rhs = result<int,std::string>{
      fail("0")
    };
    const auto lhs = 0l;

    SECTION("returns true") {
      REQUIRE(lhs != rhs);
    }
  }
}

TEST_CASE("operator>=(const result<T1,E1>&, const U&)", "[compare]") {
  SECTION("result contains a value") {
    const auto lhs = result<int,std::string>{5};
    SECTION("value is greater or equal to result") {
      SECTION("returns true") {
        const auto rhs = 0l;

        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("value is not greater or equal to result") {
      SECTION("returns false") {
        const auto rhs = 9l;

        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns true") {
      const auto lhs = result<int,std::string>{fail("hello world")};
      const auto rhs = 0l;

      REQUIRE(lhs >= rhs);
    }
  }
}

TEST_CASE("operator>=(const U&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains a value") {
    const auto rhs = result<int,std::string>{5};
    SECTION("value is greater or equal to result") {
      SECTION("returns true") {
        const auto lhs = 0l;

        REQUIRE_FALSE(lhs >= rhs);
      }
    }
    SECTION("value is not greater or equal to result") {
      SECTION("returns false") {
        const auto lhs = 9l;

        REQUIRE(lhs >= rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns true") {
      const auto rhs = result<int,std::string>{fail("hello world")};
      const auto lhs = 0l;

      REQUIRE_FALSE(lhs >= rhs);
    }
  }
}

TEST_CASE("operator<=(const result<T1,E1>&, const U&)", "[compare]") {
  SECTION("result contains a value") {
    const auto lhs = result<int,std::string>{5};
    SECTION("value is less or equal to result") {
      SECTION("returns true") {
        const auto rhs = 9l;

        REQUIRE(lhs <= rhs);
      }
    }
    SECTION("value is not less or equal to result") {
      SECTION("returns false") {
        const auto rhs = 0l;

        REQUIRE_FALSE(lhs <= rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns false") {
      const auto lhs = result<int,std::string>{fail("hello world")};
      const auto rhs = 9l;

      REQUIRE_FALSE(lhs <= rhs);
    }
  }
}

TEST_CASE("operator<=(const U&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains a value") {
    const auto rhs = result<int,std::string>{5};
    SECTION("value is less or equal to result") {
      SECTION("returns true") {
        const auto lhs = 0l;

        REQUIRE(lhs <= rhs);
      }
    }
    SECTION("value is not less or equal to result") {
      SECTION("returns false") {
        const auto lhs = 9l;

        REQUIRE_FALSE(lhs <= rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns true") {
      const auto rhs = result<int,std::string>{fail("hello world")};
      const auto lhs = 9l;

      REQUIRE(lhs <= rhs);
    }
  }
}

TEST_CASE("operator>(const result<T1,E1>&, const U&)", "[compare]") {
  SECTION("result contains a value") {
    const auto lhs = result<int,std::string>{5};
    SECTION("value is greater or equal to result") {
      SECTION("returns true") {
        const auto rhs = 0l;

        REQUIRE(lhs > rhs);
      }
    }
    SECTION("value is not greater or equal to result") {
      SECTION("returns false") {
        const auto rhs = 9l;

        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns false") {
      const auto lhs = result<int,std::string>{fail("hello world")};
      const auto rhs = 0l;

      REQUIRE_FALSE(lhs > rhs);
    }
  }
}

TEST_CASE("operator>(const U&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains a value") {
    const auto rhs = result<int,std::string>{5};
    SECTION("value is greater or equal to result") {
      SECTION("returns false") {
        const auto lhs = 0l;

        REQUIRE_FALSE(lhs > rhs);
      }
    }
    SECTION("value is not greater or equal to result") {
      SECTION("returns false") {
        const auto lhs = 9l;

        REQUIRE(lhs > rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns true") {
      const auto rhs = result<int,std::string>{fail("hello world")};
      const auto lhs = 0l;

      REQUIRE(lhs > rhs);
    }
  }
}

TEST_CASE("operator<(const result<T1,E1>&, const U&)", "[compare]") {
  SECTION("result contains a value") {
    const auto lhs = result<int,std::string>{5};
    SECTION("value is less or equal to result") {
      SECTION("returns true") {
        const auto rhs = 9l;

        REQUIRE(lhs < rhs);
      }
    }
    SECTION("value is not less or equal to result") {
      SECTION("returns false") {
        const auto rhs = 0l;

        REQUIRE_FALSE(lhs < rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns false") {
      const auto lhs = result<int,std::string>{fail("hello world")};
      const auto rhs = 9l;

      REQUIRE_FALSE(lhs < rhs);
    }
  }
}

TEST_CASE("operator<(const U&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains a value") {
    const auto rhs = result<int,std::string>{5};
    SECTION("value is less or equal to result") {
      SECTION("returns true") {
        const auto lhs = 0l;

        REQUIRE(lhs < rhs);
      }
    }
    SECTION("value is not less or equal to result") {
      SECTION("returns false") {
        const auto lhs = 9l;

        REQUIRE_FALSE(lhs < rhs);
      }
    }
  }
  SECTION("Expected contains an error") {
    SECTION("returns false") {
      const auto rhs = result<int,std::string>{fail("hello world")};
      const auto lhs = 9l;

      REQUIRE(lhs < rhs);
    }
  }
}

//-----------------------------------------------------------------------------

TEST_CASE("operator==(const result<T1,E1>&, const failure<E>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns false") {
      const auto lhs = result<int,std::string>{0};
      const auto rhs = fail("hello world");

      REQUIRE_FALSE(lhs == rhs);
    }
  }
  SECTION("result contains error") {
    const auto lhs = result<int,std::string>{
      fail("0")
    };
    SECTION("failure compares equal") {
      SECTION("Returns true") {
        const auto rhs = fail("0");

        REQUIRE(lhs == rhs);
      }
    }
    SECTION("failure compares unequal") {
      SECTION("Returns false") {
        const auto rhs = fail("hello");

        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
}

TEST_CASE("operator==(const failure<E>&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns false") {
      const auto rhs = result<int,std::string>{0};
      const auto lhs = fail("hello world");

      REQUIRE_FALSE(lhs == rhs);
    }
  }
  SECTION("result contains error") {
    const auto rhs = result<int,std::string>{
      fail("0")
    };
    SECTION("failure compares equal") {
      SECTION("Returns true") {
        const auto lhs = fail("0");

        REQUIRE(lhs == rhs);
      }
    }
    SECTION("failure compares unequal") {
      SECTION("Returns false") {
        const auto lhs = fail("hello");

        REQUIRE_FALSE(lhs == rhs);
      }
    }
  }
}

TEST_CASE("operator!=(const result<T1,E1>&, const failure<E>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns true") {
      const auto lhs = result<int,std::string>{0};
      const auto rhs = fail("hello world");

      REQUIRE(lhs != rhs);
    }
  }
  SECTION("result contains error") {
    const auto lhs = result<int,std::string>{
      fail("0")
    };
    SECTION("failure compares equal") {
      SECTION("Returns false") {
        const auto rhs = fail("0");

        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("failure compares unequal") {
      SECTION("Returns true") {
        const auto rhs = fail("hello");

        REQUIRE(lhs != rhs);
      }
    }
  }
}

TEST_CASE("operator!=(const failure<E>&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns true") {
      const auto rhs = result<int,std::string>{0};
      const auto lhs = fail("hello world");

      REQUIRE(lhs != rhs);
    }
  }
  SECTION("result contains error") {
    const auto rhs = result<int,std::string>{
      fail("0")
    };
    SECTION("failure compares equal") {
      SECTION("Returns false") {
        const auto lhs = fail("0");

        REQUIRE_FALSE(lhs != rhs);
      }
    }
    SECTION("failure compares unequal") {
      SECTION("Returns true") {
        const auto lhs = fail("hello");

        REQUIRE(lhs != rhs);
      }
    }
  }
}

TEST_CASE("operator>=(const result<T1,E1>&, const failure<E>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns false") {
      const auto lhs = result<int,std::string>{0};
      const auto rhs = fail("hello world");

      REQUIRE_FALSE(lhs >= rhs);
    }
  }
  SECTION("result contains error") {
    const auto lhs = result<int,std::string>{
      fail("5")
    };
    SECTION("result is greater or equal to failure") {
      SECTION("Returns true") {
        const auto rhs = fail("0");

        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("result is not greater or equal to failure") {
      SECTION("Returns false") {
        const auto rhs = fail("9");

        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
}

TEST_CASE("operator>=(const failure<E>&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns false") {
      const auto lhs = fail("hello world");
      const auto rhs = result<int,std::string>{0};

      REQUIRE(lhs >= rhs);
    }
  }
  SECTION("result contains error") {
    const auto rhs = result<int,std::string>{
      fail("3")
    };
    SECTION("failure is greater or equal to result") {
      SECTION("Returns true") {
        const auto lhs = fail("5");

        REQUIRE(lhs >= rhs);
      }
    }
    SECTION("failure is not greater or equal to result") {
      SECTION("Returns false") {
        const auto lhs = fail("0");

        REQUIRE_FALSE(lhs >= rhs);
      }
    }
  }
}

TEST_CASE("operator<=(const result<T1,E1>&, const failure<E>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns true") {
      const auto lhs = result<int,std::string>{0};
      const auto rhs = fail("hello world");

      REQUIRE(lhs <= rhs);
    }
  }
  SECTION("result contains error") {
    const auto lhs = result<int,std::string>{
      fail("5")
    };
    SECTION("result is less or equal to failure") {
      SECTION("Returns true") {
        const auto rhs = fail("9");

        REQUIRE(lhs <= rhs);
      }
    }
    SECTION("result is not less or equal to failure") {
      SECTION("Returns false") {
        const auto rhs = fail("0");

        REQUIRE_FALSE(lhs <= rhs);
      }
    }
  }
}

TEST_CASE("operator<=(const failure<E>&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns true") {
      const auto lhs = fail("hello world");
      const auto rhs = result<int,std::string>{0};

      REQUIRE_FALSE(lhs <= rhs);
    }
  }
  SECTION("result contains error") {
    const auto rhs = result<int,std::string>{
      fail("5")
    };
    SECTION("result is less or equal to failure") {
      SECTION("Returns true") {
        const auto lhs = fail("0");

        REQUIRE(lhs <= rhs);
      }
    }
    SECTION("result is not less or equal to failure") {
      SECTION("Returns false") {
        const auto lhs = fail("9");

        REQUIRE_FALSE(lhs <= rhs);
      }
    }
  }
}

TEST_CASE("operator>(const result<T1,E1>&, const failure<E>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns false") {
      const auto lhs = result<int,std::string>{0};
      const auto rhs = fail("hello world");

      REQUIRE_FALSE(lhs > rhs);
    }
  }
  SECTION("result contains error") {
    const auto lhs = result<int,std::string>{
      fail("5")
    };
    SECTION("result is greater or equal to failure") {
      SECTION("Returns true") {
        const auto rhs = fail("0");

        REQUIRE(lhs > rhs);
      }
    }
    SECTION("result is not greater or equal to failure") {
      SECTION("Returns false") {
        const auto rhs = fail("9");

        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
}

TEST_CASE("operator>(const failure<E>&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns false") {
      const auto lhs = fail("hello world");
      const auto rhs = result<int,std::string>{0};

      REQUIRE(lhs > rhs);
    }
  }
  SECTION("result contains error") {
    const auto rhs = result<int,std::string>{
      fail("3")
    };
    SECTION("failure is greater or equal to result") {
      SECTION("Returns true") {
        const auto lhs = fail("5");

        REQUIRE(lhs > rhs);
      }
    }
    SECTION("failure is not greater or equal to result") {
      SECTION("Returns false") {
        const auto lhs = fail("0");

        REQUIRE_FALSE(lhs > rhs);
      }
    }
  }
}

TEST_CASE("operator<(const result<T1,E1>&, const failure<E>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns true") {
      const auto lhs = result<int,std::string>{0};
      const auto rhs = fail("hello world");

      REQUIRE(lhs < rhs);
    }
  }
  SECTION("result contains error") {
    const auto lhs = result<int,std::string>{
      fail("5")
    };
    SECTION("result is less or equal to failure") {
      SECTION("Returns true") {
        const auto rhs = fail("9");

        REQUIRE(lhs < rhs);
      }
    }
    SECTION("result is not less or equal to failure") {
      SECTION("Returns false") {
        const auto rhs = fail("0");

        REQUIRE_FALSE(lhs < rhs);
      }
    }
  }
}

TEST_CASE("operator<(const failure<E>&, const result<T1,E1>&)", "[compare]") {
  SECTION("result contains value") {
    SECTION("Returns true") {
      const auto lhs = fail("hello world");
      const auto rhs = result<int,std::string>{0};

      REQUIRE_FALSE(lhs < rhs);
    }
  }
  SECTION("result contains error") {
    const auto rhs = result<int,std::string>{
      fail("5")
    };
    SECTION("result is less or equal to failure") {
      SECTION("Returns true") {
        const auto lhs = fail("0");

        REQUIRE(lhs < rhs);
      }
    }
    SECTION("result is not less or equal to failure") {
      SECTION("Returns false") {
        const auto lhs = fail("9");

        REQUIRE_FALSE(lhs < rhs);
      }
    }
  }
}

//-----------------------------------------------------------------------------
// Utilities
//-----------------------------------------------------------------------------

TEST_CASE("swap(result<T,E>&, result<T,E>&)", "[utility]") {
  SECTION("lhs and rhs contain a value") {
    auto lhs_sut = result<int, int>{42};
    auto rhs_sut = result<int, int>{100};
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
    auto lhs_sut = result<int, int>{fail(42)};
    auto rhs_sut = result<int, int>{fail(100)};
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
    auto lhs_sut = result<int, int>{42};
    auto rhs_sut = result<int, int>{fail(42)};
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
    auto lhs_sut = result<int, int>{fail(42)};
    auto rhs_sut = result<int, int>{};
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

TEST_CASE("swap(result<void,E>&, result<void,E>&)", "[utility]") {
  SECTION("lhs and rhs contain a value") {
    auto lhs_sut = result<void, int>{};
    auto rhs_sut = result<void, int>{};
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
    auto lhs_sut = result<void, int>{fail(42)};
    auto rhs_sut = result<void, int>{fail(100)};
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
    auto lhs_sut = result<void, int>{};
    auto rhs_sut = result<void, int>{fail(42)};
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
    auto lhs_sut = result<void, int>{fail(42)};
    auto rhs_sut = result<void, int>{};
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

TEST_CASE("std::hash<result<T,E>>::operator()", "[utility]") {
  SECTION("Value is active") {
    SECTION("Hashes value") {
      const auto sut = result<int,int>{42};

      const auto output = std::hash<result<int,int>>{}(sut);
      suppress_unused(output);

      SUCCEED();
    }
  }
  SECTION("Error is active") {
    SECTION("Hashes error") {
      const auto sut = result<int,int>{fail(42)};

      const auto output = std::hash<result<int,int>>{}(sut);
      suppress_unused(output);

      SUCCEED();
    }
  }

  SECTION("Hash of T and E are different for the same values for T and E") {
    const auto value_sut = result<int,int>{42};
    const auto error_sut = result<int,int>{fail(42)};

    const auto value_hash = std::hash<result<int,int>>{}(value_sut);
    const auto error_hash = std::hash<result<int,int>>{}(error_sut);

    REQUIRE(value_hash != error_hash);
  }
}

TEST_CASE("std::hash<result<void,E>>::operator()", "[utility]") {
  SECTION("Value is active") {
    SECTION("Produces '0' hash") {
      const auto sut = result<void,int>{};

      const auto output = std::hash<result<void,int>>{}(sut);

      REQUIRE(output == 0u);
    }
  }

  SECTION("Error is active") {
    SECTION("Hashes error") {
      const auto sut = result<void,int>{fail(42)};

      const auto output = std::hash<result<void,int>>{}(sut);
      suppress_unused(output);

      SUCCEED();
    }
  }
}

} // namespace test
} // namespace cpp

#if defined(_MSC_VER)
# pragma warning(pop)
#endif
