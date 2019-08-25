/*****************************************************************************
 * \file expected.hpp
 *
 *
 * \brief This header contains the 'expected' monadic type for indicating
 *        possible error conditions
 *****************************************************************************/

// This header originated from the bit-stl project.
// https://github.com/bitwizeshift/bit-stl/tree/develop/include/bit/stl/utilities/expected.hpp
//
// The file was created in 2017, and the original license is retained below.

/*
  The MIT License (MIT)

  Copyright (c) 2017 Matthew Rodusek

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
#ifndef EXPECT_EXPECT_HPP
#define EXPECT_EXPECT_HPP

#include <type_traits> // std::true_type
#include <variant>     // std::variant
#include <utility>     // std::in_place
#include <functional>  // std::reference_wrapper
#include <optional>    // std::optional
#include <initializer_list> // std::initializer_list
#include <system_error>     // std::error_code

# include <exception> // std::exception

namespace expect {

  //============================================================================
  // forward declarations
  //============================================================================

  template <typename E>
  class unexpected;
  template <typename T, typename E>
  class expected;

  //============================================================================
  // traits
  //============================================================================

  template <typename T>
  struct is_unexpected : std::false_type{};
  template <typename E>
  struct is_unexpected<unexpected<E>> : std::true_type{};

  template <typename T>
  struct is_expected : std::false_type{};
  template <typename T, typename E>
  struct is_expected<expected<T,E>> : std::true_type{};

  //============================================================================
  // class : unexpected<E>
  //============================================================================

  //////////////////////////////////////////////////////////////////////////////
  /// \brief A semantic type used for distinguishing unexpected values in an
  ///        API that returns expected types
  ///
  /// E must be non-throwing, and both copyable and moveable.
  ///
  /// \tparam E the error type
  //////////////////////////////////////////////////////////////////////////////
  template <typename E>
  class unexpected
  {
    static_assert(!is_expected<std::decay_t<E>>::value);
    static_assert(!is_unexpected<std::decay_t<E>>::value);
    static_assert(!std::is_reference<E>::value);
    static_assert(std::is_nothrow_copy_constructible<E>::value);
    static_assert(std::is_nothrow_move_constructible<E>::value);
    static_assert(std::is_nothrow_copy_assignable<E>::value);
    static_assert(std::is_nothrow_move_assignable<E>::value);

    //--------------------------------------------------------------------------
    // Public Member Types
    //--------------------------------------------------------------------------
  public:

    using error_type = E;

    //--------------------------------------------------------------------------
    // Constructors / Assignment
    //--------------------------------------------------------------------------
  public:

    unexpected() = delete;

    /// \brief Constructs an unexpected from the given error
    ///
    /// \param error the error to create an unexpected from
    constexpr explicit unexpected(E error) noexcept;

    /// \brief Constructs this unexpected by moving the contents of an existing
    ///        one
    ///
    /// \param other the other unexpected to move
    constexpr unexpected(unexpected&& other) noexcept = default;

    /// \brief Constructs this unexpected by copying the contents of an existing
    ///        one
    ///
    /// \param other the other unexpected to copy
    constexpr unexpected(const unexpected& other) noexcept = default;

    //--------------------------------------------------------------------------

    /// \brief Assigns the contents of \p other to this by move-assignment
    ///
    /// \param other the other unexpected to move
    /// \return reference to \c (*this)
    unexpected& operator=(unexpected&& other) noexcept = default;

    /// \brief Assigns the contents of \p other to this by copy-assignment
    ///
    /// \param other the other unexpected to copy
    /// \return reference to \c (*this)
    unexpected& operator=(const unexpected& other) noexcept = default;

    //--------------------------------------------------------------------------
    // Observers
    //--------------------------------------------------------------------------
  public:

    /// \{
    /// \brief Gets the underlying error
    ///
    /// \return the underlying error
    constexpr E& error() noexcept;
    constexpr const E& error() const noexcept;
    /// \}

    //--------------------------------------------------------------------------
    // Private Members
    //--------------------------------------------------------------------------
  private:

    E m_error;
  };

  //============================================================================
  // class : bad_expected_access
  //============================================================================

  //////////////////////////////////////////////////////////////////////////////
  /// \brief An exception thrown when an expected is accessed in an invalid
  ///        state
  //////////////////////////////////////////////////////////////////////////////
  class bad_expected_access : public std::exception
  {
    //--------------------------------------------------------------------------
    // Constructors / Assignment
    //--------------------------------------------------------------------------
  public:

    /// \brief Default constructs this bad_expected_access
    bad_expected_access() = default;

    /// \brief Constructs this bad_expected_access by moving the contents of
    ///        \p other
    ///
    /// \param other the other bad_expected_access to move
    bad_expected_access(bad_expected_access&& other) = default;

    /// \brief Constructs this bad_expected_access by copying the contents of
    ///        \p other
    ///
    /// \param other the other bad_expected_access to copy
    bad_expected_access(const bad_expected_access& other) = default;

    //--------------------------------------------------------------------------

    /// \brief Assigns the contents of \p other by move-assignment
    ///
    /// \param other the other exception to move
    /// \return reference to \c (*this)
    bad_expected_access& operator=(bad_expected_access&& other) = default;

    /// \brief Assigns the contents of \p other by copy-assignment
    ///
    /// \param other the other exception to copy
    /// \return reference to \c (*this)
    bad_expected_access& operator=(const bad_expected_access& other) = default;

    //--------------------------------------------------------------------------
    // Observers
    //--------------------------------------------------------------------------
  public:

    /// \brief Gets the error message
    ///
    /// \return the underlying exception message
    const char* what() const noexcept override;
  };

  //============================================================================
  // class : expected<T,E>
  //============================================================================

  //////////////////////////////////////////////////////////////////////////////
  /// \brief A semantic type for handling error conditions on an API.
  ///
  /// This type is intended to be orthogonal to exception handling by providing
  /// an alternative way of managing errors in a non-throwing manner. This type
  /// allows for APIs to explicitly inform the consumer of the fallability of
  /// a function, as well as the possible reason for the failure -- encoded in
  /// the E argument.
  ///
  /// This type is implicitly constructible by any type that can construct the
  /// underlying T type, or by an unexpected type that can implicitly construct
  /// the underlying E type.
  ///
  /// This allows for fluid APIs where the return type is expected<T,E>, but
  /// the code is simply authored as:
  ///
  /// \code
  /// if (succes) return foo;
  /// else return unexpected(foo_error::some_error_code);
  /// \endcode
  ///
  /// T may be any (possibly cv-qualified) value type, an lvalue-reference, or
  /// a (non-cv-qualified) void.
  ///
  /// E must be non-throwing, and both copyable and moveable.
  ///
  /// Neither T nor E can bea (possibly cv-qualified) expected or unexpected
  /// type.
  ///
  /// \tparam T The expected type
  /// \tparam E The unexpected error type
  //////////////////////////////////////////////////////////////////////////////
  template <typename T, typename E = std::error_code>
  class expected
  {
    static_assert(!std::is_array<T>::value);
    static_assert(!std::is_void<std::decay_t<T>>::value);
    static_assert(!std::is_rvalue_reference<T>::value);
    static_assert(!is_expected<std::decay_t<T>>::value);
    static_assert(!is_unexpected<std::decay_t<T>>::value);

    static_assert(!is_expected<E>::value);
    static_assert(!is_unexpected<E>::value);
    static_assert(!std::is_reference<E>::value);
    static_assert(std::is_nothrow_copy_constructible<E>::value);
    static_assert(std::is_nothrow_move_constructible<E>::value);
    static_assert(std::is_nothrow_copy_assignable<E>::value);
    static_assert(std::is_nothrow_move_assignable<E>::value);

    //--------------------------------------------------------------------------
    // Public Member Type
    //--------------------------------------------------------------------------
  public:

    using expected_type = T;
    using error_type = E;

    //--------------------------------------------------------------------------
    // Constructors / Assignment
    //--------------------------------------------------------------------------
  public:

    /// \brief Constructs the underlying value by default-construction by
    ///        calling T's constructor
    constexpr expected() = default;

    /// \brief Constructs the underlying value of this expected by calling
    ///        T's constructor with the given args
    ///
    /// \param args... the arguments to forward to T's constructor
    template <typename...Args>
    constexpr expected(std::in_place_t, Args&&...args)
      noexcept(std::is_nothrow_constructible<T,Args...>::value);

    /// \brief Constructs the underlying value of this expected by calling
    ///        T's constructor with the given args
    ///
    /// \param args... the arguments to forward to T's constructor
    /// \param ilist the initializer list
    template <typename...Args, typename U>
    constexpr expected(std::in_place_t,
                       std::initializer_list<U> ilist,
                       Args&&...args)
      noexcept(std::is_nothrow_constructible<T,std::initializer_list<U>,Args...>::value);

    /// \brief Constructs the underlying value of this expected by forwarding
    ///        the value of \p value to T's constructor
    ///
    /// \param value the value to use for construction
    template <typename U,
              typename = std::enable_if_t<std::is_constructible<T,U>::value>>
    constexpr /* implicit */ expected(U&& value)
      noexcept(std::is_nothrow_constructible<T,U>::value);

    /// \brief Constructs the underlying error of this expected
    ///
    /// \param e the unexpected error
    template <typename E2,
              typename = std::enable_if_t<std::is_nothrow_constructible<E,E2>::value>>
    constexpr /* implicit */ expected(unexpected<E2> e) noexcept;

    /// \brief Constructs this expected by moving the contents of \p other
    ///
    /// \param other the other expected to move
    constexpr expected(expected&& other) = default;

    /// \brief Constructs this expected by copying the contents of \p other
    ///
    /// \param other the other expected to copy
    constexpr expected(const expected& other) = default;

    //--------------------------------------------------------------------------

    /// \brief Assigns the contents of \p other to this through move-assignment
    ///
    /// \param other the other expected to move
    /// \return reference to \c (*this)
    expected& operator=(expected&& other) = default;

    /// \brief Assigns the contents of \p other to this through copy-assignment
    ///
    /// \param other the other expected to copy
    /// \return reference to \c (*this)
    expected& operator=(const expected& other) = default;

    //--------------------------------------------------------------------------
    // Observers
    //--------------------------------------------------------------------------
  public:

    /// \{
    /// \brief Gets the underlying value from this expected
    ///
    /// \return reference to the underlying value
    constexpr T& value();
    constexpr const T& value() const;
    /// \}

    /// \{
    /// \brief Gets the underlying error from this expected
    ///
    /// \return reference to the underlying error
    constexpr E& error();
    constexpr const E& error() const;
    /// \}

    //--------------------------------------------------------------------------
    // Modifiers
    //--------------------------------------------------------------------------
  public:

    /// \brief Swaps the contents of this with \p other
    ///
    /// \param other the other expected to swap contents with
    void swap(expected<T,E>& other) noexcept;

    //--------------------------------------------------------------------------
    // Private Members
    //--------------------------------------------------------------------------
  private:

    using underlying_type = std::conditional_t<
      std::is_lvalue_reference<T>::value,
      std::reference_wrapper<T>,
      T
    >;

    template <typename U>
    static U& reference_to(U& ref);
    template <typename U>
    static U& reference_to(std::reference_wrapper<U> ref);

    std::variant<underlying_type,E> m_state;
  };

  //============================================================================
  // class : expected<void,E>
  //============================================================================

  template <typename E>
  class expected<void,E>
  {
    static_assert(!is_expected<E>::value);
    static_assert(!is_unexpected<E>::value);
    static_assert(!std::is_reference<E>::value);
    static_assert(std::is_nothrow_copy_constructible<E>::value);
    static_assert(std::is_nothrow_move_constructible<E>::value);
    static_assert(std::is_nothrow_copy_assignable<E>::value);
    static_assert(std::is_nothrow_move_assignable<E>::value);

    //--------------------------------------------------------------------------
    // Public Member Type
    //--------------------------------------------------------------------------
  public:

    using expected_type = void;
    using error_type = E;

    //--------------------------------------------------------------------------
    // Constructors / Assignment
    //--------------------------------------------------------------------------
  public:

    /// \brief Constructs this expected successfully
    constexpr expected() noexcept = default;

    /// \brief Constructs the underlying error of this expected
    ///
    /// \param e the unexpected error
    template <typename E2,
              typename = std::enable_if_t<std::is_nothrow_constructible<E,E2>::value>>
    constexpr /* implicit */ expected(unexpected<E2> e) noexcept;

    /// \brief Constructs this expected by moving the contents of \p other
    ///
    /// \param other the other expected to move
    constexpr expected(expected&& other) = default;

    /// \brief Constructs this expected by copying the contents of \p other
    ///
    /// \param other the other expected to copy
    constexpr expected(const expected& other) = default;

    //--------------------------------------------------------------------------

    /// \brief Assigns the contents of \p other to this through move-assignment
    ///
    /// \param other the other expected to move
    /// \return reference to \c (*this)
    expected& operator=(expected&& other) = default;

    /// \brief Assigns the contents of \p other to this through copy-assignment
    ///
    /// \param other the other expected to copy
    /// \return reference to \c (*this)
    expected& operator=(const expected& other) = default;

    //--------------------------------------------------------------------------
    // Observers
    //--------------------------------------------------------------------------
  public:

    /// \brief Attempts to get the underlying value
    ///
    /// This function is here for symmetry with the non-void specialization
    void value() const;

    /// \{
    /// \brief Gets the underlying error from this expected
    ///
    /// \return reference to the underlying error
    constexpr E& error();
    constexpr const E& error() const;
    /// \}

    //--------------------------------------------------------------------------
    // Modifiers
    //--------------------------------------------------------------------------
  public:

    /// \brief Swaps the contents of this with \p other
    ///
    /// \param other the other expected to swap contents with
    void swap(expected<void,E>& other) noexcept;

    //--------------------------------------------------------------------------
    // Private Members
    //--------------------------------------------------------------------------
  private:

    std::optional<E> m_state;
  };

} // namespace expect

//==============================================================================
// inline definitions : class : unexpected<E>
//==============================================================================

//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

template <typename E>
inline constexpr expect::unexpected<E>::unexpected(E error)
  noexcept
  : m_error{std::move(error)}
{

}

//------------------------------------------------------------------------------
// Observers
//------------------------------------------------------------------------------

template <typename E>
inline constexpr E& expect::unexpected<E>::error()
  noexcept
{
  return m_error;
}

template <typename E>
inline constexpr const E& expect::unexpected<E>::error()
  const noexcept
{
  return m_error;
}

//==============================================================================
// inline definitions : class : bad_expected_access
//==============================================================================

//------------------------------------------------------------------------------
// Observers
//------------------------------------------------------------------------------

inline const char* expect::bad_expected_access::what()
  const noexcept
{
  return "bad_expected_access";
}

//==============================================================================
// inline definitions : class : expected<T,E>
//==============================================================================

//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

template <typename T, typename E>
template <typename...Args>
inline constexpr expect::expected<T,E>
  ::expected(std::in_place_t, Args&&...args)
  noexcept(std::is_nothrow_constructible<T,Args...>::value)
  : m_state{std::in_place_type<underlying_type>, std::forward<Args>(args)...}
{

}

template <typename T, typename E>
template <typename...Args, typename U>
inline constexpr expect::expected<T,E>
  ::expected(std::in_place_t, std::initializer_list<U> ilist, Args&&...args)
  noexcept(std::is_nothrow_constructible<T,std::initializer_list<U>, Args...>::value)
  : m_state{
    std::in_place_type<underlying_type>,
    ilist,
    std::forward<Args>(args)...
  }
{

}

template <typename T, typename E>
template <typename U, typename>
inline constexpr expect::expected<T,E>::expected(U&& value)
  noexcept(std::is_nothrow_constructible<T,U>::value)
  : m_state{std::in_place_type<underlying_type>, std::forward<U>(value)}
{

}

template <typename T, typename E>
template <typename E2, typename>
inline constexpr expect::expected<T,E>::expected(unexpected<E2> e)
  noexcept
  : m_state{std::in_place_type<E>, std::move(e.error())}
{

}

//------------------------------------------------------------------------------
// Observers
//------------------------------------------------------------------------------

template <typename T, typename E>
inline constexpr T& expect::expected<T,E>::value()
{
  if (!std::holds_alternative<underlying_type>(m_state)) {
    throw bad_expected_access{};
  }

  return reference_to(std::get<underlying_type>(m_state));
}

template <typename T, typename E>
inline constexpr const T& expect::expected<T,E>::value()
  const
{
  if (!std::holds_alternative<underlying_type>(m_state)) {
    throw bad_expected_access{};
  }

  return reference_to(std::get<underlying_type>(m_state));
}

template <typename T, typename E>
inline constexpr E& expect::expected<T,E>::error()
{
  if (!std::holds_alternative<E>(m_state)) {
    throw bad_expected_access{};
  }

  return std::get<E>(m_state);
}

template <typename T, typename E>
inline constexpr const E& expect::expected<T,E>::error()
  const
{
  if (!std::holds_alternative<E>(m_state)) {
    throw bad_expected_access{};
  }

  return std::get<E>(m_state);
}

//------------------------------------------------------------------------------
// Modifiers
//------------------------------------------------------------------------------

/// \brief Swaps the contents of this with \p other
///
/// \param other the other expected to swap contents with
template <typename T, typename E>
inline void expect::expected<T,E>::swap(expected<T,E>& other)
  noexcept
{
  using std::swap;

  swap(m_state, other.m_state);
}

//------------------------------------------------------------------------------
// Private Static Functions
//------------------------------------------------------------------------------

template <typename T, typename E>
template <typename U>
inline U& expect::expected<T,E>::reference_to(U& ref)
{
  return ref;
}

template <typename T, typename E>
template <typename U>
inline U& expect::expected<T,E>::reference_to(std::reference_wrapper<U> ref)
{
  return ref.get();
}

//==============================================================================
// inline definitions : class : expected<void,E>
//==============================================================================

//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

template <typename E>
template <typename E2, typename>
inline constexpr expect::expected<void,E>::expected(unexpected<E2> e)
  noexcept
  : m_state{std::move(e.error())}
{

}

//------------------------------------------------------------------------------
// Observers
//------------------------------------------------------------------------------

template <typename E>
inline void expect::expected<void,E>::value()
  const
{
  if (m_state.has_value()) {
    throw bad_expected_access{};
  }
}

template <typename E>
inline constexpr E& expect::expected<void,E>::error()
{
  if (!m_state.has_value()) {
    throw bad_expected_access{};
  }
  return (*m_state);
}

template <typename E>
inline constexpr const E& expect::expected<void,E>::error()
  const
{
  if (!m_state.has_value()) {
    throw bad_expected_access{};
  }
  return (*m_state);
}

//------------------------------------------------------------------------------
// Modifiers
//------------------------------------------------------------------------------

template <typename E>
void expect::expected<void,E>::swap(expected<void,E>& other)
  noexcept
{
  using std::swap;

  swap(m_state,other.m_state);
}

#endif /* EXPECT_EXPECT_HPP */
