////////////////////////////////////////////////////////////////////////////////
/// \file expected.hpp
///
/// \brief This header contains the 'expected' monadic type for indicating
///        possible error conditions
////////////////////////////////////////////////////////////////////////////////

/*
  The MIT License (MIT)

  Copyright (c) 2017 Matthew Rodusek All rights reserved.

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

#ifndef MSL_EXPECTED_HPP
#define MSL_EXPECTED_HPP

#if __cplusplus >= 201402L
# define EXPECTED_CPP14_CONSTEXPR constexpr
#else
# define EXPECTED_CPP14_CONSTEXPR
#endif

#if __cplusplus >= 201703L
# define EXPECTED_CPP17_INLINE inline
#else
# define EXPECTED_CPP17_INLINE
#endif


#if defined(__clang__) && defined(_MSC_VER)
# define EXPECTED_INLINE_VISIBILITY __attribute__((visibility("hidden"), no_instrument_function))
#elif defined(__clang__) || defined(__GNUC__)
# define EXPECTED_INLINE_VISIBILITY __attribute__((visibility("hidden"), always_inline, no_instrument_function))
#elif defined(_MSC_VER)
# define EXPECTED_INLINE_VISIBILITY __forceinline
#else
# define EXPECTED_INLINE_VISIBILITY
#endif

#include <cstddef>      // std::size_t
#include <stdexcept>    // std::logic_error
#include <type_traits>  // std::enable_if, std::is_constructible, etc
#include <new>          // placement-new
#include <memory>       // std::address_of
#include <functional>   // std::reference_wrapper, std::invoke
#include <system_error> // std::error_code
#include <utility>      // std::in_place_t, std::forward
#include <initializer_list> // std::initializer_list

namespace expect {

  //===========================================================================
  // utilities : constexpr forward
  //===========================================================================

  // std::forward is not constexpr until C++14
  namespace detail {
#if __cplusplus >= 201402L
    using std::forward;
#else
    template <typename T>
    inline EXPECTED_INLINE_VISIBILITY constexpr
    auto forward(typename std::remove_reference<T>::type& t)
      noexcept -> T&&
    {
      return static_cast<T&&>(t);
    }

    template <typename T>
    inline EXPECTED_INLINE_VISIBILITY constexpr
    auto forward(typename std::remove_reference<T>::type&& t)
      noexcept -> T&&
    {
      return static_cast<T&&>(t);
    }
#endif
  } // namespace detail

  //===========================================================================
  // utilities : invoke / invoke_result
  //===========================================================================

  // std::invoke was introduced in C++17

  namespace detail {
#if __cplusplus >= 201703L
    using std::invoke;
    using std::invoke_result;
    using std::invoke_result_t;
#else
    template<typename T>
    struct is_reference_wrapper : std::false_type{};

    template<typename U>
    struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type{};

    //-------------------------------------------------------------------------

    template <typename Base, typename T, typename Derived, typename... Args,
              typename = typename std::enable_if<
                std::is_function<T>::value &&
                std::is_base_of<Base, typename std::decay<Derived>::type>::value
              >::type>
    inline EXPECTED_INLINE_VISIBILITY constexpr
    auto invoke(T Base::*pmf, Derived&& ref, Args&&... args)
      noexcept(noexcept((::expect::detail::forward<Derived>(ref).*pmf)(::expect::detail::forward<Args>(args)...)))
      -> decltype((::expect::detail::forward<Derived>(ref).*pmf)(::expect::detail::forward<Args>(args)...))
    {
      return (expect::detail::forward<Derived>(ref).*pmf)(expect::detail::forward<Args>(args)...);
    }

    template <typename Base, typename T, typename RefWrap, typename... Args,
              typename = typename std::enable_if<
                std::is_function<T>::value &&
                is_reference_wrapper<typename std::decay<RefWrap>::type>::value
              >::type>
    inline EXPECTED_INLINE_VISIBILITY constexpr
    auto invoke(T Base::*pmf, RefWrap&& ref, Args&&... args)
      noexcept(noexcept((ref.get().*pmf)(std::forward<Args>(args)...)))
      -> decltype((ref.get().*pmf)(expect::detail::forward<Args>(args)...))
    {
      return (ref.get().*pmf)(expect::detail::forward<Args>(args)...);
    }

    template <typename Base, typename T, typename Pointer, typename... Args,
              typename = typename std::enable_if<
                std::is_function<T>::value &&
                !is_reference_wrapper<typename std::decay<Pointer>::type>::value &&
                !std::is_base_of<Base, typename std::decay<Pointer>::type>::value
              >::type>
    inline EXPECTED_INLINE_VISIBILITY constexpr
    auto invoke(T Base::*pmf, Pointer&& ptr, Args&&... args)
      noexcept(noexcept(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...)))
      -> decltype(((*expect::detail::forward<Pointer>(ptr)).*pmf)(expect::detail::forward<Args>(args)...))
    {
      return ((*expect::detail::forward<Pointer>(ptr)).*pmf)(expect::detail::forward<Args>(args)...);
    }

    template <typename Base, typename T, typename Derived,
              typename = typename std::enable_if<
                !std::is_function<T>::value &&
                std::is_base_of<Base, typename std::decay<Derived>::type>::value
              >::type>
    inline EXPECTED_INLINE_VISIBILITY constexpr
    auto invoke(T Base::*pmd, Derived&& ref)
      noexcept(noexcept(std::forward<Derived>(ref).*pmd))
      -> decltype(expect::detail::forward<Derived>(ref).*pmd)
    {
      return expect::detail::forward<Derived>(ref).*pmd;
    }

    template <typename Base, typename T, typename RefWrap,
              typename = typename std::enable_if<
                !std::is_function<T>::value &&
                is_reference_wrapper<typename std::decay<RefWrap>::type>::value
              >::type>
    inline EXPECTED_INLINE_VISIBILITY constexpr
    auto invoke(T Base::*pmd, RefWrap&& ref)
      noexcept(noexcept(ref.get().*pmd))
      -> decltype(ref.get().*pmd)
    {
      return ref.get().*pmd;
    }

    template <typename Base, typename T, typename Pointer,
              typename = typename std::enable_if<
                !std::is_function<T>::value &&
                !is_reference_wrapper<typename std::decay<Pointer>::type>::value &&
                !std::is_base_of<Base, typename std::decay<Pointer>::type>::value
              >::type>
    inline EXPECTED_INLINE_VISIBILITY constexpr
    auto invoke(T Base::*pmd, Pointer&& ptr)
      noexcept(noexcept((*std::forward<Pointer>(ptr)).*pmd))
      -> decltype((*expect::detail::forward<Pointer>(ptr)).*pmd)
    {
      return (*expect::detail::forward<Pointer>(ptr)).*pmd;
    }

    template <typename F, typename... Args,
              typename = typename std::enable_if<!std::is_member_pointer<typename std::decay<F>::type>::value>::type>
    inline EXPECTED_INLINE_VISIBILITY constexpr
    auto invoke(F&& f, Args&&... args)
        noexcept(noexcept(std::forward<F>(f)(std::forward<Args>(args)...)))
      -> decltype(expect::detail::forward<F>(f)(expect::detail::forward<Args>(args)...))
    {
      return expect::detail::forward<F>(f)(expect::detail::forward<Args>(args)...);
    }

    template <typename Fn, typename...Args>
    struct invoke_result {
      using type = decltype(expect::detail::invoke(std::declval<Fn>(), std::declval<Args>()...));
    };
    template <typename Fn, typename...Args>
    using invoke_result_t = typename invoke_result<Fn, Args...>::type;
#endif
  }

  //===========================================================================
  // struct : in_place_t
  //===========================================================================

#if __cplusplus >= 201703L
  using std::in_place_t;
  using std::in_place;
#else
  /// \brief A structure for representing in-place construction
  struct in_place_t
  {
    explicit in_place_t() = default;
  };
  EXPECTED_CPP17_INLINE constexpr auto in_place = in_place_t{};
#endif

  //===========================================================================
  // struct : in_place_t
  //===========================================================================

  /// \brief A structure for representing in-place construction of an error type
  struct in_place_error_t
  {
    explicit in_place_error_t() = default;
  };

  EXPECTED_CPP17_INLINE constexpr auto in_place_error = in_place_error_t{};

  //===========================================================================
  // forward-declarations
  //===========================================================================

  template <typename>
  class unexpected;

  template <typename, typename>
  class expected;

  class bad_expected_access;

  //===========================================================================
  // traits
  //===========================================================================

  template <typename T>
  struct is_unexpected : std::false_type{};
  template <typename E>
  struct is_unexpected<unexpected<E>> : std::true_type{};

  template <typename T>
  struct is_expected : std::false_type{};
  template <typename T, typename E>
  struct is_expected<expected<T,E>> : std::true_type{};

  //===========================================================================
  // trait : detail::wrapped_expected_type
  //===========================================================================

  namespace detail {

    template <typename T>
    using wrapped_expected_type = typename std::conditional<
      std::is_lvalue_reference<T>::value,
      std::reference_wrapper<
        typename std::remove_reference<T>::type
      >,
      typename std::remove_const<T>::type
    >::type;

  } // namespace detail

#if !defined(EXPECTED_DISABLE_EXCEPTIONS)

  //===========================================================================
  // class : bad_expected_access
  //===========================================================================

  /////////////////////////////////////////////////////////////////////////////
  /// \brief An exception thrown when expected::value is accessed without
  ///        a contained value
  /////////////////////////////////////////////////////////////////////////////
  class bad_expected_access : public std::exception
  {
    //-------------------------------------------------------------------------
    // Constructor / Assignment
    //-------------------------------------------------------------------------
  public:

    bad_expected_access() = default;
    bad_expected_access(const bad_expected_access& other) = default;
    bad_expected_access(bad_expected_access&& other) = default;

    //-------------------------------------------------------------------------

    auto operator=(const bad_expected_access& other) -> bad_expected_access& = default;
    auto operator=(bad_expected_access&& other) -> bad_expected_access& = default;

    //-------------------------------------------------------------------------
    // Observers
    //-------------------------------------------------------------------------
  public:

    auto what() const noexcept -> const char* override;
  };

#endif

  //===========================================================================
  // class : unexpected_type
  //===========================================================================

  //////////////////////////////////////////////////////////////////////////////
  /// \brief A semantic type used for distinguishing unexpected values in an
  ///        API that returns expected types
  ///
  /// \tparam E the error type
  //////////////////////////////////////////////////////////////////////////////
  template <typename E>
  class unexpected
  {
    static_assert(
      !is_expected<typename std::decay<E>::type>::value,
      "A (possibly CV-qualified) expected 'E' type is ill-formed."
    );
    static_assert(
      !is_unexpected<typename std::decay<E>::type>::value,
      "A (possibly CV-qualified) unexpected 'E' type is ill-formed."
    );
    static_assert(
      !std::is_void<typename std::decay<E>::type>::value,
      "A (possibly CV-qualified) 'void' 'E' type is ill-formed."
    );
    static_assert(
      !std::is_rvalue_reference<E>::value,
      "rvalue references for 'E' type is ill-formed. "
      "Only lvalue references are valid."
    );

    //-------------------------------------------------------------------------
    // Public Member Types
    //-------------------------------------------------------------------------
  public:

    using error_type = typename std::remove_reference<E>::type;

    //-------------------------------------------------------------------------
    // Constructors / Assignment
    //-------------------------------------------------------------------------
  public:

    /// \brief Constructs an unexpected via default construction
    constexpr unexpected() = default;

    /// \brief Constructs an unexpected by delegating construction to the
    ///        underlying constructor
    ///
    /// \param args the arguments to forward to E's constructor
    template <typename...Args,
              typename = typename std::enable_if<std::is_constructible<E,Args...>::value>::type>
    constexpr unexpected(in_place_t, Args&&...args)
      noexcept(std::is_nothrow_constructible<E, Args...>::value);

    /// \{
    /// \brief Constructs an unexpected from the given error
    ///
    /// \param error the error to create an unexpected from
    template <typename E2=E,
              typename std::enable_if<!std::is_lvalue_reference<E2>::value && std::is_constructible<E,const E2&>::value,int>::type = 0>
    constexpr explicit unexpected(const error_type& error)
      noexcept(std::is_nothrow_copy_constructible<E>::value);
    template <typename E2=E,
              typename std::enable_if<!std::is_lvalue_reference<E2>::value && std::is_constructible<E,E2&&>::value,int>::type = 0>
    constexpr explicit unexpected(error_type&& error)
      noexcept(std::is_nothrow_move_constructible<E>::value);
    /// \}

    /// \brief Constructs an unexpected from a reference to the given error
    ///
    /// This overload is only enabled
    ///
    /// \param error the reference to the error
    template <typename E2=E,
              typename std::enable_if<std::is_lvalue_reference<E2>::value,int>::type = 0>
    constexpr explicit unexpected(error_type& error) noexcept;

    /// \brief Constructs this unexpected by copying the contents of an existing
    ///        one
    ///
    /// \param other the other unexpected to copy
    constexpr unexpected(const unexpected& other) = default;

    /// \brief Constructs this unexpected by moving the contents of an existing
    ///        one
    ///
    /// \param other the other unexpected to move
    constexpr /* implicit */ unexpected(unexpected&& other) = default;

    /// \brief Constructs this unexpected by copy-converting \p other
    ///
    /// \param other the other unexpected to copy
    template <typename E2,
              typename = typename std::enable_if<std::is_constructible<E,const E2&>::value>::type>
    constexpr /* implicit */ unexpected(const unexpected<E2>& other)
      noexcept(std::is_nothrow_constructible<E,const E2&>::value);

    /// \brief Constructs this unexpected by move-converting \p other
    ///
    /// \param other the other unexpected to copy
    template <typename E2,
              typename = typename std::enable_if<std::is_constructible<E,E2&&>::value>::type>
    constexpr unexpected(unexpected<E2>&& other)
      noexcept(std::is_nothrow_constructible<E,E2&&>::value);

    //--------------------------------------------------------------------------

    /// \brief Assigns the value of \p error to this unexpected through
    ///        copy-assignment
    ///
    /// \param error the value to assign
    /// \return reference to `(*this)`
    template <typename E2=E,
              typename std::enable_if<!std::is_lvalue_reference<E2>::value && std::is_assignable<E,const E2&>::value,int>::type = 0>
    EXPECTED_CPP14_CONSTEXPR
    auto operator=(const error_type& error)
      noexcept(std::is_nothrow_copy_assignable<E>::value) -> unexpected&;

    /// \brief Assigns the value of \p error to this unexpected through
    ///        move-assignment
    ///
    /// \param error the value to assign
    /// \return reference to `(*this)`
    template <typename E2=E,
              typename std::enable_if<!std::is_lvalue_reference<E2>::value && std::is_assignable<E,E2&&>::value,int>::type = 0>
    EXPECTED_CPP14_CONSTEXPR
    auto operator=(error_type&& error)
      noexcept(std::is_nothrow_move_assignable<E>::value) -> unexpected&;

    /// \brief Assigns the value of \p error to this unexpected reference
    ///
    /// \param error the reference to assign
    /// \return reference to `(*this)`
    template <typename E2=E,
              typename std::enable_if<std::is_lvalue_reference<E2>::value,int>::type = 0>
    EXPECTED_CPP14_CONSTEXPR
    auto operator=(error_type& error) noexcept -> unexpected&;


    /// \brief Assigns the contents of \p other to this by copy-assignment
    ///
    /// \param other the other unexpected to copy
    /// \return reference to `(*this)`
    EXPECTED_CPP14_CONSTEXPR
    auto operator=(const unexpected& other) -> unexpected& = default;

    /// \brief Assigns the contents of \p other to this by move-assignment
    ///
    /// \param other the other unexpected to move
    /// \return reference to `(*this)`
    EXPECTED_CPP14_CONSTEXPR
    auto operator=(unexpected&& other) -> unexpected& = default;

    /// \brief Assigns the contents of \p other to this by copy conversion
    ///
    /// \param other the other unexpected to copy-convert
    /// \return reference to `(*this)`
    template <typename E2,
              typename = typename std::enable_if<std::is_assignable<E,const E2&>::value>::type>
    EXPECTED_CPP14_CONSTEXPR
    auto operator=(const unexpected<E2>& other)
      noexcept(std::is_nothrow_assignable<E,const E2&>::value) -> unexpected&;

    /// \brief Assigns the contents of \p other to this by move conversion
    ///
    /// \param other the other unexpected to move-convert
    /// \return reference to `(*this)`
    template <typename E2,
              typename = typename std::enable_if<std::is_assignable<E,E2&&>::value>::type>
    EXPECTED_CPP14_CONSTEXPR
    auto operator=(unexpected<E2>&& other)
      noexcept(std::is_nothrow_assignable<E,E2&&>::value) -> unexpected&;

    //--------------------------------------------------------------------------
    // Observers
    //--------------------------------------------------------------------------
  public:

    /// \{
    /// \brief Gets the underlying error
    ///
    /// \return the underlying error
    EXPECTED_CPP14_CONSTEXPR
    auto error() & noexcept -> error_type&;
    EXPECTED_CPP14_CONSTEXPR
    auto error() && noexcept -> error_type&&;
    constexpr auto error() const & noexcept -> const error_type&;
    constexpr auto error() const && noexcept -> const error_type&&;
    /// \}

    //-------------------------------------------------------------------------
    // Private Member Types
    //-------------------------------------------------------------------------
  private:

    using underlying_type = detail::wrapped_expected_type<E>;

    //-------------------------------------------------------------------------
    // Private Members
    //-------------------------------------------------------------------------
  private:

    underlying_type m_unexpected;
  };

#if __cplusplus >= 201703L
  template <typename T>
  unexpected(std::reference_wrapper<T>) -> unexpected<T>;

  template <typename T>
  unexpected(T&&) -> unexpected<typename std::decay<T>::type>;
#endif

  //===========================================================================
  // non-member functions : class : unexpected
  //===========================================================================

  //---------------------------------------------------------------------------
  // Comparison
  //---------------------------------------------------------------------------

  template <typename E1, typename E2>
  constexpr auto operator==(const unexpected<E1>& lhs,
                            const unexpected<E2>& rhs) noexcept -> bool;
  template <typename E1, typename E2>
  constexpr auto operator!=(const unexpected<E1>& lhs,
                            const unexpected<E2>& rhs) noexcept -> bool;
  template <typename E1, typename E2>
  constexpr auto operator<(const unexpected<E1>& lhs,
                           const unexpected<E2>& rhs) noexcept -> bool;
  template <typename E1, typename E2>
  constexpr auto operator>(const unexpected<E1>& lhs,
                           const unexpected<E2>& rhs) noexcept -> bool;
  template <typename E1, typename E2>
  constexpr auto operator<=(const unexpected<E1>& lhs,
                            const unexpected<E2>& rhs) noexcept -> bool;
  template <typename E1, typename E2>
  constexpr auto operator>=(const unexpected<E1>& lhs,
                            const unexpected<E2>& rhs) noexcept -> bool;

  //---------------------------------------------------------------------------
  // Utilities
  //---------------------------------------------------------------------------

  /// \brief Deduces and constructs an unexpected type from \p e
  ///
  /// \param e the unexpected value
  /// \return a constructed unexpected value
  template <typename E>
  constexpr auto make_unexpected(E&& e)
    noexcept(std::is_nothrow_constructible<typename std::decay<E>::type,E>::value)
    -> unexpected<typename std::decay<E>::type>;

  /// \brief Deduces an unexpected reference from a reverence_wrapper
  ///
  /// \param e the unexpected value
  /// \return a constructed unexpected reference
  template <typename E>
  constexpr auto make_unexpected(std::reference_wrapper<E> e)
    noexcept -> unexpected<E&>;

  /// \brief Constructs an unexpected type from a series of arguments
  ///
  /// \tparam E the unexpected type
  /// \param args the arguments to forward to E's constructor
  /// \return a constructed unexpected type
  template <typename E, typename...Args,
            typename = typename std::enable_if<std::is_constructible<E,Args...>::value>::type>
  constexpr auto make_unexpected(Args&&...args)
    noexcept(std::is_nothrow_constructible<E, Args...>::value)
    -> unexpected<E>;

  /// \brief Swaps the contents of two unexpected values
  ///
  /// \param lhs the left unexpected
  /// \param rhs the right unexpected
  template <typename E>
  auto swap(unexpected<E>& lhs, unexpected<E>& rhs)
#if __cplusplus >= 201703L
    noexcept(std::is_nothrow_swappable<E>::value) -> void;
#else
    noexcept -> void;
#endif

  namespace detail {

    //=========================================================================
    // class : unit
    //=========================================================================

    /// \brief A standalone monostate object (effectively std::monostate). This
    ///        exists to allow for `void` specializations
    struct unit {};

    //=========================================================================
    // non-member functions : class : unit
    //=========================================================================

    constexpr bool operator==(unit, unit) noexcept { return true; }
    constexpr bool operator!=(unit, unit) noexcept { return false; }
    constexpr bool operator<(unit, unit) noexcept { return false; }
    constexpr bool operator>(unit, unit) noexcept { return false; }
    constexpr bool operator<=(unit, unit) noexcept { return true; }
    constexpr bool operator>=(unit, unit) noexcept { return true; }

    //=========================================================================
    // class : detail::expected_destruct_base<T, E, IsTrivial>
    //=========================================================================

    ///////////////////////////////////////////////////////////////////////////
    /// \brief A base class template for expected guarantees trivial types have
    ///        a trivial destructor
    ///
    /// \tparam T the value type expected to be returned
    /// \tparam E the error type returned on failure
    /// \tparam IsTrivial
    ///////////////////////////////////////////////////////////////////////////
    template <typename T, typename E,
              bool IsTrivial = std::is_trivially_destructible<T>::value &&
                               std::is_trivially_destructible<E>::value>
    struct expected_destruct_base
    {
      //-----------------------------------------------------------------------
      // Public Member Types
      //-----------------------------------------------------------------------

      using underlying_value_type = wrapped_expected_type<T>;
      using underlying_error_type = E;

      //-----------------------------------------------------------------------
      // Constructors / Assignment
      //-----------------------------------------------------------------------

      /// \brief Constructs an empty object
      ///
      /// This is for use with conversion constructors, since it allows a
      /// temporary unused object to be set
      expected_destruct_base(unit) noexcept;

      /// \brief Constructs the underlying value from the specified \p args
      ///
      /// \param args the arguments to forward to T's constructor
      template <typename...Args>
      constexpr expected_destruct_base(in_place_t, Args&&...args)
        noexcept(std::is_nothrow_constructible<T, Args...>::value);

      /// \brief Constructs the underlying error from the specified \p args
      ///
      /// \param args the arguments to forward to E's constructor
      template <typename...Args>
      constexpr expected_destruct_base(in_place_error_t, Args&&...args)
        noexcept(std::is_nothrow_constructible<E, Args...>::value);

      expected_destruct_base(const expected_destruct_base&) = default;
      expected_destruct_base(expected_destruct_base&&) = default;

      //-----------------------------------------------------------------------

      auto operator=(const expected_destruct_base&) -> expected_destruct_base& = default;
      auto operator=(expected_destruct_base&&) -> expected_destruct_base& = default;

      //-----------------------------------------------------------------------
      // Modifiers
      //-----------------------------------------------------------------------

      /// \brief A no-op for trivial types
      auto destroy() const noexcept -> void;

      //-----------------------------------------------------------------------
      // Public Members
      //-----------------------------------------------------------------------

      union {
        underlying_value_type m_value;
        underlying_error_type m_error;
        unit m_empty;
      };
      bool m_has_value;
    };

    //-------------------------------------------------------------------------

    template <typename T, typename E>
    struct expected_destruct_base<T, E, false>
    {
      //-----------------------------------------------------------------------
      // Public Member Types
      //-----------------------------------------------------------------------

      using underlying_value_type = wrapped_expected_type<T>;
      using underlying_error_type = E;

      //-----------------------------------------------------------------------
      // Constructors / Assignment / Destructor
      //-----------------------------------------------------------------------

      expected_destruct_base(unit) noexcept;

      /// \brief Constructs the underlying value from the specified \p args
      ///
      /// \param args the arguments to forward to T's constructor
      template <typename...Args>
      constexpr expected_destruct_base(in_place_t, Args&&...args)
        noexcept(std::is_nothrow_constructible<T, Args...>::value);

      /// \brief Constructs the underlying error from the specified \p args
      ///
      /// \param args the arguments to forward to E's constructor
      template <typename...Args>
      constexpr expected_destruct_base(in_place_error_t, Args&&...args)
        noexcept(std::is_nothrow_constructible<E, Args...>::value);

      expected_destruct_base(const expected_destruct_base&) = default;
      expected_destruct_base(expected_destruct_base&&) = default;

      //-----------------------------------------------------------------------

      /// \brief Destroys the underlying stored object
      ~expected_destruct_base()
        noexcept(std::is_nothrow_destructible<T>::value &&
                 std::is_nothrow_destructible<E>::value);

      //-----------------------------------------------------------------------

      auto operator=(const expected_destruct_base&) -> expected_destruct_base& = default;
      auto operator=(expected_destruct_base&&) -> expected_destruct_base& = default;

      //-----------------------------------------------------------------------
      // Modifiers
      //-----------------------------------------------------------------------

      /// \brief Destroys the underlying stored object
      auto destroy() -> void;

      //-----------------------------------------------------------------------
      // Public Members
      //-----------------------------------------------------------------------

      union {
        underlying_value_type m_value;
        underlying_error_type m_error;
        unit m_empty;
      };
      bool m_has_value;
    };

    //=========================================================================
    // class : expected_construct_base<T, E>
    //=========================================================================

    ///////////////////////////////////////////////////////////////////////////
    /// \brief Base class of assignment to enable construction and assignment
    ///
    /// This class is used with several pieces of construction to ensure
    /// trivial constructibility and assignability:
    ///
    /// * `expected_trivial_copy_ctor_base`
    /// * `expected_trivial_move_ctor_base`
    /// * `expected_copy_assign_base`
    /// * `expected_move_assign_base`
    ///
    /// \tparam T the value type
    /// \tparam E the error type
    ///////////////////////////////////////////////////////////////////////////
    template <typename T, typename E>
    struct expected_construct_base
        : expected_destruct_base<T, E>
    {
      using base_type = expected_destruct_base<T, E>;

      using base_type::base_type;

      //-----------------------------------------------------------------------
      // Construction / Assignment
      //-----------------------------------------------------------------------

      /// \brief Constructs the value type from \p args
      ///
      /// \note This is an implementation detail only meant to be used during
      ///       construction
      ///
      /// \pre there is no contained value or error at the time of construction
      ///
      /// \param args the arguments to forward to T's constructor
      template <typename...Args>
      auto construct_value(Args&&...args)
        noexcept(std::is_nothrow_constructible<T,Args...>::value) -> void;

      /// \brief Constructs the error type from \p args
      ///
      /// \note This is an implementation detail only meant to be used during
      ///       construction
      ///
      /// \pre there is no contained value or error at the time of construction
      ///
      /// \param args the arguments to forward to E's constructor
      template <typename...Args>
      auto construct_error(Args&&...args)
        noexcept(std::is_nothrow_constructible<E,Args...>::value) -> void;

      /// \brief Constructs the underlying error from the \p other expected
      ///
      /// If \p other contains a value, then the T type will be
      /// default-constructed.
      ///
      /// \note This is an implementation detail only meant to be used during
      ///       construction of `expected<void, E>` types
      ///
      /// \pre there is no contained value or error at the time of construction
      ///
      /// \param other the other expected to construct
      template <typename Expected>
      auto construct_error_from_expected(Expected&& other) -> void;

      /// \brief Constructs the underlying type from an expected object
      ///
      /// \note This is an implementation detail only meant to be used during
      ///       construction
      ///
      /// \pre there is no contained value or error at the time of construction
      ///
      /// \param other the other expected to construct
      template <typename Expected>
      auto construct_from_expected(Expected&& other) -> void;

      //-----------------------------------------------------------------------

      template <typename Value>
      auto assign_value(Value&& value)
        noexcept(std::is_nothrow_assignable<T, Value>::value) -> void;

      template <typename Error>
      auto assign_error(Error&& error)
        noexcept(std::is_nothrow_assignable<E, Error>::value) -> void;

      template <typename Expected>
      auto assign_error_from_expected(Expected&& other) -> void;

      template <typename Expected>
      auto assign_from_expected(Expected&& other) -> void;
    };

    //=========================================================================
    // class : expected_trivial_copy_ctor_base
    //=========================================================================

    template <typename T, typename E>
    struct expected_trivial_copy_ctor_base_impl : expected_construct_base<T,E>
    {
      using base_type = expected_construct_base<T,E>;
      using base_type::base_type;

      expected_trivial_copy_ctor_base_impl(const expected_trivial_copy_ctor_base_impl& other)
        noexcept(std::is_nothrow_copy_constructible<T>::value &&
                 std::is_nothrow_copy_constructible<E>::value);
      expected_trivial_copy_ctor_base_impl(expected_trivial_copy_ctor_base_impl&& other) = default;

      auto operator=(const expected_trivial_copy_ctor_base_impl& other) -> expected_trivial_copy_ctor_base_impl& = default;
      auto operator=(expected_trivial_copy_ctor_base_impl&& other) -> expected_trivial_copy_ctor_base_impl& = default;
    };

    template <typename T, typename E>
    using expected_trivial_copy_ctor_base = typename std::conditional<
      std::is_trivially_copy_constructible<T>::value &&
      std::is_trivially_copy_constructible<E>::value,
      expected_construct_base<T,E>,
      expected_trivial_copy_ctor_base_impl<T,E>
    >::type;

    //=========================================================================
    // class : expected_trivial_move_ctor_base
    //=========================================================================

    template <typename T, typename E>
    struct expected_trivial_move_ctor_base_impl : expected_trivial_copy_ctor_base<T,E>
    {
      using base_type = expected_trivial_copy_ctor_base<T,E>;
      using base_type::base_type;

      expected_trivial_move_ctor_base_impl(const expected_trivial_move_ctor_base_impl& other) = default;
      expected_trivial_move_ctor_base_impl(expected_trivial_move_ctor_base_impl&& other)
        noexcept(std::is_nothrow_move_constructible<T>::value &&
                 std::is_nothrow_move_constructible<E>::value);

      auto operator=(const expected_trivial_move_ctor_base_impl& other) -> expected_trivial_move_ctor_base_impl& = default;
      auto operator=(expected_trivial_move_ctor_base_impl&& other) -> expected_trivial_move_ctor_base_impl& = default;
    };

    template <typename T, typename E>
    using expected_trivial_move_ctor_base = typename std::conditional<
      std::is_trivially_move_constructible<T>::value &&
      std::is_trivially_move_constructible<E>::value,
      typename expected_trivial_move_ctor_base_impl<T,E>::base_type,
      expected_trivial_move_ctor_base_impl<T,E>
    >::type;

    //=========================================================================
    // class : expected_trivial_copy_assign_base
    //=========================================================================

    template <typename T, typename E>
    struct expected_trivial_copy_assign_base_impl
      : expected_trivial_move_ctor_base<T, E>
    {
      using base_type = expected_trivial_move_ctor_base<T,E>;
      using base_type::base_type;

      expected_trivial_copy_assign_base_impl(const expected_trivial_copy_assign_base_impl& other) = default;
      expected_trivial_copy_assign_base_impl(expected_trivial_copy_assign_base_impl&& other) = default;

      auto operator=(const expected_trivial_copy_assign_base_impl& other)
        noexcept(std::is_nothrow_copy_constructible<T>::value &&
                 std::is_nothrow_copy_constructible<E>::value &&
                 std::is_nothrow_copy_assignable<T>::value &&
                 std::is_nothrow_copy_assignable<E>::value)
        -> expected_trivial_copy_assign_base_impl&;
      auto operator=(expected_trivial_copy_assign_base_impl&& other)
        -> expected_trivial_copy_assign_base_impl& = default;
    };

    template <typename T, typename E>
    using expected_trivial_copy_assign_base = typename std::conditional<
      std::is_trivially_copy_constructible<T>::value &&
      std::is_trivially_copy_constructible<E>::value &&
      std::is_trivially_copy_assignable<T>::value &&
      std::is_trivially_copy_assignable<E>::value &&
      std::is_trivially_destructible<T>::value &&
      std::is_trivially_destructible<E>::value,
      expected_trivial_move_ctor_base<T,E>,
      expected_trivial_copy_assign_base_impl<T,E>
    >::type;

    //=========================================================================
    // class : expected_trivial_move_assign_base
    //=========================================================================

    template <typename T, typename E>
    struct expected_trivial_move_assign_base_impl
      : expected_trivial_copy_assign_base<T, E>
    {
      using base_type = expected_trivial_copy_assign_base<T,E>;
      using base_type::base_type;

      expected_trivial_move_assign_base_impl(const expected_trivial_move_assign_base_impl& other) = default;
      expected_trivial_move_assign_base_impl(expected_trivial_move_assign_base_impl&& other) = default;

      auto operator=(const expected_trivial_move_assign_base_impl& other)
        -> expected_trivial_move_assign_base_impl& = default;
      auto operator=(expected_trivial_move_assign_base_impl&& other)
        noexcept(std::is_nothrow_move_constructible<T>::value &&
                 std::is_nothrow_move_constructible<E>::value &&
                 std::is_nothrow_move_assignable<T>::value &&
                 std::is_nothrow_move_assignable<E>::value)
        -> expected_trivial_move_assign_base_impl&;
    };

    template <typename T, typename E>
    using expected_trivial_move_assign_base = typename std::conditional<
      std::is_trivially_move_constructible<T>::value &&
      std::is_trivially_move_constructible<E>::value &&
      std::is_trivially_move_assignable<T>::value &&
      std::is_trivially_move_assignable<E>::value &&
      std::is_trivially_destructible<T>::value &&
      std::is_trivially_destructible<E>::value,
      expected_trivial_copy_assign_base<T,E>,
      expected_trivial_move_assign_base_impl<T,E>
    >::type;

    //=========================================================================
    // class : expected_copy_ctor_base
    //=========================================================================

    template <typename T, typename E>
    struct expected_copy_ctor_base_impl : expected_trivial_move_assign_base<T,E>
    {
      using base_type = expected_trivial_move_assign_base<T,E>;
      using base_type::base_type;

      expected_copy_ctor_base_impl(const expected_copy_ctor_base_impl& other) = delete;
      expected_copy_ctor_base_impl(expected_copy_ctor_base_impl&& other) = default;

      auto operator=(const expected_copy_ctor_base_impl& other)
        -> expected_copy_ctor_base_impl& = default;
      auto operator=(expected_copy_ctor_base_impl&& other)
        -> expected_copy_ctor_base_impl& = default;
    };

    template <typename T, typename E>
    using expected_copy_ctor_base = typename std::conditional<
      std::is_copy_constructible<T>::value &&
      std::is_copy_constructible<E>::value,
      expected_trivial_move_assign_base<T,E>,
      expected_copy_ctor_base_impl<T,E>
    >::type;

    //=========================================================================
    // class : expected_move_ctor_base
    //=========================================================================

    template <typename T, typename E>
    struct expected_move_ctor_base_impl : expected_copy_ctor_base<T,E>
    {
      using base_type = expected_copy_ctor_base<T,E>;
      using base_type::base_type;

      expected_move_ctor_base_impl(const expected_move_ctor_base_impl& other) = default;
      expected_move_ctor_base_impl(expected_move_ctor_base_impl&& other) = delete;

      auto operator=(const expected_move_ctor_base_impl& other)
        -> expected_move_ctor_base_impl& = default;
      auto operator=(expected_move_ctor_base_impl&& other)
        -> expected_move_ctor_base_impl& = default;
    };

    template <typename T, typename E>
    using expected_move_ctor_base = typename std::conditional<
      std::is_move_constructible<T>::value &&
      std::is_move_constructible<E>::value,
      expected_copy_ctor_base<T,E>,
      expected_move_ctor_base_impl<T,E>
    >::type;

    //=========================================================================
    // class : expected_copy_assign_base
    //=========================================================================

    template <typename T, typename E>
    struct expected_copy_assign_base_impl
      : expected_move_ctor_base<T, E>
    {
      using base_type = expected_move_ctor_base<T, E>;
      using base_type::base_type;

      expected_copy_assign_base_impl(const expected_copy_assign_base_impl& other) = default;
      expected_copy_assign_base_impl(expected_copy_assign_base_impl&& other) = default;

      auto operator=(const expected_copy_assign_base_impl& other)
        -> expected_copy_assign_base_impl& = delete;
      auto operator=(expected_copy_assign_base_impl&& other)
        -> expected_copy_assign_base_impl& = default;
    };

    template <typename T, typename E>
    using expected_copy_assign_base = typename std::conditional<
      std::is_nothrow_copy_constructible<T>::value &&
      std::is_nothrow_copy_constructible<E>::value &&
      std::is_copy_assignable<T>::value &&
      std::is_copy_assignable<E>::value,
      typename expected_copy_assign_base_impl<T,E>::base_type,
      expected_copy_assign_base_impl<T,E>
    >::type;

    //=========================================================================
    // class : expected_move_assign_base
    //=========================================================================

    template <typename T, typename E>
    struct expected_move_assign_base_impl
      : expected_copy_assign_base<T, E>
    {
      using base_type = expected_copy_assign_base<T, E>;
      using base_type::base_type;

      expected_move_assign_base_impl(const expected_move_assign_base_impl& other) = default;
      expected_move_assign_base_impl(expected_move_assign_base_impl&& other) = default;

      auto operator=(const expected_move_assign_base_impl& other)
        -> expected_move_assign_base_impl& = default;
      auto operator=(expected_move_assign_base_impl&& other)
        -> expected_move_assign_base_impl& = delete;
    };

    template <typename T, typename E>
    using expected_move_assign_base = typename std::conditional<
      std::is_nothrow_move_constructible<T>::value &&
      std::is_nothrow_move_constructible<E>::value &&
      std::is_move_assignable<T>::value &&
      std::is_move_assignable<E>::value,
      typename expected_move_assign_base_impl<T,E>::base_type,
      expected_move_assign_base_impl<T,E>
    >::type;

    //=========================================================================
    // alias : expected_storage
    //=========================================================================

    template <typename T, typename E>
    using expected_storage = expected_move_assign_base<T, E>;

    //=========================================================================
    // traits : expected
    //=========================================================================

    template <typename T1, typename E1, typename T2, typename E2>
    using expected_is_convertible = std::integral_constant<bool,(
      // T1 constructible from expected<T2,E2>
      std::is_constructible<T1, expected<T2,E2>&>:: value ||
      std::is_constructible<T1, const expected<T2,E2>&>:: value ||
      std::is_constructible<T1, expected<T2,E2>&&>:: value ||
      std::is_constructible<T1, const expected<T2,E2>&&>:: value ||

      // E1 constructible from expected<T2,E2>
      std::is_constructible<E1, expected<T2,E2>&>:: value ||
      std::is_constructible<E1, const expected<T2,E2>&>:: value ||
      std::is_constructible<E1, expected<T2,E2>&&>:: value ||
      std::is_constructible<E1, const expected<T2,E2>&&>:: value ||

      // expected<T2,E2> convertible to T1
      std::is_convertible<expected<T2,E2>&, T1>:: value ||
      std::is_convertible<const expected<T2,E2>&, T1>:: value ||
      std::is_convertible<expected<T2,E2>&&, T1>::value ||
      std::is_convertible<const expected<T2,E2>&&, T1>::value ||

      // expected<T2,E2> convertible to E2
      std::is_convertible<expected<T2,E2>&, E1>:: value ||
      std::is_convertible<const expected<T2,E2>&, E1>:: value ||
      std::is_convertible<expected<T2,E2>&&, E1>::value ||
      std::is_convertible<const expected<T2,E2>&&, E1>::value
    )>;

    //-------------------------------------------------------------------------

    template <typename T1, typename E1, typename T2, typename E2>
    using expected_is_copy_convertible = std::integral_constant<bool,(
      !expected_is_convertible<T1,E1,T2,E2>::value &&
      std::is_constructible<T1, const T2&>::value &&
      std::is_constructible<E1, const E2&>::value
    )>;
    template <typename T1, typename E1, typename T2, typename E2>
    using expected_is_implicit_copy_convertible = std::integral_constant<bool,(
      expected_is_copy_convertible<T1,E1,T2,E2>::value &&
      std::is_convertible<const T2&, T1>::value &&
      std::is_convertible<const E2&, E1>::value
    )>;
    template <typename T1, typename E1, typename T2, typename E2>
    using expected_is_explicit_copy_convertible = std::integral_constant<bool,(
      expected_is_copy_convertible<T1,E1,T2,E2>::value &&
      (!std::is_convertible<const T2&, T1>::value ||
       !std::is_convertible<const E2&, E1>::value)
    )>;

    //-------------------------------------------------------------------------

    template <typename T1, typename E1, typename T2, typename E2>
    using expected_is_move_convertible = std::integral_constant<bool,(
      !expected_is_convertible<T1,E1,T2,E2>::value &&
      std::is_constructible<T1, T2&&>::value &&
      std::is_constructible<E1, E2&&>::value
    )>;
    template <typename T1, typename E1, typename T2, typename E2>
    using expected_is_implicit_move_convertible = std::integral_constant<bool,(
      expected_is_move_convertible<T1,E1,T2,E2>::value &&
      std::is_convertible<T2&&, T1>::value &&
      std::is_convertible<E2&&, E1>::value
    )>;
    template <typename T1, typename E1, typename T2, typename E2>
    using expected_is_explicit_move_convertible = std::integral_constant<bool,(
      expected_is_move_convertible<T1,E1,T2,E2>::value &&
      (!std::is_convertible<T2&&, T1>::value ||
       !std::is_convertible<E2&&, E1>::value)
    )>;

    //-------------------------------------------------------------------------

    template <typename T, typename U>
    using expected_is_value_convertible = std::integral_constant<bool,(
      std::is_constructible<T, U&&>::value &&
      !std::is_same<typename std::decay<U>::type, in_place_t>::value &&
      !std::is_same<typename std::decay<U>::type, in_place_error_t>::value &&
      !is_expected<typename std::decay<U>::type>::value
    )>;
    template <typename T, typename U>
    using expected_is_explicit_value_convertible = std::integral_constant<bool,(
      expected_is_value_convertible<T, U>::value &&
      !std::is_convertible<U&&, T>::value
    )>;
    template <typename T, typename U>
    using expected_is_implicit_value_convertible = std::integral_constant<bool,(
      expected_is_value_convertible<T, U>::value &&
      std::is_convertible<U&&, T>::value
    )>;

    //-------------------------------------------------------------------------

    template <typename T1, typename E1, typename T2, typename E2>
    using expected_is_convert_assignable = std::integral_constant<bool,(
      !expected_is_convertible<T1,E1,T2,E2>::value &&

      !std::is_assignable<T1&, expected<T2,E2>&>::value &&
      !std::is_assignable<T1&, const expected<T2,E2>&>::value &&
      !std::is_assignable<T1&, expected<T2,E2>&&>::value &&
      !std::is_assignable<T1&, const expected<T2,E2>&&>::value &&

      !std::is_assignable<E1&, expected<T2,E2>&>::value &&
      !std::is_assignable<E1&, const expected<T2,E2>&>::value &&
      !std::is_assignable<E1&, expected<T2,E2>&&>::value &&
      !std::is_assignable<E1&, const expected<T2,E2>&&>::value
    )>;
    template <typename T1, typename E1, typename T2, typename E2>
    using expected_is_copy_convert_assignable = std::integral_constant<bool,(
      !expected_is_convertible<T1,E1,T2,E2>::value::value &&

      std::is_nothrow_constructible<T1, const T2&>::value &&
      std::is_assignable<T1&, const T2&>::value &&
      std::is_nothrow_constructible<E1, const E2&>::value &&
      std::is_assignable<E1&, const E2&>::value
    )>;
    template <typename T1, typename E1, typename T2, typename E2>
    using expected_is_move_convert_assignable = std::integral_constant<bool,(
      !expected_is_convertible<T1,E1,T2,E2>::value::value &&

      std::is_nothrow_constructible<T1, const T2&>::value &&
      std::is_assignable<T1&, const T2&>::value &&
      std::is_nothrow_constructible<E1, const E2&>::value &&
      std::is_assignable<E1&, const E2&>::value
    )>;

    //-------------------------------------------------------------------------

    template <typename T, typename U>
    using expected_is_value_assignable = std::integral_constant<bool,(
      !is_expected<typename std::decay<U>::type>::value &&
      !is_unexpected<typename std::decay<U>::type>::value &&
      std::is_nothrow_constructible<T,U>::value &&
      std::is_assignable<T,U>::value &&
      (
        !std::is_same<typename std::decay<U>::type,T>::value ||
        !std::is_scalar<T>::value
      )
    )>;

    template <typename E, typename E2>
    using expected_is_unexpected_assignable = std::integral_constant<bool,(
      std::is_nothrow_constructible<E,E2>::value &&
      std::is_assignable<E,E2>::value
    )>;

    // Friending 'extract_error" below was causing some compilers to incorrectly
    // identify `exp.m_storage.m_error` as being an access violation despite the
    // friendship. Using a type name instead seems to be ubiquitous across
    // compilers
    struct expected_error_extractor
    {
      template <typename T, typename E>
      static constexpr auto get(const expected<T,E>& exp) noexcept -> const E&;
    };

    template <typename T, typename E>
    constexpr auto extract_error(const expected<T,E>& exp) noexcept -> const E&;

    [[noreturn]]
    auto throw_bad_expected_access() -> void;

  } // namespace detail

  /////////////////////////////////////////////////////////////////////////////
  /// \brief The class template `expected` manages expected results from APIs,
  ///        while encoding possible failure conditions.
  ///
  /// A common use-case for expected is the return value of a function that
  /// may fail. As opposed to other approaches, such as `std::pair<T,bool>`
  /// or `std::optional`, `expected` more accurately conveys the intent of the
  /// user along with the failure condition to the caller. This effectively
  /// produces an orthogonal error handling mechanism that allows for exception
  /// safety while also allowing discrete testability of the return type.
  ///
  /// `expected<T,E>` types may contain a `T` value, which signifies that an
  /// operation succeeded in producing the expected value of type `T`. If an
  /// `expected` does not contain a `T` value, it will always contain an `E`
  /// error condition instead.
  ///
  /// An `expected<T,E>` can always be queried for a possible error case by
  /// calling the `error()` function, even if it contains a value.
  /// In the case that an `expected<T,E>` contains a value object, this will
  /// simply return an `E` object constructed through default aggregate
  /// construction, as if through the expression `E{}`, which is assumed to be
  /// a "valid" (no-error) state for an `E` type.
  /// For example:
  ///
  /// * `std::error_code{}` produces a default-construct error-code, which is
  ///   the "no error" state,
  /// * integral (or enum) error codes produce a `0` value (no error), thanks to
  ///   zero-initialization,
  /// * `std::exception_ptr{}` produces a null-pointer,
  /// * `std::string{}` produces an empty string `""`,
  /// * etc.
  ///
  /// When an `expected<T,E>` contains either a value or error, the storage for
  /// that object is guaranteed to be allocated as part of the expected
  /// object's footprint, i.e. no dynamic memory allocation ever takes place.
  /// Thus, an expected object models an object, not a pointer, even though the
  /// `operator*()` and `operator->()` are defined.
  ///
  /// When an object of type `expected<T,E>` is contextually converted to
  /// `bool`, the conversion returns `true` if the object contains a value and
  /// `false` if it contains an error.
  ///
  /// `expected` objects do not have a "valueless" state like `variant`s do.
  /// Once an `expected` has been constructed with a value or error, the
  /// active underlying type can only be changed through assignment which may
  /// is only enabled if construction is guaranteed to be *non-throwing*. This
  /// ensures that a valueless state cannot occur naturally.
  ///
  /// Example Use:
  /// \code
  /// auto to_string(int x) -> expected<std::string>
  /// {
  ///   try {
  ///     return std::stoi(x);
  ///   } catch (const std::invalid_argument&) {
  ///     return make_unexpected(std::errc::invalid_argument);
  ///   } catch (const std::std::out_of_range&) {
  ///     return make_unexpected(std::errc::result_out_of_range);
  ///   }
  /// }
  /// \endcode
  ///
  /// \note If using C++17 or above, `make_unexpected` can be replaced with
  ///       `unexpected{...}` thanks to CTAD.
  ///
  /// \tparam T the underlying value type
  /// \tparam E the underlying error type (`std::error_code` by default)
  ///////////////////////////////////////////////////////////////////////////
  template <typename T, typename E = std::error_code>
  class expected
  {
    // Type requirements

    static_assert(
      !std::is_abstract<T>::value,
      "It is ill-formed for T to be abstract type"
    );
    static_assert(
      !std::is_same<typename std::decay<T>::type, in_place_t>::value,
      "It is ill-formed for T to be a (possibly CV-qualified) in_place_t type"
    );
    static_assert(
      !is_expected<typename std::decay<T>::type>::value,
      "It is ill-formed for T to be a (possibly CV-qualified) 'expected' type"
    );
    static_assert(
      !is_unexpected<typename std::decay<T>::type>::value,
      "It is ill-formed for T to be a (possibly CV-qualified) 'unexpected' type"
    );

    static_assert(
      !std::is_abstract<E>::value,
      "It is ill-formed for E to be abstract type"
    );
    static_assert(
      !std::is_void<typename std::decay<E>::type>::value,
      "It is ill-formed for E to be (possibly CV-qualified) void type"
    );
    static_assert(
      !is_expected<typename std::decay<E>::type>::value,
      "It is ill-formed for E to be a (possibly CV-qualified) 'expected' type"
    );
    static_assert(
      !is_unexpected<typename std::decay<E>::type>::value,
      "It is ill-formed for E to be a (possibly CV-qualified) 'unexpected' type"
    );
    static_assert(
      !std::is_same<typename std::decay<E>::type, in_place_t>::value,
      "It is ill-formed for T to be a (possibly CV-qualified) in_place_t type"
    );

    // Friendship

    friend detail::expected_error_extractor;

    template <typename T2, typename E2>
    friend class expected;

    //-------------------------------------------------------------------------
    // Public Member Types
    //-------------------------------------------------------------------------
  public:

    using value_type = T; ///< The value type of this expected
    using error_type = E; ///< The error type of this expected

    //-------------------------------------------------------------------------
    // Constructor / Destructor / Assignment
    //-------------------------------------------------------------------------
  public:

    /// \brief Default-constructs an expected with the underlying value type
    ///        active
    ///
    /// This constructor is only enabled if T is default-constructible
    template <typename U=T,
              typename = typename std::enable_if<std::is_constructible<U>::value>::type>
    constexpr expected()
      noexcept(std::is_nothrow_constructible<U>::value);

    /// \brief Copy constructs this expected
    ///
    /// If \p other contains a value, initializes the contained value as if
    /// direct-initializing (but not direct-list-initializing) an object of
    /// type T with the expression *other.
    ///
    /// If other contains an error, constructs an object that contains a copy
    /// of that error.
    ///
    /// \note This constructor is defined as deleted if
    ///       `std::is_copy_constructible<T>::value` or
    ///       `std::is_copy_constructible<E>::value` is false
    ///
    /// \note This constructor is defined as trivial if both
    ///       `std::is_trivially_copy_constructible<T>::value` and
    ///       `std::is_trivially_copy_constructible<E>::value` are true
    ///
    /// \param other the expected to copy
    constexpr expected(const expected& other) = default;

    /// \brief Move constructs an expected
    ///
    /// If other contains a value, initializes the contained value as if
    /// direct-initializing (but not direct-list-initializing) an object
    /// of type T with the expression std::move(*other) and does not make
    /// other empty: a moved-from expected still contains a value, but the
    /// value itself is moved from.
    ///
    /// If other contains an error, move-constructs this expected from that
    /// error.
    ///
    /// \note This constructor is defined as deleted if
    ///       `std::is_move_constructible<T>::value` or
    ///       `std::is_move_constructible<E>::value` is false
    ///
    /// \note This constructor is defined as trivial if both
    ///       `std::is_trivially_move_constructible<T>::value` and
    ///       `std::is_trivially_move_constructible<E>::value` are true
    ///
    /// \param other the expected to move
    constexpr expected(expected&& other) = default;

    /// \{
    /// \brief Converting copy constructor
    ///
    /// If \p other contains a value, constructs an expected object
    /// that contains a value, initialized as if direct-initializing
    /// (but not direct-list-initializing) an object of type T with the
    /// expression `*other`.
    ///
    /// If \p other contains an error, constructs an expected object that
    /// contains an error, initialized as if direct-initializing
    /// (but not direct-list-initializing) an object of type E.
    ///
    /// \note This constructor does not participate in overload resolution
    ///       unless the following conditions are met:
    ///       - std::is_constructible_v<T, const U&> is true
    ///       - T is not constructible or convertible from any expression
    ///         of type (possibly const) expected<T2,E2>
    ///       - E is not constructible or convertible from any expression
    ///         of type (possible const) expected<T2,E2>
    ///
    /// \note This constructor is explicit if and only if
    ///       `std::is_convertible_v<const T2&, T>` or
    ///       `std::is_convertible_v<const E2&, E>` is false
    ///
    /// \param other the other type to convert
    template <typename T2, typename E2,
              typename std::enable_if<detail::expected_is_implicit_copy_convertible<T,E,T2,E2>::value,int>::type = 0>
    expected(const expected<T2,E2>& other)
      noexcept(std::is_nothrow_constructible<T,const T2&>::value &&
               std::is_nothrow_constructible<E,const E2&>::value);
    template <typename T2, typename E2,
              typename std::enable_if<detail::expected_is_explicit_copy_convertible<T,E,T2,E2>::value,int>::type = 0>
    explicit expected(const expected<T2,E2>& other)
      noexcept(std::is_nothrow_constructible<T,const T2&>::value &&
               std::is_nothrow_constructible<E,const E2&>::value);
    /// \}

    /// \{
    /// \brief Converting move constructor
    ///
    /// If \p other contains a value, constructs an expected object
    /// that contains a value, initialized as if direct-initializing
    /// (but not direct-list-initializing) an object of type T with the
    /// expression `std::move(*other)`.
    ///
    /// If \p other contains an error, constructs an expected object that
    /// contains an error, initialized as if direct-initializing
    /// (but not direct-list-initializing) an object of type E&&.
    ///
    /// \note This constructor does not participate in overload resolution
    ///       unless the following conditions are met:
    ///       - std::is_constructible_v<T, const U&> is true
    ///       - T is not constructible or convertible from any expression
    ///         of type (possibly const) expected<T2,E2>
    ///       - E is not constructible or convertible from any expression
    ///         of type (possible const) expected<T2,E2>
    ///
    /// \note This constructor is explicit if and only if
    ///       `std::is_convertible_v<const T2&, T>` or
    ///       `std::is_convertible_v<const E2&, E>` is false
    ///
    /// \param other the other type to convert
    template <typename T2, typename E2,
              typename std::enable_if<detail::expected_is_implicit_move_convertible<T,E,T2,E2>::value,int>::type = 0>
    expected(expected<T2,E2>&& other)
      noexcept(std::is_nothrow_constructible<T,T2&&>::value &&
               std::is_nothrow_constructible<E,E2&&>::value);
    template <typename T2, typename E2,
              typename std::enable_if<detail::expected_is_explicit_move_convertible<T,E,T2,E2>::value,int>::type = 0>
    explicit expected(expected<T2,E2>&& other)
      noexcept(std::is_nothrow_constructible<T,T2&&>::value &&
               std::is_nothrow_constructible<E,E2&&>::value);
    /// \}

    //-------------------------------------------------------------------------

    /// \brief Constructs an expected object that contains a value
    ///
    /// the value is initialized as if direct-initializing (but not
    /// direct-list-initializing) an object of type T from the arguments
    /// std::forward<Args>(args)...
    ///
    /// \param args the arguments to pass to T's constructor
    template <typename...Args,
              typename = typename std::enable_if<std::is_constructible<T,Args...>::value>::type>
    constexpr explicit expected(in_place_t, Args&&... args)
      noexcept(std::is_nothrow_constructible<T, Args...>::value);

    /// \brief Constructs an expected object that contains a value
    ///
    /// The value is initialized as if direct-initializing (but not
    /// direct-list-initializing) an object of type T from the arguments
    /// std::forward<std::initializer_list<U>>(ilist), std::forward<Args>(args)...
    ///
    /// \param ilist An initializer list of entries to forward
    /// \param args  the arguments to pass to T's constructor
    template <typename U, typename...Args,
              typename = typename std::enable_if<std::is_constructible<T, std::initializer_list<U>&, Args...>::value>::type>
    constexpr explicit expected(in_place_t,
                                std::initializer_list<U> ilist,
                                Args&&...args)
      noexcept(std::is_nothrow_constructible<T, std::initializer_list<U>, Args...>::value);

    //-------------------------------------------------------------------------

    /// \brief Constructs an expected object that contains an error
    ///
    /// the value is initialized as if direct-initializing (but not
    /// direct-list-initializing) an object of type E from the arguments
    /// std::forward<Args>(args)...
    ///
    /// \param args the arguments to pass to E's constructor
    template <typename...Args,
              typename = typename std::enable_if<std::is_constructible<E,Args...>::value>::type>
    constexpr explicit expected(in_place_error_t, Args&&... args)
      noexcept(std::is_nothrow_constructible<E, Args...>::value);

    /// \brief Constructs an expected object that contains an error
    ///
    /// The value is initialized as if direct-initializing (but not
    /// direct-list-initializing) an object of type E from the arguments
    /// std::forward<std::initializer_list<U>>(ilist), std::forward<Args>(args)...
    ///
    /// \param ilist An initializer list of entries to forward
    /// \param args  the arguments to pass to Es constructor
    template <typename U, typename...Args,
              typename = typename std::enable_if<std::is_constructible<E, std::initializer_list<U>&, Args...>::value>::type>
    constexpr explicit expected(in_place_error_t,
                                std::initializer_list<U> ilist,
                                Args&&...args)
      noexcept(std::is_nothrow_constructible<E, std::initializer_list<U>, Args...>::value);

    //-------------------------------------------------------------------------

    /// \{
    /// \brief Constructs the underlying error of this expected
    ///
    /// \note This constructor only participates in overload resolution if
    ///       `E` is constructible from \p e
    ///
    /// \param e the unexpected error
    template <typename E2,
              typename = typename std::enable_if<std::is_constructible<E,const E2&>::value>::type>
    constexpr /* implicit */ expected(const unexpected<E2>& e)
      noexcept(std::is_nothrow_constructible<E,const E2&>::value);
    template <typename E2,
              typename = typename std::enable_if<std::is_constructible<E,E2&&>::value>::type>
    constexpr /* implicit */ expected(unexpected<E2>&& e)
      noexcept(std::is_nothrow_constructible<E,E2&&>::value);
    /// \}

    /// \{
    /// \brief Constructs an expected object that contains a value
    ///
    /// The value is initialized as if direct-initializing (but not
    /// direct-list-initializing) an object of type T with the expression
    /// value.
    ///
    /// \note This constructor is constexpr if the constructor of T
    ///       selected by direct-initialization is constexpr
    ///
    /// \note This constructor does not participate in overload
    ///       resolution unless `std::is_constructible_v<T, U&&>` is true
    ///       and `decay_t<U>` is neither `in_place_t`, `in_place_error_t`,
    ///       nor an `expected` type.
    ///
    /// \note This constructor is explicit if and only if
    ///       `std::is_convertible_v<U&&, T>` is `false`
    ///
    /// \param value the value to copy
    template <typename U,
              typename std::enable_if<detail::expected_is_explicit_value_convertible<T,U>::value,int>::type = 0>
    constexpr explicit expected(U&& value)
      noexcept(std::is_nothrow_constructible<T,U>::value);
    template <typename U,
              typename std::enable_if<detail::expected_is_implicit_value_convertible<T,U>::value,int>::type = 0>
    constexpr /* implicit */ expected(U&& value)
      noexcept(std::is_nothrow_constructible<T,U>::value);
    /// \}

    //-------------------------------------------------------------------------

    /// \brief Copy assigns the expected stored in \p other
    ///
    /// \note This assignment operator only participates in overload resolution
    ///       if the following conditions are met:
    ///       - `std::is_nothrow_copy_constructible_v<T>` is `true`, and
    ///       - `std::is_nothrow_copy_constructible_v<E>` is `true`
    ///       this restriction guarantees that no '
    ///
    /// \note This assignment operator is defined as trivial if the following
    ///       conditions are all `true`:
    ///       - `std::is_trivially_copy_constructible<T>::value`
    ///       - `std::is_trivially_copy_constructible<E>::value`
    ///       - `std::is_trivially_copy_assignable<T>::value`
    ///       - `std::is_trivially_copy_assignable<E>::value`
    ///       - `std::is_trivially_destructible<T>::value`
    ///       - `std::is_trivially_destructible<E>::value`
    ///
    /// \param other the other expected to copy
    auto operator=(const expected& other) -> expected& = default;

    /// \brief Move assigns the expected stored in \p other
    ///
    /// \note This assignment operator only participates in overload resolution
    ///       if the following conditions are met:
    ///       - `std::is_nothrow_copy_constructible_v<T>` is `true`, and
    ///       - `std::is_nothrow_copy_constructible_v<E>` is `true`
    ///       this restriction guarantees that no 'valueless_by_exception` state
    ///       may occur.
    ///
    /// \note This assignment operator is defined as trivial if the following
    ///       conditions are all `true`:
    ///       - `std::is_trivially_move_constructible<T>::value`
    ///       - `std::is_trivially_move_constructible<E>::value`
    ///       - `std::is_trivially_move_assignable<T>::value`
    ///       - `std::is_trivially_move_assignable<E>::value`
    ///       - `std::is_trivially_destructible<T>::value`
    ///       - `std::is_trivially_destructible<E>::value`
    ///
    /// \param other the other expected to copy
    auto operator=(expected&& other) -> expected& = default;

    /// \brief Copy-converts the state of \p other
    ///
    /// If both `*this` and \p other contain either values or errors, the
    /// underlying value is constructed as if through assignment.
    ///
    /// Otherwise if `*this` contains a value, but \p other contains an error,
    /// then the contained value is destroyed by calling its destructor. `*this`
    /// will no longer contain a value after the call, and will now contain `E`
    /// constructed as if direct-initializing (but not direct-list-initializing)
    /// an object with an argument of `const E2&`.
    ///
    /// If \p other contains a value and `*this` contains an error, then the
    /// contained error is destroyed by calling its destructor. `*this` now
    /// contains a value constructed as if direct-initializing (but not
    /// direct-list-initializing) an object with an argument of `const T2&`.
    ///
    /// \note The function does not participate in overload resolution unless
    ///       - `std::is_nothrow_constructible_v<T, const T2&>`,
    ///         `std::is_assignable_v<T&, const T2&>`,
    ///         `std::is_nothrow_constructible_v<E, const E2&>`,
    ///         `std::is_assignable_v<E&, const E2&>` are all true.
    ///       - T is not constructible, convertible, or assignable from any
    ///         expression of type (possibly const) `expected<T2,E2>`
    ///
    /// \param other the other expected object to convert
    /// \return reference to `(*this)`
    template <typename T2, typename E2,
              typename = typename std::enable_if<detail::expected_is_copy_convert_assignable<T,E,T2,E2>::value>::type>
    auto operator=(const expected<T2,E2>& other)
      noexcept(std::is_nothrow_assignable<T,const T2&>::value &&
               std::is_nothrow_assignable<E,const E2&>::value) -> expected&;

    /// \brief Move-converts the state of \p other
    ///
    /// If both `*this` and \p other contain either values or errors, the
    /// underlying value is constructed as if through move-assignment.
    ///
    /// Otherwise if `*this` contains a value, but \p other contains an error,
    /// then the contained value is destroyed by calling its destructor. `*this`
    /// will no longer contain a value after the call, and will now contain `E`
    /// constructed as if direct-initializing (but not direct-list-initializing)
    /// an object with an argument of `E2&&`.
    ///
    /// If \p other contains a value and `*this` contains an error, then the
    /// contained error is destroyed by calling its destructor. `*this` now
    /// contains a value constructed as if direct-initializing (but not
    /// direct-list-initializing) an object with an argument of `T2&&`.
    ///
    /// \note The function does not participate in overload resolution unless
    ///       - `std::is_nothrow_constructible_v<T, T2&&>`,
    ///         `std::is_assignable_v<T&, T2&&>`,
    ///         `std::is_nothrow_constructible_v<E, E2&&>`,
    ///         `std::is_assignable_v<E&, E2&&>` are all true.
    ///       - T is not constructible, convertible, or assignable from any
    ///         expression of type (possibly const) `expected<T2,E2>`
    ///
    /// \param other the other expected object to convert
    /// \return reference to `(*this)`
    template <typename T2, typename E2,
              typename = typename std::enable_if<detail::expected_is_move_convert_assignable<T,E,T2,E2>::value>::type>
    auto operator=(expected<T2,E2>&& other)
      noexcept(std::is_nothrow_assignable<T,T2&&>::value &&
               std::is_nothrow_assignable<E,E2&&>::value) -> expected&;

    /// \brief Perfect-forwarded assignment
    ///
    /// Depending on whether `*this` contains a value before the call, the
    /// contained value is either direct-initialized from std::forward<U>(value)
    /// or assigned from std::forward<U>(value).
    ///
    /// \note The function does not participate in overload resolution unless
    ///       - decay_t<U> is not an expected type,
    ///       - decay_t<U> is not an unexpected type
    ///       - std::is_nothrow_constructible_v<T, U> is true
    ///       - std::is_assignable_v<T&, U> is true
    ///       - and at least one of the following is true:
    ///           - T is not a scalar type;
    ///           - decay_t<U> is not T.
    ///
    /// \param value to assign to the contained value
    /// \return reference to `(*this)`
    template <typename U,
              typename = typename std::enable_if<detail::expected_is_value_assignable<T,U>::value>::type>
    auto operator=(U&& value)
      noexcept(std::is_nothrow_assignable<T,U>::value) -> expected&;

    /// \{
    /// \brief Perfect-forwarded assignment
    ///
    /// Depending on whether `*this` contains a value before the call, the
    /// contained value is either direct-initialized from std::forward<U>(value)
    /// or assigned from std::forward<U>(value).
    ///
    /// \note The function does not participate in overload resolution unless
    ///       - `std::is_nothrow_constructible_v<E, E2>` is `true`
    ///       - `std::is_assignable_v<T&, U>` is `true`
    ///       - and at least one of the following is true:
    ///           - `T` is not a scalar type;
    ///           - `decay_t<U>` is not `T`.
    ///
    /// \param value to assign to the contained value
    /// \return reference to `(*this)`
    template <typename E2,
              typename = typename std::enable_if<detail::expected_is_unexpected_assignable<E,const E2&>::value>::type>
    auto operator=(const unexpected<E2>& other)
      noexcept(std::is_nothrow_assignable<E, const E2&>::value) -> expected&;
    template <typename E2,
              typename = typename std::enable_if<detail::expected_is_unexpected_assignable<E,E2&&>::value>::type>
    auto operator=(unexpected<E2>&& other)
      noexcept(std::is_nothrow_assignable<E, E2&&>::value) -> expected&;
    /// \}

    //-------------------------------------------------------------------------
    // Observers
    //-------------------------------------------------------------------------
  public:

    /// \{
    /// \brief Accesses the contained value
    ///
    /// \note The behavior is undefined if `*this` does not contain a value.
    ///
    /// \return a pointer to the contained value
    EXPECTED_CPP14_CONSTEXPR auto operator->() noexcept -> value_type*;
    constexpr auto operator->() const noexcept -> const value_type*;
    /// \}

    /// \{
    /// \brief Accesses the contained value
    ///
    /// \note The behaviour is undefined if `*this` does not contain a value
    ///
    /// \return a reference to the contained value
    EXPECTED_CPP14_CONSTEXPR auto operator*() & noexcept -> value_type&;
    EXPECTED_CPP14_CONSTEXPR auto operator*() && noexcept -> value_type&&;
    constexpr auto operator*() const& noexcept -> const value_type&;
    constexpr auto operator*() const&& noexcept -> const value_type&&;
    /// \}

    /// \brief Checks whether `*this` contains a value
    ///
    /// \return `true` if `*this` contains a value, `false` if `*this`
    ///         does not contain a value
    constexpr explicit operator bool() const noexcept;

    /// \brief Checks whether `*this` contains a value
    ///
    /// \return `true` if `*this` contains a value, `false` if `*this`
    ///         contains an error
    constexpr auto has_value() const noexcept -> bool;

    /// \brief Checks whether `*this` contains an error
    ///
    /// \return `true` if `*this` contains an error, `false` if `*this`
    ///          contains a value
    constexpr auto has_error() const noexcept -> bool;

    //-------------------------------------------------------------------------

    /// \{
    /// \brief Returns the contained value.
    ///
    /// \throws bad_expected_access if `*this` does not contain a value.
    ///
    /// \return the value of `*this`
    EXPECTED_CPP14_CONSTEXPR auto value() & -> value_type&;
    EXPECTED_CPP14_CONSTEXPR auto value() && -> value_type&&;
    /// \}

    /// \{
    /// \brief Returns the contained value.
    ///
    /// \throws bad_optional_access if `*this` does not contain a value.
    ///
    /// \return the value of `*this`
    constexpr auto value() const & -> const value_type&;
    constexpr auto value() const && -> const value_type&&;
    /// \}

    /// \{
    /// \brief Returns the contained error, if one exists, or a
    ///        default-constructed error value
    ///
    /// \return the error or a default-constructed error value
    constexpr auto error() const &
      noexcept(std::is_nothrow_constructible<E>::value &&
               std::is_nothrow_copy_constructible<E>::value) -> E;
    EXPECTED_CPP14_CONSTEXPR auto error() &&
      noexcept(std::is_nothrow_constructible<E>::value &&
               std::is_nothrow_move_constructible<E>::value) -> E;
    /// }

    //-------------------------------------------------------------------------
    // Monadic Functionalities
    //-------------------------------------------------------------------------
  public:

    /// \{
    /// \brief Returns the contained value if `*this` has a value,
    ///        otherwise returns \p default_value.
    ///
    /// \param default_value the value to use in case `*this` contains an error
    /// \return the contained value or \p default_value
    template <typename U>
    constexpr auto value_or(U&& default_value) const & -> value_type;
    template <typename U>
    EXPECTED_CPP14_CONSTEXPR auto value_or(U&& default_value) && -> value_type;
    /// \}

    /// \{
    /// \brief Returns the contained error if `*this` has an error,
    ///        otherwise returns \p default_error.
    ///
    /// \param default_error the error to use in case `*this` is empty
    /// \return the contained value or \p default_error
    template <typename U>
    constexpr auto error_or(U&& default_error) const & -> error_type;
    template <typename U>
    EXPECTED_CPP14_CONSTEXPR auto error_or(U&& default_error) && -> error_type;
    /// \}

    //-------------------------------------------------------------------------

    /// \brief Returns an expected containing \p value if this expected contains
    ///        a value, otherwise returns an expected containing the current
    ///        error.
    ///
    /// \param value the value to return as an expected
    /// \return an expected of \p value if this contains a value
    template <typename U>
    constexpr auto and_then(U&& value) const -> expected<typename std::decay<U>::type,E>;

    /// \{
    /// \brief Invokes the function \p fn with the value of this expected as
    ///        the argument
    ///
    /// If this expected contains an error, an expected of the error is returned
    ///
    /// The function being called must return an `expected` type or the program
    /// is ill-formed
    ///
    /// If this is called on an rvalue of `expected` which contains an error,
    /// the returned `expected` is constructed from an rvalue of that error.
    ///
    /// \param fn the function to invoke with this
    /// \return The result of the function being called
    template <typename Fn>
    constexpr auto flat_map(Fn&& fn) const & -> detail::invoke_result_t<Fn, const T&>;
    template <typename Fn>
    EXPECTED_CPP14_CONSTEXPR auto flat_map(Fn&& fn) && -> detail::invoke_result_t<Fn, const T&>;
    /// \}

    /// \{
    /// \brief Invokes the function \p fn with the value of this expected as
    ///        the argument
    ///
    /// If this expected is an error, the result of this function is that
    /// error. Otherwise this function wraps the result and returns it as an
    /// expected.
    ///
    /// If this is called on an rvalue of `expected` which contains an error,
    /// the returned `expected` is constructed from an rvalue of that error.
    ///
    /// \param fn the function to invoke with this
    /// \return The expected result of the function invoked
    template <typename Fn>
    constexpr auto map(Fn&& fn) const & -> expected<detail::invoke_result_t<Fn,const T&>,E>;
    template <typename Fn>
    EXPECTED_CPP14_CONSTEXPR auto map(Fn&& fn) && -> expected<detail::invoke_result_t<Fn,const T&>,E>;
    /// \}

    /// \{
    /// \brief Invokes the function \p fn with the error of this expected as
    ///        the argument
    ///
    /// If this expected contains a value, the result of this function is that
    /// value. Otherwise the function is called with that error and returns the
    /// result as an expected.
    ///
    /// If this is called on an rvalue of `expected` which contains a value,
    /// the returned `expected` is constructed from an rvalue of that value.
    ///
    /// \param fn the function to invoke with this
    /// \return The expected result of the function invoked
    template <typename Fn>
    constexpr auto map_error(Fn&& fn) const & -> expected<T, detail::invoke_result_t<Fn,const E&>>;
    template <typename Fn>
    EXPECTED_CPP14_CONSTEXPR
    auto map_error(Fn&& fn) && -> expected<T, detail::invoke_result_t<Fn,const E&>>;
    /// \}

    //-------------------------------------------------------------------------
    // Private Members
    //-------------------------------------------------------------------------
  private:

    detail::expected_storage<T,E> m_storage;
  };

  //===========================================================================
  // class : expected<void,E>
  //===========================================================================

  /////////////////////////////////////////////////////////////////////////////
  /// \brief Partial specialization of `expected<void, E>`
  ///
  /// \tparam E the underlying error type (`std::error_code` by default)
  /////////////////////////////////////////////////////////////////////////////
  template <typename E>
  class expected<void,E>
  {
    // Type requirements

    static_assert(
      !std::is_void<typename std::decay<E>::type>::value,
      "It is ill-formed for E to be (possibly CV-qualified) void type"
    );
    static_assert(
      !std::is_abstract<E>::value,
      "It is ill-formed for E to be abstract type"
    );
    static_assert(
      !is_unexpected<typename std::decay<E>::type>::value,
      "It is ill-formed for E to be a (possibly CV-qualified) 'unexpected' type"
    );
    static_assert(
      std::is_default_constructible<E>::value,
      "E must be default-constructible in order to be retrieved as the "
      "default unexpected state."
    );

    // Friendship

    friend detail::expected_error_extractor;

    template <typename T2, typename E2>
    friend class expected;

    //-------------------------------------------------------------------------
    // Public Member Types
    //-------------------------------------------------------------------------
  public:

    using value_type = void; ///< The value type of this expected
    using error_type = E;    ///< The error type of this expected

    //-------------------------------------------------------------------------
    // Constructor / Assignment
    //-------------------------------------------------------------------------
  public:

    /// \brief Constructs an `expected` object with
    constexpr expected() noexcept;

    /// \brief Copy constructs this expected
    ///
    /// If other contains an error, constructs an object that contains a copy
    /// of that error.
    ///
    /// \note This constructor is defined as deleted if
    ///       `std::is_copy_constructible<E>::value` is false
    ///
    /// \note This constructor is defined as trivial if both
    ///       `std::is_trivially_copy_constructible<E>::value` are true
    ///
    /// \param other the expected to copy
    constexpr expected(const expected& other) = default;

    /// \brief Move constructs an expected
    ///
    /// If other contains an error, move-constructs this expected from that
    /// error.
    ///
    /// \note This constructor is defined as deleted if
    ///       `std::is_move_constructible<E>::value` is false
    ///
    /// \note This constructor is defined as trivial if both
    ///       `std::is_trivially_move_constructible<E>::value` are true
    ///
    /// \param other the expected to move
    constexpr expected(expected&& other) = default;

    /// \brief Converting copy constructor
    ///
    /// If \p other contains a value, constructs an expected object that is not
    /// in an error state -- ignoring the value.
    ///
    /// If \p other contains an error, constructs an expected object that
    /// contains an error, initialized as if direct-initializing
    /// (but not direct-list-initializing) an object of type E.
    ///
    /// \note This constructor does not participate in overload resolution
    ///       unless the following conditions are met:
    ///       - std::is_constructible_v<E, const E2&> is true
    ///       - E is not constructible or convertible from any expression
    ///         of type (possible const) expected<T2,E2>
    ///
    /// \note This constructor is explicit if and only if
    ///       `std::is_convertible_v<const E2&, E>` is false
    ///
    /// \param other the other type to convert
    template <typename U, typename E2,
              typename = typename std::enable_if<std::is_constructible<E,E2>::value>::type>
    explicit expected(const expected<U,E2>& other)
      noexcept(std::is_nothrow_constructible<E,const E2&>::value);

    /// \brief Converting move constructor
    ///
    /// If \p other contains an error, constructs an expected object that
    /// contains an error, initialized as if direct-initializing
    /// (but not direct-list-initializing) an object of type E&&.
    ///
    /// \note This constructor does not participate in overload resolution
    ///       unless the following conditions are met:
    ///       - std::is_constructible_v<T, const U&> is true
    ///       - T is not constructible or convertible from any expression
    ///         of type (possibly const) expected<T2,E2>
    ///       - E is not constructible or convertible from any expression
    ///         of type (possible const) expected<T2,E2>
    ///
    /// \note This constructor is explicit if and only if
    ///       `std::is_convertible_v<const T2&, T>` or
    ///       `std::is_convertible_v<const E2&, E>` is false
    ///
    /// \param other the other type to convert
    template <typename U, typename E2,
              typename = typename std::enable_if<std::is_constructible<E,E2>::value>::type>
    explicit expected(expected<U,E2>&& other)
      noexcept(std::is_nothrow_constructible<E,E2&&>::value);

    //-------------------------------------------------------------------------

    /// \brief Constructs an expected object that contains an error
    ///
    /// the value is initialized as if direct-initializing (but not
    /// direct-list-initializing) an object of type E from the arguments
    /// std::forward<Args>(args)...
    ///
    /// \param args the arguments to pass to E's constructor
    template <typename...Args,
              typename = typename std::enable_if<std::is_constructible<E,Args...>::value>::type>
    constexpr explicit expected(in_place_error_t, Args&&... args)
      noexcept(std::is_nothrow_constructible<E, Args...>::value);

    /// \brief Constructs an expected object that contains an error
    ///
    /// The value is initialized as if direct-initializing (but not
    /// direct-list-initializing) an object of type E from the arguments
    /// std::forward<std::initializer_list<U>>(ilist), std::forward<Args>(args)...
    ///
    /// \param ilist An initializer list of entries to forward
    /// \param args  the arguments to pass to Es constructor
    template <typename U, typename...Args,
              typename = typename std::enable_if<std::is_constructible<E, std::initializer_list<U>&, Args...>::value>::type>
    constexpr explicit expected(in_place_error_t,
                                std::initializer_list<U> ilist,
                                Args&&...args)
      noexcept(std::is_nothrow_constructible<E, std::initializer_list<U>, Args...>::value);

    //-------------------------------------------------------------------------

    /// \{
    /// \brief Constructs the underlying error of this expected
    ///
    /// \note This constructor only participates in overload resolution if
    ///       `E` is constructible from \p e
    ///
    /// \param e the unexpected error
    template <typename E2,
              typename = typename std::enable_if<std::is_constructible<E,const E2&>::value>::type>
    constexpr /* implicit */ expected(const unexpected<E2>& e)
      noexcept(std::is_nothrow_constructible<E,const E2&>::value);
    template <typename E2,
              typename = typename std::enable_if<std::is_constructible<E,E2&&>::value>::type>
    constexpr /* implicit */ expected(unexpected<E2>&& e)
      noexcept(std::is_nothrow_constructible<E,E2&&>::value);
    /// \}

    //-------------------------------------------------------------------------

    /// \brief Copy assigns the expected stored in \p other
    ///
    /// \note The function does not participate in overload resolution unless
    ///       - `std::is_nothrow_copy_constructible_v<E>` is `true`
    ///       this restriction guarantees that no 'valueless_by_exception` state
    ///       may occur.
    ///
    /// \note This assignment operator is defined as trivial if the following
    ///       conditions are all `true`:
    ///       - `std::is_trivially_copy_constructible<E>::value`
    ///       - `std::is_trivially_copy_assignable<E>::value`
    ///       - `std::is_trivially_destructible<E>::value`
    ///
    /// \param other the other expected to copy
    auto operator=(const expected& other) -> expected& = default;

    /// \brief Move assigns the expected stored in \p other
    ///
    /// \note The function does not participate in overload resolution unless
    ///       - `std::is_nothrow_copy_constructible_v<E>` is `true`
    ///       this restriction guarantees that no 'valueless_by_exception` state
    ///       may occur.
    ///
    /// \note This assignment operator is defined as trivial if the following
    ///       conditions are all `true`:
    ///       - `std::is_trivially_move_constructible<E>::value`
    ///       - `std::is_trivially_move_assignable<E>::value`
    ///       - `std::is_trivially_destructible<E>::value`
    ///
    /// \param other the other expected to copy
    auto operator=(expected&& other) -> expected& = default;

    /// \brief Copy-converts the state of \p other
    ///
    /// If both this and \p other contain an error, the underlying error is
    /// assigned through copy-assignment.
    ///
    /// If \p other contains a value state, this expected is constructed in a
    /// value state.
    ///
    /// If \p other contans an error state, and this contains a value state,
    /// the underlying error is constructed through copy-construction.
    ///
    /// \note The function does not participate in overload resolution unless
    ///       - `std::is_nothrow_constructible_v<E, const E2&>`,
    ///         `std::is_assignable_v<E&, const E2&>` are all true.
    ///
    /// \param other the other expected object to convert
    /// \return reference to `(*this)`
    template <typename T2, typename E2,
              typename = typename std::enable_if<std::is_nothrow_constructible<E,const E2&>::value &&
                                                 std::is_assignable<E,const E2&>::value>::type>
    auto operator=(const expected<T2,E2>& other)
      noexcept(std::is_nothrow_assignable<E, const E2&>::value) -> expected&;

    /// \brief Move-converts the state of \p other
    ///
    /// If both this and \p other contain an error, the underlying error is
    /// assigned through move-assignment.
    ///
    /// If \p other contains a value state, this expected is constructed in a
    /// value state.
    ///
    /// If \p other contans an error state, and this contains a value state,
    /// the underlying error is constructed through move-construction.
    ///
    /// \note The function does not participate in overload resolution unless
    ///       - `std::is_nothrow_constructible_v<E, E2&&>`,
    ///         `std::is_assignable_v<E&, E2&&>` are all true.
    ///
    /// \param other the other expected object to convert
    /// \return reference to `(*this)`
    template <typename T2, typename E2,
              typename = typename std::enable_if<std::is_nothrow_constructible<E,E2&&>::value &&
                                                 std::is_assignable<E,E2&&>::value>::type>
    auto operator=(expected<T2,E2>&& other)
      noexcept(std::is_nothrow_assignable<E, E2&&>::value) -> expected&;

    /// \{
    /// \brief Perfect-forwarded assignment
    ///
    /// Depending on whether `*this` contains a value before the call, the
    /// contained value is either direct-initialized from std::forward<U>(value)
    /// or assigned from std::forward<U>(value).
    ///
    /// \note The function does not participate in overload resolution unless
    ///       - `std::is_nothrow_constructible_v<E, E2>` is `true`
    ///       - `std::is_assignable_v<T&, U>` is `true`
    ///
    /// \param value to assign to the contained value
    /// \return reference to `(*this)`
    template <typename E2,
              typename = typename std::enable_if<detail::expected_is_unexpected_assignable<E,const E2&>::value>::type>
    auto operator=(const unexpected<E2>& other)
      noexcept(std::is_nothrow_assignable<E, const E2&>::value) -> expected&;
    template <typename E2,
              typename = typename std::enable_if<detail::expected_is_unexpected_assignable<E,E2&&>::value>::type>
    auto operator=(unexpected<E2>&& other)
      noexcept(std::is_nothrow_assignable<E, E2&&>::value) -> expected&;
    /// \}

    //-------------------------------------------------------------------------
    // Observers
    //-------------------------------------------------------------------------
  public:

    /// \brief Checks whether `*this` contains a value
    ///
    /// \return `true` if `*this` contains a value, `false` if `*this`
    ///         does not contain a value
    constexpr explicit operator bool() const noexcept;

    /// \brief Checks whether `*this` contains a value
    ///
    /// \return `true` if `*this` contains a value, `false` if `*this`
    ///         contains an error
    constexpr auto has_value() const noexcept -> bool;

    /// \brief Checks whether `*this` contains an error
    ///
    /// \return `true` if `*this` contains an error, `false` if `*this`
    ///          contains a value
    constexpr auto has_error() const noexcept -> bool;

    //-------------------------------------------------------------------------

    /// \brief Throws an exception if contains an error
    ///
    /// \throws bad_expected_access if `*this` contains an error.
    EXPECTED_CPP14_CONSTEXPR auto value() const -> void;

    /// \{
    /// \brief Returns the contained error, if one exists, or a
    ///        default-constructed error value
    ///
    /// \return the error or a default-constructed error value
    constexpr auto error() const &
      noexcept(std::is_nothrow_constructible<E>::value &&
               std::is_nothrow_copy_constructible<E>::value) -> E;
    EXPECTED_CPP14_CONSTEXPR auto error() &&
      noexcept(std::is_nothrow_constructible<E>::value &&
               std::is_nothrow_copy_constructible<E>::value) -> E;
    /// \}

    //-------------------------------------------------------------------------
    // Monadic Functionalities
    //-------------------------------------------------------------------------
  public:

    /// \{
    /// \brief Returns the contained error if `*this` has an error,
    ///        otherwise returns \p default_error.
    ///
    /// \param default_error the error to use in case `*this` is empty
    /// \return the contained value or \p default_error
    template <typename U>
    constexpr auto error_or(U&& default_error) const & -> error_type;
    template <typename U>
    EXPECTED_CPP14_CONSTEXPR auto error_or(U&& default_error) && -> error_type;
    /// \}

    //-------------------------------------------------------------------------

    /// \brief Returns an expected containing \p value if this expected contains
    ///        a value, otherwise returns an expected containing the current
    ///        error.
    ///
    /// \param value the value to return as an expected
    /// \return an expected of \p value if this contains a value
    template <typename U>
    constexpr auto and_then(U&& value) const -> expected<typename std::decay<U>::type,E>;

    /// \{
    /// \brief Invokes the function \p fn with the value of this expected as
    ///        the argument
    ///
    /// If this expected contains an error, an expected of the error is returned
    ///
    /// The function being called must return an `expected` type or the program
    /// is ill-formed
    ///
    /// If this is called on an rvalue of `expected` which contains an error,
    /// the returned `expected` is constructed from an rvalue of that error.
    ///
    /// \param fn the function to invoke with this
    /// \return The result of the function being called
    template <typename Fn>
    constexpr auto flat_map(Fn&& fn) const & -> detail::invoke_result_t<Fn>;
    template <typename Fn>
    EXPECTED_CPP14_CONSTEXPR auto flat_map(Fn&& fn) && -> detail::invoke_result_t<Fn>;
    /// \}

    /// \{
    /// \brief Invokes the function \p fn with the value of this expected as
    ///        the argument
    ///
    /// If this expected is an error, the result of this function is that
    /// error. Otherwise this function wraps the result and returns it as an
    /// expected.
    ///
    /// If this is called on an rvalue of `expected` which contains an error,
    /// the returned `expected` is constructed from an rvalue of that error.
    ///
    /// \param fn the function to invoke with this
    /// \return The expected result of the function invoked
    template <typename Fn>
    constexpr auto map(Fn&& fn) const & -> expected<detail::invoke_result_t<Fn>,E>;
    template <typename Fn>
    EXPECTED_CPP14_CONSTEXPR auto map(Fn&& fn) && -> expected<detail::invoke_result_t<Fn>,E>;
    /// \}

    /// \brief Invokes the function \p fn with the error of this expected as
    ///        the argument
    ///
    /// If this expected contains a value, the result of this function is that
    /// value. Otherwise the function is called with that error and returns the
    /// result as an expected.
    ///
    /// \param fn the function to invoke with this
    /// \return The expected result of the function invoked
    template <typename Fn>
    constexpr auto map_error(Fn&& fn) const -> expected<void, detail::invoke_result_t<Fn,const E&>>;

    //-------------------------------------------------------------------------
    // Private Members
    //-------------------------------------------------------------------------
  private:

    detail::expected_storage<detail::unit,E> m_storage;
  };

  //===========================================================================
  // non-member functions : class : expected
  //===========================================================================

  //---------------------------------------------------------------------------
  // Comparison
  //---------------------------------------------------------------------------

  template <typename T1, typename E1, typename T2, typename E2>
  constexpr auto operator==(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
    noexcept -> bool;
  template <typename T1, typename E1, typename T2, typename E2>
  constexpr auto operator!=(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
    noexcept -> bool;
  template <typename T1, typename E1, typename T2, typename E2>
  constexpr auto operator>=(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
    noexcept -> bool;
  template <typename T1, typename E1, typename T2, typename E2>
  constexpr auto operator<=(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
    noexcept -> bool;
  template <typename T1, typename E1, typename T2, typename E2>
  constexpr auto operator>(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
    noexcept -> bool;
  template <typename T1, typename E1, typename T2, typename E2>
  constexpr auto operator<(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
    noexcept -> bool;

  //---------------------------------------------------------------------------

  template <typename E1, typename E2>
  constexpr auto operator==(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
    noexcept -> bool;
  template <typename E1, typename E2>
  constexpr auto operator!=(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
    noexcept -> bool;
  template <typename E1, typename E2>
  constexpr auto operator>=(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
    noexcept -> bool;
  template <typename E1, typename E2>
  constexpr auto operator<=(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
    noexcept -> bool;
  template <typename E1, typename E2>
  constexpr auto operator>(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
    noexcept -> bool;
  template <typename E1, typename E2>
  constexpr auto operator<(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
    noexcept -> bool;

  //---------------------------------------------------------------------------

  template <typename T, typename E, typename U,
            typename = typename std::enable_if<!std::is_same<T,void>::value>::type>
  constexpr auto operator==(const expected<T,E>& exp, const U& value)
    noexcept -> bool;
  template <typename T, typename U, typename E,
            typename = typename std::enable_if<!std::is_same<U,void>::value>::type>
  constexpr auto operator==(const T& value, const expected<U,E>& exp)
    noexcept -> bool;
  template <typename T, typename E, typename U,
            typename = typename std::enable_if<!std::is_same<T,void>::value>::type>
  constexpr auto operator!=(const expected<T,E>& exp, const U& value)
    noexcept -> bool;
  template <typename T, typename U, typename E,
            typename = typename std::enable_if<!std::is_same<U,void>::value>::type>
  constexpr auto operator!=(const T& value, const expected<U,E>& exp)
    noexcept -> bool;
  template <typename T, typename E, typename U,
            typename = typename std::enable_if<!std::is_same<T,void>::value>::type>
  constexpr auto operator<=(const expected<T,E>& exp, const U& value)
    noexcept -> bool;
  template <typename T, typename U, typename E,
            typename = typename std::enable_if<!std::is_same<U,void>::value>::type>
  constexpr auto operator<=(const T& value, const expected<U,E>& exp)
    noexcept -> bool;
  template <typename T, typename E, typename U,
            typename = typename std::enable_if<!std::is_same<T,void>::value>::type>
  constexpr auto operator>=(const expected<T,E>& exp, const U& value)
    noexcept -> bool;
  template <typename T, typename U, typename E,
            typename = typename std::enable_if<!std::is_same<U,void>::value>::type>
  constexpr auto operator>=(const T& value, const expected<U,E>& exp)
    noexcept -> bool;
  template <typename T, typename E, typename U,
            typename = typename std::enable_if<!std::is_same<T,void>::value>::type>
  constexpr auto operator<(const expected<T,E>& exp, const U& value)
    noexcept -> bool;
  template <typename T, typename U, typename E,
            typename = typename std::enable_if<!std::is_same<U,void>::value>::type>
  constexpr auto operator<(const T& value, const expected<U,E>& exp)
    noexcept -> bool;
  template <typename T, typename E, typename U,
            typename = typename std::enable_if<!std::is_same<T,void>::value>::type>
  constexpr auto operator>(const expected<T,E>& exp, const U& value)
    noexcept -> bool;
  template <typename T, typename U, typename E,
            typename = typename std::enable_if<!std::is_same<U,void>::value>::type>
  constexpr auto operator>(const T& value, const expected<U,E>& exp)
    noexcept -> bool;

  //---------------------------------------------------------------------------

  template <typename T, typename E, typename U>
  constexpr auto operator==(const expected<T,E>& exp, const unexpected<U>& value)
    noexcept -> bool;
  template <typename T, typename U, typename E>
  constexpr auto operator==(const unexpected<T>& value, const expected<E,U>& exp)
    noexcept -> bool;
  template <typename T, typename E, typename U>
  constexpr auto operator!=(const expected<T,E>& exp, const unexpected<U>& value)
    noexcept -> bool;
  template <typename T, typename U, typename E>
  constexpr auto operator!=(const unexpected<T>& value, const expected<E,U>& exp)
    noexcept -> bool;
  template <typename T, typename E, typename U>
  constexpr auto operator<=(const expected<T,E>& exp, const unexpected<U>& value)
    noexcept -> bool;
  template <typename T, typename U, typename E>
  constexpr auto operator<=(const unexpected<T>& value, const expected<E,U>& exp)
    noexcept -> bool;
  template <typename T, typename E, typename U>
  constexpr auto operator>=(const expected<T,E>& exp, const unexpected<U>& value)
    noexcept -> bool;
  template <typename T, typename U, typename E>
  constexpr auto operator>=(const unexpected<T>& value, const expected<E,U>& exp)
    noexcept -> bool;
  template <typename T, typename E, typename U>
  constexpr auto operator<(const expected<T,E>& exp, const unexpected<U>& value)
    noexcept -> bool;
  template <typename T, typename U, typename E>
  constexpr auto operator<(const unexpected<T>& value, const expected<E,U>& exp)
    noexcept -> bool;
  template <typename T, typename E, typename U>
  constexpr auto operator>(const expected<T,E>& exp, const unexpected<U>& value)
    noexcept -> bool;
  template <typename T, typename U, typename E>
  constexpr auto operator>(const unexpected<T>& value, const expected<E,U>& exp)
    noexcept -> bool;
} // namespace expect

namespace std {

  template <typename T, typename E>
  struct hash<::expect::expected<T,E>>
  {
    auto operator()(const expect::expected<T,E>& x) const -> std::size_t
    {
      if (x.has_value()) {
        return std::hash<T>{}(*x) + 1; // add '1' to differentiate from error case
      }
      return std::hash<E>{}(::expect::detail::extract_error(x));
    }
  };

  template <typename E>
  struct hash<::expect::expected<void,E>>
  {
    auto operator()(const expect::expected<void,E>& x) const -> std::size_t
    {
      if (x.has_value()) {
        return 0;
      }
      return std::hash<E>{}(::expect::detail::extract_error(x));
    }
  };

} // namespace std

//=============================================================================
// class : bad_expected_access
//=============================================================================

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

#if !defined(EXPECTED_DISABLE_EXCEPTIONS)

inline
auto expect::bad_expected_access::what()
  const noexcept -> const char*
{
  return "bad_expected_access";
}

#endif

//=============================================================================
// class : unexpected
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------

template <typename E>
template <typename...Args, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::unexpected<E>::unexpected(in_place_t, Args&&...args)
  noexcept(std::is_nothrow_constructible<E, Args...>::value)
  : m_unexpected(detail::forward<Args>(args)...)
{

}

template <typename E>
template <typename E2,
          typename std::enable_if<!std::is_lvalue_reference<E2>::value && std::is_constructible<E,const E2&>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::unexpected<E>::unexpected(const error_type& error)
  noexcept(std::is_nothrow_copy_constructible<E>::value)
  : m_unexpected(error)
{

}

template <typename E>
template <typename E2,
          typename std::enable_if<!std::is_lvalue_reference<E2>::value && std::is_constructible<E,E2&&>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::unexpected<E>::unexpected(error_type&& error)
  noexcept(std::is_nothrow_move_constructible<E>::value)
  : m_unexpected(error)
{

}

template <typename E>
template <typename E2,
          typename std::enable_if<std::is_lvalue_reference<E2>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::unexpected<E>::unexpected(error_type& error)
  noexcept
  : m_unexpected(error)
{

}

template <typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::unexpected<E>::unexpected(const unexpected<E2>& other)
  noexcept(std::is_nothrow_constructible<E,const E2&>::value)
  : m_unexpected(other.error())
{

}

template <typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::unexpected<E>::unexpected(unexpected<E2>&& other)
  noexcept(std::is_nothrow_constructible<E,E2&&>::value)
  : m_unexpected(static_cast<unexpected<E2>&&>(other).error())
{

}

//-----------------------------------------------------------------------------

template <typename E>
template <typename E2,
          typename std::enable_if<!std::is_lvalue_reference<E2>::value && std::is_assignable<E,const E2&>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::unexpected<E>::operator=(const error_type& error)
  noexcept(std::is_nothrow_copy_assignable<E>::value) -> unexpected&
{
  m_unexpected = error;

  return (*this);
}

template <typename E>
template <typename E2,
          typename std::enable_if<!std::is_lvalue_reference<E2>::value && std::is_assignable<E,E2&&>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::unexpected<E>::operator=(error_type&& error)
  noexcept(std::is_nothrow_move_assignable<E>::value) -> unexpected&
{
  m_unexpected = static_cast<error_type&&>(error);

  return (*this);
}

template <typename E>
template <typename E2,
          typename std::enable_if<std::is_lvalue_reference<E2>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::unexpected<E>::operator=(error_type& error)
  noexcept -> unexpected&
{
  m_unexpected = error;

  return (*this);
}

template <typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::unexpected<E>::operator=(const unexpected<E2>& other)
  noexcept(std::is_nothrow_assignable<E,const E2&>::value)
  -> unexpected&
{
  m_unexpected = other.error();

  return (*this);
}

template <typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::unexpected<E>::operator=(unexpected<E2>&& other)
  noexcept(std::is_nothrow_assignable<E,E2&&>::value)
  -> unexpected&
{
  m_unexpected = static_cast<unexpected<E2>&&>(other).error();

  return (*this);
}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

template <typename E>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::unexpected<E>::error()
  & noexcept -> error_type&
{
  return m_unexpected;
}

template <typename E>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::unexpected<E>::error()
  && noexcept -> error_type&&
{
  return static_cast<error_type&&>(static_cast<error_type&>(m_unexpected));
}

template <typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::unexpected<E>::error()
  const & noexcept -> const error_type&
{
  return m_unexpected;
}

template <typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr auto expect::unexpected<E>::error()
  const && noexcept -> const error_type&&
{
  return static_cast<const error_type&&>(static_cast<const error_type&>(m_unexpected));
}

//=============================================================================
// non-member functions : class : unexpected
//=============================================================================

//-----------------------------------------------------------------------------
// Comparison
//-----------------------------------------------------------------------------

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator==(const unexpected<E1>& lhs,
                     const unexpected<E2>& rhs)
  noexcept -> bool
{
  return lhs.error() == rhs.error();
}

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator!=(const unexpected<E1>& lhs,
                     const unexpected<E2>& rhs)
  noexcept -> bool
{
  return lhs.error() != rhs.error();
}

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<(const unexpected<E1>& lhs,
                    const unexpected<E2>& rhs)
  noexcept -> bool
{
  return lhs.error() < rhs.error();
}

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>(const unexpected<E1>& lhs,
                    const unexpected<E2>& rhs)
  noexcept -> bool
{
  return lhs.error() > rhs.error();
}

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<=(const unexpected<E1>& lhs,
                     const unexpected<E2>& rhs)
  noexcept -> bool
{
  return lhs.error() <= rhs.error();
}

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>=(const unexpected<E1>& lhs,
                     const unexpected<E2>& rhs)
  noexcept -> bool
{
  return lhs.error() >= rhs.error();
}

//-----------------------------------------------------------------------------
// Utilities
//-----------------------------------------------------------------------------

template <typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::make_unexpected(E&& e)
  noexcept(std::is_nothrow_constructible<typename std::decay<E>::type,E>::value)
  -> unexpected<typename std::decay<E>::type>
{
  using result_type = unexpected<typename std::decay<E>::type>;

  return result_type(
    detail::forward<E>(e)
  );
}

template <typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::make_unexpected(std::reference_wrapper<E> e)
  noexcept -> unexpected<E&>
{
  using result_type = unexpected<E&>;

  return result_type{e.get()};
}

template <typename E, typename...Args, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::make_unexpected(Args&&...args)
  noexcept(std::is_nothrow_constructible<E, Args...>::value)
  -> unexpected<E>
{
  return unexpected<E>(in_place, detail::forward<Args>(args)...);
}

template <typename E>
inline EXPECTED_INLINE_VISIBILITY
auto expect::swap(unexpected<E>& lhs, unexpected<E>& rhs)
#if __cplusplus >= 201703L
    noexcept(std::is_nothrow_swappable<E>::value) -> void
#else
    noexcept -> void
#endif
{
  using std::swap;

  swap(lhs, rhs);
}

//=============================================================================
// class : detail::expected_destruct_base<T, E, IsTrivial>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Assignment
//-----------------------------------------------------------------------------

template <typename T, typename E, bool IsTrivial>
inline EXPECTED_INLINE_VISIBILITY
expect::detail::expected_destruct_base<T, E, IsTrivial>
  ::expected_destruct_base(unit)
  noexcept
  : m_empty{}
{
  // m_has_value intentionally not set
}

template <typename T, typename E, bool IsTrivial>
template <typename...Args>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::detail::expected_destruct_base<T,E,IsTrivial>
  ::expected_destruct_base(in_place_t, Args&&...args)
  noexcept(std::is_nothrow_constructible<T, Args...>::value)
  : m_value(detail::forward<Args>(args)...),
    m_has_value{true}
{
}

template <typename T, typename E, bool IsTrivial>
template <typename...Args>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::detail::expected_destruct_base<T,E,IsTrivial>
  ::expected_destruct_base(in_place_error_t, Args&&...args)
  noexcept(std::is_nothrow_constructible<E, Args...>::value)
  : m_error(detail::forward<Args>(args)...),
    m_has_value{false}
{
}

//-----------------------------------------------------------------------------
// Modifiers
//-----------------------------------------------------------------------------

template <typename T, typename E, bool IsTrivial>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_destruct_base<T, E, IsTrivial>::destroy()
  const noexcept -> void
{
  // do nothing
}

//=============================================================================
// class : detail::expected_destruct_base<T, E, false>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors / Destructor / Assignment
//-----------------------------------------------------------------------------

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY
expect::detail::expected_destruct_base<T, E, false>
  ::expected_destruct_base(unit)
  noexcept
  : m_empty{}
{
  // m_has_value intentionally not set
}

template <typename T, typename E>
template <typename...Args>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::detail::expected_destruct_base<T,E,false>
  ::expected_destruct_base(in_place_t, Args&&...args)
  noexcept(std::is_nothrow_constructible<T, Args...>::value)
  : m_value(detail::forward<Args>(args)...),
    m_has_value{true}
{
}

template <typename T, typename E>
template <typename...Args>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::detail::expected_destruct_base<T,E,false>
  ::expected_destruct_base(in_place_error_t, Args&&...args)
  noexcept(std::is_nothrow_constructible<E, Args...>::value)
  : m_error(detail::forward<Args>(args)...),
    m_has_value{false}
{
}

//-----------------------------------------------------------------------------

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY
expect::detail::expected_destruct_base<T,E,false>
  ::~expected_destruct_base()
  noexcept(std::is_nothrow_destructible<T>::value && std::is_nothrow_destructible<E>::value)
{
  destroy();
}

//-----------------------------------------------------------------------------
// Modifiers
//-----------------------------------------------------------------------------

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_destruct_base<T, E, false>::destroy()
  -> void
{
  if (m_has_value) {
    m_value.~underlying_value_type();
  } else {
    m_error.~underlying_error_type();
  }
}

//=============================================================================
// class : expected_construct_base<T, E>
//=============================================================================

//-----------------------------------------------------------------------------
// Construction / Assignment
//-----------------------------------------------------------------------------

template <typename T, typename E>
template <typename...Args>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_construct_base<T,E>::construct_value(Args&&...args)
  noexcept(std::is_nothrow_constructible<T,Args...>::value)
  -> void
{
  using value_type = typename base_type::underlying_value_type;

  auto* p = static_cast<void*>(std::addressof(this->m_value));
  new (p) value_type(detail::forward<Args>(args)...);
  this->m_has_value = true;
}

template <typename T, typename E>
template <typename...Args>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_construct_base<T,E>::construct_error(Args&&...args)
  noexcept(std::is_nothrow_constructible<E,Args...>::value)
  -> void
{
  using error_type = typename base_type::underlying_error_type;

  auto* p = static_cast<void*>(std::addressof(this->m_error));
  new (p) error_type(detail::forward<Args>(args)...);
  this->m_has_value = false;
}

template <typename T, typename E>
template <typename Expected>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_construct_base<T,E>::construct_error_from_expected(
  Expected&& other
) -> void
{
  if (other.m_has_value) {
    construct_value();
  } else {
    construct_error(detail::forward<Expected>(other).m_error);
  }
}


template <typename T, typename E>
template <typename Expected>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_construct_base<T,E>::construct_from_expected(
  Expected&& other
) -> void
{
  if (other.m_has_value) {
    construct_value(detail::forward<Expected>(other).m_value);
  } else {
    construct_error(detail::forward<Expected>(other).m_error);
  }
}

template <typename T, typename E>
template <typename Value>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_construct_base<T,E>::assign_value(Value&& value)
  noexcept(std::is_nothrow_assignable<T,Value>::value)
  -> void
{
  if (!base_type::m_has_value) {
    base_type::destroy();
    construct_value(detail::forward<Value>(value));
  } else {
    base_type::m_value = detail::forward<Value>(value);
  }
}

template <typename T, typename E>
template <typename Error>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_construct_base<T,E>::assign_error(Error&& error)
  noexcept(std::is_nothrow_assignable<E,Error>::value)
  -> void
{
  if (base_type::m_has_value) {
    base_type::destroy();
    construct_error(detail::forward<Error>(error));
  } else {
    base_type::m_error = detail::forward<Error>(error);
  }
}

template <typename T, typename E>
template <typename Expected>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_construct_base<T, E>::assign_error_from_expected(
  Expected&& other
) -> void
{
  if (other.m_has_value != base_type::m_has_value) {
    base_type::destroy();
    if (other.m_has_value) {
      construct_value();
    } else {
      construct_error(detail::forward<Expected>(other).m_error);
    }
  } else if (!other.m_has_value) {
    base_type::m_error = detail::forward<Expected>(other).m_error;
  }
}

template <typename T, typename E>
template <typename Expected>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_construct_base<T,E>::assign_from_expected(Expected&& other)
  -> void
{
  if (other.m_has_value != base_type::m_has_value) {
    base_type::destroy();
    construct_from_expected(detail::forward<Expected>(other));
  } else if (base_type::m_has_value) {
    base_type::m_value = detail::forward<Expected>(other).m_value;
  } else {
    base_type::m_error = detail::forward<Expected>(other).m_error;
  }
}

//=============================================================================
// class : expected_trivial_copy_ctor_base_impl
//=============================================================================

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY
expect::detail::expected_trivial_copy_ctor_base_impl<T,E>
  ::expected_trivial_copy_ctor_base_impl(const expected_trivial_copy_ctor_base_impl& other)
  noexcept(std::is_nothrow_copy_constructible<T>::value &&
           std::is_nothrow_copy_constructible<E>::value)
  : base_type(unit{})
{
  using ctor_base = expected_construct_base<T,E>;

  ctor_base::construct_from_expected(static_cast<const ctor_base&>(other));
}

//=============================================================================
// class : expected_trivial_move_ctor_base
//=============================================================================

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY
expect::detail::expected_trivial_move_ctor_base_impl<T, E>
  ::expected_trivial_move_ctor_base_impl(expected_trivial_move_ctor_base_impl&& other)
  noexcept(std::is_nothrow_move_constructible<T>::value &&
           std::is_nothrow_move_constructible<E>::value)
  : base_type(unit{})
{
  using ctor_base = expected_construct_base<T,E>;

  ctor_base::construct_from_expected(static_cast<ctor_base&&>(other));
}

//=============================================================================
// class : expected_copy_assign_base
//=============================================================================

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_trivial_copy_assign_base_impl<T, E>
  ::operator=(const expected_trivial_copy_assign_base_impl& other)
  noexcept(std::is_nothrow_copy_constructible<T>::value &&
           std::is_nothrow_copy_constructible<E>::value &&
           std::is_nothrow_copy_assignable<T>::value &&
           std::is_nothrow_copy_assignable<E>::value)
  -> expected_trivial_copy_assign_base_impl&
{
  using ctor_base = expected_construct_base<T,E>;

  ctor_base::assign_from_expected(static_cast<const ctor_base&>(other));
  return (*this);
}

//=========================================================================
// class : expected_move_assign_base
//=========================================================================

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::expected_trivial_move_assign_base_impl<T, E>
  ::operator=(expected_trivial_move_assign_base_impl&& other)
  noexcept(std::is_nothrow_move_constructible<T>::value &&
           std::is_nothrow_move_constructible<E>::value &&
           std::is_nothrow_move_assignable<T>::value &&
           std::is_nothrow_move_assignable<E>::value)
  -> expected_trivial_move_assign_base_impl&
{
  using ctor_base = expected_construct_base<T,E>;

  ctor_base::assign_from_expected(static_cast<ctor_base&&>(other));
  return (*this);
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::detail::expected_error_extractor::get(const expected<T,E>& exp)
  noexcept -> const E&
{
  return exp.m_storage.m_error;
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::detail::extract_error(const expected<T,E>& exp) noexcept -> const E&
{
  return expected_error_extractor::get(exp);
}

[[noreturn]]
inline EXPECTED_INLINE_VISIBILITY
auto expect::detail::throw_bad_expected_access() -> void
{
#if defined(EXPECTED_DISABLE_EXCEPTIONS)
  std::abort();
#else
  throw bad_expected_access{};
#endif
}




//=============================================================================
// class : expected<T,E>
//=============================================================================

template <typename T, typename E>
template <typename U, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<T, E>::expected()
  noexcept(std::is_nothrow_constructible<U>::value)
  : m_storage(in_place)
{

}

template <typename T, typename E>
template <typename T2, typename E2,
          typename std::enable_if<expect::detail::expected_is_implicit_copy_convertible<T,E,T2,E2>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY
expect::expected<T, E>::expected(const expected<T2,E2>& other)
  noexcept(std::is_nothrow_constructible<T,const T2&>::value &&
           std::is_nothrow_constructible<E,const E2&>::value)
  : m_storage(detail::unit{})
{
  m_storage.construct_from_expected(
    static_cast<const expected<T2,E2>&>(other).m_storage
  );
}

template <typename T, typename E>
template <typename T2, typename E2,
          typename std::enable_if<expect::detail::expected_is_explicit_copy_convertible<T,E,T2,E2>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY
expect::expected<T, E>::expected(const expected<T2,E2>& other)
  noexcept(std::is_nothrow_constructible<T,const T2&>::value &&
           std::is_nothrow_constructible<E,const E2&>::value)
  : m_storage(detail::unit{})
{
  m_storage.construct_from_expected(
    static_cast<const expected<T2,E2>&>(other).m_storage
  );
}

template <typename T, typename E>
template <typename T2, typename E2,
          typename std::enable_if<expect::detail::expected_is_implicit_move_convertible<T,E,T2,E2>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY
expect::expected<T, E>::expected(expected<T2,E2>&& other)
  noexcept(std::is_nothrow_constructible<T,T2&&>::value &&
           std::is_nothrow_constructible<E,E2&&>::value)
  : m_storage(detail::unit{})
{
  m_storage.construct_from_expected(
    static_cast<expected<T2,E2>&&>(other).m_storage
  );
}

template <typename T, typename E>
template <typename T2, typename E2,
          typename std::enable_if<expect::detail::expected_is_explicit_move_convertible<T,E,T2,E2>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY
expect::expected<T, E>::expected(expected<T2,E2>&& other)
  noexcept(std::is_nothrow_constructible<T,T2&&>::value &&
           std::is_nothrow_constructible<E,E2&&>::value)
  : m_storage(detail::unit{})
{
  m_storage.construct_from_expected(
    static_cast<expected<T2,E2>&&>(other).m_storage
  );
}

//-----------------------------------------------------------------------------

template <typename T, typename E>
template <typename...Args, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<T, E>::expected(in_place_t, Args&&...args)
  noexcept(std::is_nothrow_constructible<T, Args...>::value)
  : m_storage(in_place, detail::forward<Args>(args)...)
{

}

template <typename T, typename E>
template <typename U, typename...Args, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<T, E>::expected(in_place_t,
                                 std::initializer_list<U> ilist,
                                 Args&&...args)
  noexcept(std::is_nothrow_constructible<T, std::initializer_list<U>, Args...>::value)
  : m_storage(in_place, ilist, detail::forward<Args>(args)...)
{

}

//-----------------------------------------------------------------------------

template <typename T, typename E>
template <typename...Args, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<T, E>::expected(in_place_error_t, Args&&...args)
  noexcept(std::is_nothrow_constructible<E, Args...>::value)
  : m_storage(in_place_error, detail::forward<Args>(args)...)
{

}

template <typename T, typename E>
template <typename U, typename...Args, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<T, E>::expected(in_place_error_t,
                                 std::initializer_list<U> ilist,
                                 Args&&...args)
  noexcept(std::is_nothrow_constructible<E, std::initializer_list<U>, Args...>::value)
  : m_storage(in_place_error, ilist, detail::forward<Args>(args)...)
{

}

//-------------------------------------------------------------------------

template <typename T, typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<T, E>::expected(const unexpected<E2>& e)
  noexcept(std::is_nothrow_constructible<E,const E2&>::value)
  : m_storage(in_place_error, e.error())
{

}

template <typename T, typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<T, E>::expected(unexpected<E2>&& e)
  noexcept(std::is_nothrow_constructible<E,E2&&>::value)
  : m_storage(in_place_error, static_cast<E2&&>(e.error()))
{

}

template <typename T, typename E>
template <typename U,
          typename std::enable_if<expect::detail::expected_is_explicit_value_convertible<T,U>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<T, E>::expected(U&& value)
  noexcept(std::is_nothrow_constructible<T,U>::value)
  : m_storage(in_place, detail::forward<U>(value))
{

}

template <typename T, typename E>
template <typename U,
          typename std::enable_if<expect::detail::expected_is_implicit_value_convertible<T,U>::value,int>::type>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<T, E>::expected(U&& value)
  noexcept(std::is_nothrow_constructible<T,U>::value)
  : m_storage(in_place, detail::forward<U>(value))
{

}

//-----------------------------------------------------------------------------

template <typename T, typename E>
template <typename T2, typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY
auto expect::expected<T, E>::operator=(const expected<T2,E2>& other)
  noexcept(std::is_nothrow_assignable<T, const T2&>::value &&
           std::is_nothrow_assignable<E, const E2&>::value)
  -> expected&
{
  m_storage.assign_from_expected(
    static_cast<const expected<T2,E2>&>(other).m_storage
  );
  return (*this);
}

template <typename T, typename E>
template <typename T2, typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY
auto expect::expected<T, E>::operator=(expected<T2,E2>&& other)
  noexcept(std::is_nothrow_assignable<T, T2&&>::value &&
           std::is_nothrow_assignable<E, E2&&>::value)
  -> expected&
{
  m_storage.assign_from_expected(
    static_cast<expected<T2,E2>&&>(other).m_storage
  );
  return (*this);
}

template <typename T, typename E>
template <typename U, typename>
inline EXPECTED_INLINE_VISIBILITY
auto expect::expected<T, E>::operator=(U&& value)
  noexcept(std::is_nothrow_assignable<T, U>::value)
  -> expected&
{
  m_storage.assign_value(detail::forward<U>(value));
  return (*this);
}

template <typename T, typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY
auto expect::expected<T, E>::operator=(const unexpected<E2>& other)
  noexcept(std::is_nothrow_assignable<E, const E2&>::value)
  -> expected&
{
  m_storage.assign_error(other.error());
  return (*this);
}

template <typename T, typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY
auto expect::expected<T, E>::operator=(unexpected<E2>&& other)
  noexcept(std::is_nothrow_assignable<E, E2&&>::value)
  -> expected&
{
  m_storage.assign_error(static_cast<E2&&>(other.error()));
  return (*this);
}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T, E>::operator->()
  noexcept -> value_type*
{
  // Prior to C++17, std::addressof was not `constexpr`.
  // Since `addressof` fixes a relatively obscure issue where users define a
  // custom `operator&`, the pre-C++17 implementation has been defined to be
  // `&(**this)` so that it may exist in constexpr contexts.
#if __cplusplus >= 201703L
  return &(**this);
#else
  return std::addressof(**this);
#endif
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T, E>::operator->()
  const noexcept -> const value_type*
{
#if __cplusplus >= 201703L
  return &(**this);
#else
  return std::addressof(**this);
#endif
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T, E>::operator*()
  & noexcept -> value_type&
{
  return static_cast<T&>(m_storage.m_value);
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T, E>::operator*()
  && noexcept -> value_type&&
{
  return static_cast<T&&>(static_cast<T&>(m_storage.m_value));
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T, E>::operator*()
  const& noexcept -> const value_type&
{
  return static_cast<const T&>(m_storage.m_value);
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T, E>::operator*()
  const&& noexcept -> const value_type&&
{
  return static_cast<const T&&>(static_cast<const T&>(m_storage.m_value));
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<T,E>::operator bool()
  const noexcept
{
  return m_storage.m_has_value;
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T,E>::has_value()
  const noexcept -> bool
{
  return m_storage.m_has_value;
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T,E>::has_error()
  const noexcept -> bool
{
  return !m_storage.m_has_value;
}

//-----------------------------------------------------------------------------

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T,E>::value()
  & -> value_type&
{
  return (has_value() || (detail::throw_bad_expected_access(), false),
    m_storage.m_value
  );
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T,E>::value()
  && -> value_type&&
{
  using underlying = typename std::remove_reference<T>::type;

  return (has_value() || (detail::throw_bad_expected_access(), true),
    static_cast<underlying&&>(static_cast<underlying&>(m_storage.m_value))
  );
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T,E>::value()
  const & -> const value_type&
{
  return (has_value() || (detail::throw_bad_expected_access(), true),
    m_storage.m_value
  );
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T,E>::value()
  const && -> const value_type&&
{
  using underlying = typename std::remove_reference<T>::type;

  return (has_value() || (detail::throw_bad_expected_access(), true),
    static_cast<const underlying&&>(static_cast<const underlying&>(m_storage.m_value))
  );
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T,E>::error() const &
  noexcept(std::is_nothrow_constructible<E>::value &&
           std::is_nothrow_copy_constructible<E>::value) -> E
{
  static_assert(
    std::is_default_constructible<E>::value,
    "E must be default-constructible if 'error()' checks are used. "
    "This is to allow for default-constructed error states to represent the "
    "'good' state"
  );

  return m_storage.m_has_value
    ? E()
    : m_storage.m_error;
}

template <typename T, typename E>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T,E>::error() &&
  noexcept(std::is_nothrow_constructible<E>::value &&
           std::is_nothrow_move_constructible<E>::value) -> E
{
  static_assert(
    std::is_default_constructible<E>::value,
    "E must be default-constructible if 'error()' checks are used. "
    "This is to allow for default-constructed error states to represent the "
    "'good' state"
  );

  return m_storage.m_has_value
    ? E()
    : static_cast<E&&>(m_storage.m_error);
}

//-----------------------------------------------------------------------------
// Monadic Functionalities
//-----------------------------------------------------------------------------

template <typename T, typename E>
template <typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T, E>::value_or(U&& default_value)
  const& -> value_type
{
  return m_storage.m_has_value
    ? m_storage.m_value
    : detail::forward<U>(default_value);
}

template <typename T, typename E>
template <typename U>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T, E>::value_or(U&& default_value)
  && -> value_type
{
  return m_storage.m_has_value
    ? static_cast<T&&>(**this)
    : detail::forward<U>(default_value);
}

template <typename T, typename E>
template <typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T, E>::error_or(U&& default_error)
  const& -> error_type
{
  return m_storage.m_has_value
    ? detail::forward<U>(default_error)
    : m_storage.m_error;
}

template <typename T, typename E>
template <typename U>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T, E>::error_or(U&& default_error)
  && -> error_type
{
  return m_storage.m_has_value
    ? detail::forward<U>(default_error)
    : static_cast<E&&>(m_storage.m_error);
}

template <typename T, typename E>
template <typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T, E>::and_then(U&& value)
  const -> expected<typename std::decay<U>::type,E>
{
  return map([&value](const T&){
    return detail::forward<U>(value);
  });
}

template <typename T, typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T, E>::flat_map(Fn&& fn)
  const & -> detail::invoke_result_t<Fn, const T&>
{
  using result_type = detail::invoke_result_t<Fn, const T&>;

  static_assert(
    is_expected<result_type>::value,
    "flat_map must return an expected type or the program is ill-formed"
  );

  return has_value()
    ? detail::invoke(detail::forward<Fn>(fn), **this)
    : result_type(in_place_error, m_storage.m_error);
}

template <typename T, typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T, E>::flat_map(Fn&& fn)
  && -> detail::invoke_result_t<Fn, const T&>
{
  using result_type = detail::invoke_result_t<Fn, const T&>;

  static_assert(
    is_expected<result_type>::value,
    "flat_map must return an expected type or the program is ill-formed"
  );

  return has_value()
    ? detail::invoke(detail::forward<Fn>(fn), **this)
    : result_type(in_place_error, static_cast<E&&>(m_storage.m_error));
}

template <typename T, typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T, E>::map(Fn&& fn)
  const & -> expected<detail::invoke_result_t<Fn,const T&>,E>
{
  using result_type = detail::invoke_result_t<Fn,const T&>;
  using expected_type = expected<result_type, E>;

  static_assert(
    !std::is_same<result_type,void>::value,
    "expected::map functions must return a value or the program is ill-formed"
  );

  return has_value()
    ? expected_type(detail::invoke(detail::forward<Fn>(fn), m_storage.m_value))
    : expected_type(in_place_error, m_storage.m_error);
}

template <typename T, typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T, E>::map(Fn&& fn)
  && -> expected<detail::invoke_result_t<Fn,const T&>,E>
{
  using result_type = detail::invoke_result_t<Fn,const T&>;
  using expected_type = expected<result_type, E>;

  static_assert(
    !std::is_same<result_type,void>::value,
    "expected::map functions must return a value or the program is ill-formed"
  );

  return has_value()
    ? expected_type(detail::invoke(detail::forward<Fn>(fn), **this))
    : expected_type(in_place_error, static_cast<E&&>(m_storage.m_error));
}

template <typename T, typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<T, E>::map_error(Fn&& fn)
  const & -> expected<T, detail::invoke_result_t<Fn,const E&>>
{
  using result_type = expected<T, detail::invoke_result_t<Fn, const E&>>;

  return has_error()
    ? result_type(in_place_error, detail::invoke(detail::forward<Fn>(fn), m_storage.m_error))
    : result_type(**this);
}

template <typename T, typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<T, E>::map_error(Fn&& fn)
  && -> expected<T, detail::invoke_result_t<Fn,const E&>>
{
  using result_type = expected<T, detail::invoke_result_t<Fn, const E&>>;

  return has_error()
    ? result_type(in_place_error, detail::invoke(detail::forward<Fn>(fn), m_storage.m_error))
    : result_type(static_cast<T&&>(**this));
}

//=============================================================================
// class : expected<void,E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructor / Assignment
//-----------------------------------------------------------------------------

template <typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<void, E>::expected()
  noexcept
  : m_storage(in_place)
{

}

template <typename E>
template <typename U, typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY
expect::expected<void, E>::expected(const expected<U,E2>& other)
  noexcept(std::is_nothrow_constructible<E,const E2&>::value)
  : m_storage(detail::unit{})
{
  m_storage.construct_error_from_expected(
    static_cast<const expected<U,E2>&>(other).m_storage
  );
}

template <typename E>
template <typename U, typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY
expect::expected<void, E>::expected(expected<U,E2>&& other)
  noexcept(std::is_nothrow_constructible<E,E2&&>::value)
  : m_storage(detail::unit{})
{
  m_storage.construct_error_from_expected(
    static_cast<expected<U,E2>&&>(other).m_storage
  );
}


//-----------------------------------------------------------------------------

template <typename E>
template <typename...Args, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<void, E>::expected(in_place_error_t, Args&&...args)
  noexcept(std::is_nothrow_constructible<E, Args...>::value)
  : m_storage(in_place_error, detail::forward<Args>(args)...)
{

}

template <typename E>
template <typename U, typename...Args, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<void, E>::expected(in_place_error_t,
                                    std::initializer_list<U> ilist,
                                    Args&&...args)
  noexcept(std::is_nothrow_constructible<E, std::initializer_list<U>, Args...>::value)
  : m_storage(in_place_error, ilist, detail::forward<Args>(args)...)
{

}

//-----------------------------------------------------------------------------

template <typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<void, E>::expected(const unexpected<E2>& e)
  noexcept(std::is_nothrow_constructible<E,const E2&>::value)
  : m_storage(in_place_error, e.error())
{

}

template <typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<void, E>::expected(unexpected<E2>&& e)
  noexcept(std::is_nothrow_constructible<E,E2&&>::value)
  : m_storage(in_place_error, static_cast<E2&&>(e.error()))
{

}

//-----------------------------------------------------------------------------

template <typename E>
template <typename T2, typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY
auto expect::expected<void, E>::operator=(const expected<T2,E2>& other)
  noexcept(std::is_nothrow_assignable<E, const E2&>::value)
  -> expected&
{
  m_storage.assign_error_from_expected(
    static_cast<const expected<T2,E2>&>(other).m_storage
  );
  return (*this);
}

template <typename E>
template <typename T2, typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY
auto expect::expected<void, E>::operator=(expected<T2,E2>&& other)
  noexcept(std::is_nothrow_assignable<E, E2&&>::value)
  -> expected&
{
  m_storage.assign_error_from_expected(
    static_cast<expected<T2,E2>&&>(other).m_storage
  );
  return (*this);
}

template <typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY
auto expect::expected<void, E>::operator=(const unexpected<E2>& other)
  noexcept(std::is_nothrow_assignable<E, const E2&>::value)
  -> expected&
{
  m_storage.assign_error(other.error());
  return (*this);
}

template <typename E>
template <typename E2, typename>
inline EXPECTED_INLINE_VISIBILITY
auto expect::expected<void, E>::operator=(unexpected<E2>&& other)
  noexcept(std::is_nothrow_assignable<E, E2&&>::value)
  -> expected&
{
  m_storage.assign_error(static_cast<E2&&>(other.error()));
  return (*this);
}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

template <typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
expect::expected<void, E>::operator bool()
  const noexcept
{
  return has_value();
}

template <typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<void, E>::has_value()
  const noexcept -> bool
{
  return m_storage.m_has_value;
}

template <typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<void, E>::has_error()
  const noexcept -> bool
{
  return !has_value();
}

//-------------------------------------------------------------------------

template <typename E>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<void, E>::value()
  const -> void
{
  static_cast<void>(has_value() || (detail::throw_bad_expected_access(), true));
}

template <typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<void, E>::error()
  const &
  noexcept(std::is_nothrow_constructible<E>::value &&
           std::is_nothrow_copy_constructible<E>::value) -> E
{
  return has_value() ? E{} : m_storage.m_error;
}

template <typename E>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<void, E>::error()
  && noexcept(std::is_nothrow_constructible<E>::value &&
              std::is_nothrow_copy_constructible<E>::value) -> E
{
  return has_value() ? E{} : static_cast<E&&>(m_storage.m_error);
}

//-----------------------------------------------------------------------------
// Monadic Functionalities
//-----------------------------------------------------------------------------

template <typename E>
template <typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<void, E>::error_or(U&& default_error)
  const & -> error_type
{
  return has_value()
    ? detail::forward<U>(default_error)
    : m_storage.m_error;
}

template <typename E>
template <typename U>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<void, E>::error_or(U&& default_error)
  && -> error_type
{
  return has_value()
    ? detail::forward<U>(default_error)
    : static_cast<E&&>(m_storage.m_error);
}

template <typename E>
template <typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<void, E>::and_then(U&& value)
  const -> expected<typename std::decay<U>::type,E>
{
  return map([&value]{
    return detail::forward<U>(value);
  });
}

template <typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<void, E>::flat_map(Fn&& fn)
  const & -> detail::invoke_result_t<Fn>
{
  using result_type = detail::invoke_result_t<Fn>;

  static_assert(
    is_expected<result_type>::value,
    "flat_map must return an expected type or the program is ill-formed"
  );

  return has_value()
    ? detail::invoke(detail::forward<Fn>(fn))
    : result_type(in_place_error, m_storage.m_error);
}

template <typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<void, E>::flat_map(Fn&& fn)
  && -> detail::invoke_result_t<Fn>
{
  using result_type = detail::invoke_result_t<Fn>;

  static_assert(
    is_expected<result_type>::value,
    "flat_map must return an expected type or the program is ill-formed"
  );

  return has_value()
    ? detail::invoke(detail::forward<Fn>(fn))
    : result_type(in_place_error, static_cast<E&&>(m_storage.m_error));
}

template <typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<void, E>::map(Fn&& fn)
  const & -> expected<detail::invoke_result_t<Fn>,E>
{
  using result_type = detail::invoke_result_t<Fn>;
  using expected_type = expected<result_type, E>;

  static_assert(
    !std::is_same<result_type,void>::value,
    "expected::map functions must return a value or the program is ill-formed"
  );

  return has_value()
    ? expected_type(in_place, detail::invoke(detail::forward<Fn>(fn)))
    : expected_type(in_place_error, m_storage.m_error);
}

template <typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY EXPECTED_CPP14_CONSTEXPR
auto expect::expected<void, E>::map(Fn&& fn)
  && -> expected<detail::invoke_result_t<Fn>,E>
{
  using result_type = detail::invoke_result_t<Fn>;
  using expected_type = expected<result_type, E>;

  static_assert(
    !std::is_same<result_type,void>::value,
    "expected::map functions must return a value or the program is ill-formed"
  );

  return has_value()
    ? expected_type(in_place, detail::invoke(detail::forward<Fn>(fn)))
    : expected_type(in_place_error, static_cast<E&&>(m_storage.m_error));
}

template <typename E>
template <typename Fn>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::expected<void, E>::map_error(Fn&& fn)
  const -> expected<void, detail::invoke_result_t<Fn,const E&>>
{
  using result_type = expected<void, detail::invoke_result_t<Fn, const E&>>;

  return has_error()
    ? result_type(in_place_error, detail::invoke(detail::forward<Fn>(fn), m_storage.m_error))
    : result_type{};
}

//=============================================================================
// non-member functions : class : expected
//=============================================================================

//-----------------------------------------------------------------------------
// Comparison
//-----------------------------------------------------------------------------

template <typename T1, typename E1, typename T2, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator==(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
  noexcept -> bool
{
  return (lhs.has_value() == rhs.has_value())
    ? (
        lhs.has_value()
        ? *lhs == *rhs
        : detail::extract_error(lhs) == detail::extract_error(rhs)
      )
    : false;
}

template <typename T1, typename E1, typename T2, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator!=(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
  noexcept -> bool
{
  return (lhs.has_value() == rhs.has_value())
    ? (
        lhs.has_value()
        ? *lhs != *rhs
        : detail::extract_error(lhs) != detail::extract_error(rhs)
      )
    : true;
}

template <typename T1, typename E1, typename T2, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>=(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
  noexcept -> bool
{
  return (lhs.has_value() == rhs.has_value())
    ? (
        lhs.has_value()
        ? *lhs >= *rhs
        : detail::extract_error(lhs) >= detail::extract_error(rhs)
      )
    : static_cast<int>(static_cast<bool>(lhs)) >= static_cast<int>(static_cast<bool>(rhs));
}

template <typename T1, typename E1, typename T2, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<=(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
  noexcept -> bool
{
  return (lhs.has_value() == rhs.has_value())
    ? (
        lhs.has_value()
        ? *lhs <= *rhs
        : detail::extract_error(lhs) <= detail::extract_error(rhs)
      )
    : static_cast<int>(static_cast<bool>(lhs)) <= static_cast<int>(static_cast<bool>(rhs));
}

template <typename T1, typename E1, typename T2, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
  noexcept -> bool
{
  return (lhs.has_value() == rhs.has_value())
    ? (
        lhs.has_value()
        ? *lhs > *rhs
        : detail::extract_error(lhs) > detail::extract_error(rhs)
      )
    : static_cast<int>(static_cast<bool>(lhs)) > static_cast<int>(static_cast<bool>(rhs));
}

template <typename T1, typename E1, typename T2, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<(const expected<T1,E1>& lhs, const expected<T2,E2>& rhs)
  noexcept -> bool
{
  return (lhs.has_value() == rhs.has_value())
    ? (
        lhs.has_value()
        ? *lhs < *rhs
        : detail::extract_error(lhs) < detail::extract_error(rhs)
      )
    : static_cast<int>(static_cast<bool>(lhs)) < static_cast<int>(static_cast<bool>(rhs));
}


//-----------------------------------------------------------------------------

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator==(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
  noexcept -> bool
{
  return lhs.has_value() == rhs.has_value()
    ? (
        lhs.has_value()
        ? true
        : detail::extract_error(lhs) == detail::extract_error(rhs)
      )
    : false;
}

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator!=(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
  noexcept -> bool
{
  return lhs.has_value() == rhs.has_value()
    ? (
        lhs.has_value()
        ? false
        : detail::extract_error(lhs) != detail::extract_error(rhs)
      )
    : true;
}

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>=(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
  noexcept -> bool
{
  return lhs.has_value() == rhs.has_value()
    ? (
        lhs.has_value()
        ? true
        : detail::extract_error(lhs) >= detail::extract_error(rhs)
      )
    : static_cast<int>(static_cast<bool>(lhs)) >= static_cast<int>(static_cast<bool>(rhs));
}

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<=(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
  noexcept -> bool
{
  return lhs.has_value() == rhs.has_value()
    ? (
        lhs.has_value()
        ? true
        : detail::extract_error(lhs) <= detail::extract_error(rhs)
      )
    : static_cast<int>(static_cast<bool>(lhs)) <= static_cast<int>(static_cast<bool>(rhs));
}

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
  noexcept -> bool
{
  return lhs.has_value() == rhs.has_value()
    ? (
        lhs.has_value()
        ? false
        : detail::extract_error(lhs) > detail::extract_error(rhs)
      )
    : static_cast<int>(static_cast<bool>(lhs)) > static_cast<int>(static_cast<bool>(rhs));
}

template <typename E1, typename E2>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<(const expected<void,E1>& lhs, const expected<void,E2>& rhs)
  noexcept -> bool
{
  return lhs.has_value() == rhs.has_value()
    ? (
        lhs.has_value()
        ? false
        : detail::extract_error(lhs) < detail::extract_error(rhs)
      )
    : static_cast<int>(static_cast<bool>(lhs)) < static_cast<int>(static_cast<bool>(rhs));
}


//-----------------------------------------------------------------------------

template <typename T, typename E, typename U, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator==(const expected<T,E>& exp, const U& value)
  noexcept -> bool
{
  return (exp.has_value() && *exp == value);
}

template <typename T, typename U, typename E, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator==(const T& value, const expected<U,E>& exp)
  noexcept -> bool
{
  return (exp.has_value() && *exp == value);
}

template <typename T, typename E, typename U, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator!=(const expected<T,E>& exp, const U& value)
  noexcept -> bool
{
  return exp.has_value() ? *exp != value : true;
}

template <typename T, typename U, typename E, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator!=(const T& value, const expected<U,E>& exp)
  noexcept -> bool
{
  return exp.has_value() ? value != *exp : true;
}

template <typename T, typename E, typename U, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<=(const expected<T,E>& exp, const U& value)
  noexcept -> bool
{
  return exp.has_value() ? *exp <= value : false;
}

template <typename T, typename U, typename E, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<=(const T& value, const expected<U,E>& exp)
  noexcept -> bool
{
  return exp.has_value() ? value <= *exp : true;
}

template <typename T, typename E, typename U, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>=(const expected<T,E>& exp, const U& value)
  noexcept -> bool
{
  return exp.has_value() ? *exp >= value : true;
}

template <typename T, typename U, typename E, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>=(const T& value, const expected<U,E>& exp)
  noexcept -> bool
{
  return exp.has_value() ? value >= *exp : false;
}

template <typename T, typename E, typename U, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<(const expected<T,E>& exp, const U& value)
  noexcept -> bool
{
  return exp.has_value() ? *exp < value : false;
}

template <typename T, typename U, typename E, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<(const T& value, const expected<U,E>& exp)
  noexcept -> bool
{
  return exp.has_value() ? value < *exp : true;
}

template <typename T, typename E, typename U, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>(const expected<T,E>& exp, const U& value)
  noexcept -> bool
{
  return exp.has_value() ? *exp > value : false;
}

template <typename T, typename U, typename E, typename>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>(const T& value, const expected<U,E>& exp)
  noexcept -> bool
{
  return exp.has_value() ? value > *exp : true;
}

//-----------------------------------------------------------------------------

template <typename T, typename E, typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator==(const expected<T,E>& exp, const unexpected<U>& error)
  noexcept -> bool
{
  return exp.has_error() ? detail::extract_error(exp) == error.error() : false;
}

template <typename T, typename U, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator==(const unexpected<T>& error, const expected<E,U>& exp)
  noexcept -> bool
{
  return exp.has_error() ? error.error() == detail::extract_error(exp) : false;
}

template <typename T, typename E, typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator!=(const expected<T,E>& exp, const unexpected<U>& error)
  noexcept -> bool
{
  return exp.has_error() ? detail::extract_error(exp) != error.error() : true;
}

template <typename T, typename U, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator!=(const unexpected<T>& error, const expected<E,U>& exp)
  noexcept -> bool
{
  return exp.has_error() ? error.error() != detail::extract_error(exp) : true;
}

template <typename T, typename E, typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<=(const expected<T,E>& exp, const unexpected<U>& error)
  noexcept -> bool
{
  return exp.has_error() ? detail::extract_error(exp) <= error.error() : true;
}

template <typename T, typename U, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<=(const unexpected<T>& error, const expected<E,U>& exp)
  noexcept -> bool
{
  return exp.has_error() ? error.error() <= detail::extract_error(exp) : false;
}

template <typename T, typename E, typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>=(const expected<T,E>& exp, const unexpected<U>& error)
  noexcept -> bool
{
  return exp.has_error() ? detail::extract_error(exp) >= error.error() : false;
}

template <typename T, typename U, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>=(const unexpected<T>& error, const expected<E,U>& exp)
  noexcept -> bool
{
  return exp.has_error() ? error.error() >= detail::extract_error(exp) : true;
}

template <typename T, typename E, typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<(const expected<T,E>& exp, const unexpected<U>& error)
  noexcept -> bool
{
  return exp.has_error() ? detail::extract_error(exp) < error.error() : true;
}

template <typename T, typename U, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator<(const unexpected<T>& error, const expected<E,U>& exp)
  noexcept -> bool
{
  return exp.has_error() ? error.error() < detail::extract_error(exp) : false;
}

template <typename T, typename E, typename U>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>(const expected<T,E>& exp, const unexpected<U>& error)
  noexcept -> bool
{
  return exp.has_error() ? detail::extract_error(exp) > error.error() : false;
}

template <typename T, typename U, typename E>
inline EXPECTED_INLINE_VISIBILITY constexpr
auto expect::operator>(const unexpected<T>& error, const expected<E,U>& exp)
  noexcept -> bool
{
  return exp.has_error() ? error.error() > detail::extract_error(exp) : true;
}

#endif /* MSL_EXPECTED_HPP */
