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

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <initializer_list> // std::initializer_list
#include <stdexcept>        // std::logic_error
#include <type_traits>  // std::is_constructible
#include <system_error> // std::error_condition

namespace expect {

  enum class tribool {
    e_false,
    e_true,
    e_indeterminate,
  };

  /// \brief This function is a special disambiguation tag for variadic
  ///        functions, used in any and optional
  ///
  /// \note Calling this function results in undefined behaviour.
  struct in_place_t
  {
    explicit in_place_t() = default;
  };
  constexpr in_place_t in_place{};

  /// \brief This function is a special disambiguation tag for variadic
  ///        functions, used in any and optional
  ///
  /// \note Calling this function results in undefined behaviour.
  template<typename T>
  struct in_place_type_t
  {
      explicit in_place_type_t() = default;
  };
  template<typename T>
  constexpr in_place_type_t<T> in_place_type{};


  /// \brief This function is a special disambiguation tag for variadic
  ///        functions, used in any and optional
  ///
  /// \note Calling this function results in undefined behaviour.
  template<std::size_t I> struct in_place_index_t
  {
    explicit in_place_index_t() = default;
  };

  template<std::size_t I>
  constexpr in_place_index_t<I> in_place_index{};

  //-------------------------------------------------------------------------

  /// \brief Type-trait to determine if the type is an in_place type
  ///
  /// The result is aliased as \c ::value
  template<typename T>
  struct is_in_place : std::false_type{};

  template<>
  struct is_in_place<in_place_t> : std::true_type{};

  template<typename T>
  struct is_in_place<in_place_type_t<T>> : std::true_type{};

  template<std::size_t I>
  struct is_in_place<in_place_index_t<I>> : std::true_type{};

  template<typename T>
  constexpr bool is_in_place_v = is_in_place<T>::value;

  //--------------------------------------------------------------------------

  template<typename T>
  struct is_reference_wrapper : std::false_type {};

  template<typename U>
  struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type {};

  template<typename T>
  constexpr bool is_reference_wrapper_v = is_reference_wrapper<T>::value;

  template<typename Base, typename T, typename Derived, typename... Args>
  inline constexpr auto invoke_impl(T Base::*pmf, Derived&& ref, Args&&... args)
    noexcept(noexcept((std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value &&
                        std::is_base_of<Base, std::decay_t<Derived>>::value,
        decltype((std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...))>
  {
    return (std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...);
  }

  template<typename Base, typename T, typename RefWrap, typename... Args>
  inline constexpr auto invoke_impl(T Base::*pmf, RefWrap&& ref, Args&&... args)
    noexcept(noexcept((ref.get().*pmf)(std::forward<Args>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value &&
                        is_reference_wrapper<std::decay_t<RefWrap>>::value,
        decltype((ref.get().*pmf)(std::forward<Args>(args)...))>

  {
    return (ref.get().*pmf)(std::forward<Args>(args)...);
  }

  template<typename Base, typename T, typename Pointer, typename... Args>
  inline constexpr auto invoke_impl(T Base::*pmf, Pointer&& ptr, Args&&... args)
    noexcept(noexcept(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...)))
    -> std::enable_if_t<std::is_function<T>::value &&
                        !is_reference_wrapper<std::decay_t<Pointer>>::value &&
                        !std::is_base_of<Base, std::decay_t<Pointer>>::value,
        decltype(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...))>
  {
    return ((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...);
  }

  template<typename Base, typename T, typename Derived>
  inline constexpr auto invoke_impl(T Base::*pmd, Derived&& ref)
    noexcept(noexcept(std::forward<Derived>(ref).*pmd))
    -> std::enable_if_t<!std::is_function<T>::value &&
                        std::is_base_of<Base, std::decay_t<Derived>>::value,
        decltype(std::forward<Derived>(ref).*pmd)>
  {
    return std::forward<Derived>(ref).*pmd;
  }

  template<typename Base, typename T, typename RefWrap>
  inline constexpr auto invoke_impl(T Base::*pmd, RefWrap&& ref)
    noexcept(noexcept(ref.get().*pmd))
    -> std::enable_if_t<!std::is_function<T>::value &&
                        is_reference_wrapper<std::decay_t<RefWrap>>::value,
        decltype(ref.get().*pmd)>
  {
    return ref.get().*pmd;
  }

  template<typename Base, typename T, typename Pointer>
  inline constexpr auto invoke_impl(T Base::*pmd, Pointer&& ptr)
    noexcept(noexcept((*std::forward<Pointer>(ptr)).*pmd))
    -> std::enable_if_t<!std::is_function<T>::value &&
                        !is_reference_wrapper<std::decay_t<Pointer>>::value &&
                        !std::is_base_of<Base, std::decay_t<Pointer>>::value,
        decltype((*std::forward<Pointer>(ptr)).*pmd)>
  {
    return (*std::forward<Pointer>(ptr)).*pmd;
  }

  template<typename F, typename... Args>
  inline constexpr auto invoke_impl(F&& f, Args&&... args)
      noexcept(noexcept(std::forward<F>(f)(std::forward<Args>(args)...)))
    -> std::enable_if_t<!std::is_member_pointer<std::decay_t<F>>::value,
      decltype(std::forward<F>(f)(std::forward<Args>(args)...))>
  {
    return std::forward<F>(f)(std::forward<Args>(args)...);
  }

  //=======================================================================
  // is_invocable
  //=======================================================================

  template<typename Fn, typename...Args>
  struct is_invocable
  {
    template<typename Fn2, typename...Args2>
    static auto test( Fn2&&, Args2&&... )
      -> decltype(invoke_impl(std::declval<Fn2>(), std::declval<Args2>()...), std::true_type{});

    static auto test(...)
      -> std::false_type;

    static constexpr bool value =
      decltype(test(std::declval<Fn>(), std::declval<Args>()...))::value;
  };

  //=======================================================================
  // is_nothrow_invocable
  //=======================================================================

  template<typename Fn, typename...Args>
  struct is_nothrow_invocable
  {
    template<typename Fn2, typename...Args2>
    static auto test( Fn2&&, Args2&&... )
      -> decltype(invoke_impl(std::declval<Fn2>(), std::declval<Args2>()...),
                  std::integral_constant<bool,noexcept(invoke_impl(std::declval<Fn2>(), std::declval<Args2>()...))>{});

    static auto test(...)
      -> std::false_type;

    static constexpr bool value =
      decltype(test(std::declval<Fn>(), std::declval<Args>()...))::value;
  };

} // namespace detail

/// \brief Invoke the Callable object \p function with the parameters \p args.
///
/// As by \c INVOKE(std::forward<F>(f), std::forward<Args>(args)...)
///
/// \param function Callable object to be invoked
/// \param args     arguments to pass to \p function
template<typename Func, typename... Args>
constexpr auto invoke(Func&& function, Args&&... args)
#ifndef _MSC_VER
  noexcept( noexcept( detail::invoke_impl(std::forward<Func>(function), std::forward<Args>(args)...) ) )
#endif
  -> decltype( detail::invoke_impl(std::forward<Func>(function), std::forward<Args>(args)...) );


template<typename F, typename...Types>
struct invoke_result : identity<decltype(invoke( std::declval<F>(), std::declval<Types>()... ))>{};

template<typename F, typename...Types>
using invoke_result_t = typename invoke_result<F,Types...>::type;


/// \brief Type trait to determine whether \p Fn is invokable with
///        \p Args...
///
/// Formally, the expression:
/// \code
/// INVOKE( std::declval<Fn>(), std::declval<Args>()... )
/// \endcode
/// is well formed
///
/// The result is aliased as \c ::value
template<typename Fn, typename...Args>
using is_invocable = detail::is_invocable<Fn,Args...>;

/// \brief Type-trait helper to retrieve the \c ::value of is_invokable
template<typename Fn, typename...Args>
constexpr bool is_invocable_v = is_invocable<Fn,Args...>::value;


//    template<typename R, typename Fn, typename... Types>
//    using is_invocable_r = is_detected_convertible<R,invoke_result_t,Fn,Types...>;
//
//    template<typename R, typename Fn, typename... Types>
//    constexpr bool is_invocable_r_v = is_invocable_r<R,Fn,Types...>::value;

//------------------------------------------------------------------------

/// \brief Type trait to determine whether \p Fn is nothrow invokable with
///        \p Args...
///
/// Formally, the expression:
/// \code
/// INVOKE( std::declval<Fn>(), std::declval<Args>()... )
/// \endcode
/// is well formed and is known not to throw
///
/// The result is aliased as \c ::value
template<typename Fn, typename...Args>
using is_nothrow_invocable = detail::is_nothrow_invocable<Fn,Args...>;

/// \brief Type-trait helper to retrieve the \c ::value of is_nothrow_invokable
template<typename Fn, typename...Args>
constexpr bool is_nothrow_invocable_v = is_nothrow_invocable<Fn,Args...>::value;

  /// \brief Similar to enable_if, but doesn't sfinae-away a type; instead
  ///        produces an uninstantiable unique type when true
  ///
  /// This is used to selectively disable constructors, since sfinae doesn't
  /// work for copy/move constructors
  template<bool b, typename T>
  struct enable_overload_if : identity<T>{};

  template<typename T>
  struct enable_overload_if<false,T>
  {
    class type{ type() = delete; ~type() = delete; };
  };

  /// \brief Convenience alias to retrieve the \c ::type member of
  ///        \c block_if
  template<bool B, typename T>
  using enable_overload_if_t = typename enable_overload_if<B,T>::type;

  //---------------------------------------------------------------------------

  /// \brief Inverse of \c block_if
  template<bool B, typename T>
  using disable_overload_if = enable_overload_if<!B,T>;

  /// \brief Convenience alias to retrieve the \c ::type member of
  ///        \c block_unless
  template<bool B, typename T>
  using disable_overload_if_t = typename disable_overload_if<B,T>::type;

  template <typename> struct empty{};

  /// \brief Type trait to determine the bool_constant from a logical
  ///        AND operation of other bool_constants
  ///
  /// The result is aliased as \c ::value
  template<typename...>
  struct conjunction;

  template<typename B1>
  struct conjunction<B1> : B1{};

  template<typename B1, typename... Bn>
  struct conjunction<B1, Bn...>
    : std::conditional_t<B1::value, conjunction<Bn...>, B1>{};

  template<typename T, typename E>
  class expected;

  //=========================================================================
  // trait : is_expected
  //=========================================================================

  template<typename T>
  struct is_expected : std::false_type{};

  template<typename T, typename E>
  struct is_expected<expected<T,E>> : std::true_type{};

  template<typename T>
  constexpr bool is_expected_v = is_expected<T>::value;

  //=========================================================================
  // class : bad_expected_access
  //=========================================================================

  ///////////////////////////////////////////////////////////////////////////
  /// \brief The exception type thrown when accessing a non-active expected
  ///        member
  ///
  /// \tparam E the error type
  ///////////////////////////////////////////////////////////////////////////
  template<typename E>
  class bad_expected_access : public std::logic_error
  {
    //-----------------------------------------------------------------------
    // Public Member Types
    //-----------------------------------------------------------------------
  public:

    using error_type = E;

    //-----------------------------------------------------------------------
    // Constructors
    //-----------------------------------------------------------------------
  public:

    /// \brief Constructs a bad_expected_access from a given error, \p e
    ///
    /// \param e the error
    explicit bad_expected_access( error_type e );

    //-----------------------------------------------------------------------
    // Observers
    //-----------------------------------------------------------------------
  public:

    /// \{
    /// \brief Gets the underlying error
    ///
    /// \return the error type
    error_type& error() & noexcept;
    error_type&& error() && noexcept;
    const error_type& error() const & noexcept;
    const error_type&& error() const && noexcept;
    /// \}

    //-----------------------------------------------------------------------
    // Private Members
    //-----------------------------------------------------------------------
  private:

    error_type m_error_value;
  };

  //=========================================================================

  template<>
  class bad_expected_access<void> : std::logic_error
  {
    //-----------------------------------------------------------------------
    // Constructors
    //-----------------------------------------------------------------------
  public:

    /// \brief Constructs a bad_expected_access
    bad_expected_access();

  };

  //=========================================================================
  // class : unexpected_type
  //=========================================================================

  ///////////////////////////////////////////////////////////////////////////
  /// \brief A type representing an unexpected value coming from an expected
  ///        type
  ///
  /// \tparam E the unexpected type
  ///////////////////////////////////////////////////////////////////////////
  template<typename E>
  class unexpected_type
  {
    //-----------------------------------------------------------------------
    // Constructors
    //-----------------------------------------------------------------------
  public:

    // Deleted default constructor
    unexpected_type() = delete;

    /// \brief Moves an unexpected_type from an existing one
    ///
    /// \param other the other unexpected_type to move
    constexpr unexpected_type( unexpected_type&& other ) = default;

    /// \brief Copies an unexpected_type from an existing one
    ///
    /// \param other the other unexpected_type to copy
    constexpr unexpected_type( const unexpected_type& other ) = default;

    /// \brief Move-converts an expected_type to this type
    ///
    /// \note This constructor only participates in overload resolution if E2
    ///       is convertible to E
    ///
    /// \param other the other unexpected_type to move-convert
    template<typename E2, typename = std::enable_if_t<std::is_convertible<E2,E>::value>>
    constexpr unexpected_type( unexpected_type<E2>&& other );

    /// \brief Copy converts an expected_Type to this type
    ///
    /// \note This constructor only participates in overload resolution if E2
    ///       is convertible to E
    ///
    /// \param other the other unexpected_type to copy-convert
    template<typename E2, typename = std::enable_if_t<std::is_convertible<E2,E>::value>>
    constexpr unexpected_type( const unexpected_type<E2>& other );

    /// \brief Constructs the expected_type by copying the underlying
    ///        unexpected_type by value
    ///
    /// \param value the value to copy
    constexpr explicit unexpected_type( const E& value );

    /// \brief Constructs the expected_type by copying the underlying
    ///        unexpected_type by value
    ///
    /// \param value the value to copy
    constexpr explicit unexpected_type( E&& value );

    /// \brief Constructs the underlying unexpected_type by forwarding \p args
    ///        to \p E's constructor
    ///
    /// \throws ... any exception that E's constructor may throw
    /// \param args the arguments to forward
    template<typename...Args, typename = std::enable_if_t<std::is_constructible<E,Args...>::value>>
    constexpr explicit unexpected_type( in_place_t, Args&&...args );

    /// \brief Constructs the underlying unexpected_type by forwarding \p args
    ///        to \p E's constructor
    ///
    /// \throws ... any exception that E's constructor may throw
    /// \param ilist an initializer list of arguments
    /// \param args the arguments to forward
    template<typename U, typename...Args, typename = std::enable_if_t<std::is_constructible<E,std::initializer_list<U>,Args...>::value>>
    constexpr explicit unexpected_type( in_place_t,
                                        std::initializer_list<U> ilist,
                                        Args&&...args );

    //-----------------------------------------------------------------------
    // Observers
    //-----------------------------------------------------------------------
  public:

    /// \{
    /// \brief Extracts the value for this unexpected_type
    ///
    /// \return reference to the value
    constexpr E& value() &;
    constexpr const E& value() const &;
    constexpr E&& value() &&;
    constexpr const E&& value() const &&;
    /// \}

    //-----------------------------------------------------------------------
    // Private Members
    //-----------------------------------------------------------------------
  private:

    E m_value;
  };

  //=========================================================================
  // trait : is_unexpected
  //=========================================================================

  template<typename T>
  struct is_unexpected_type : std::false_type{};

  template<typename T>
  struct is_unexpected_type<unexpected_type<T>> : std::true_type{};

  template<typename T>
  constexpr bool is_unexpected_type_v = is_unexpected_type<T>::value;

  //=========================================================================
  // utilities : class unexpected
  //=========================================================================

  /// \brief Makes an unexpected type \c E
  ///
  /// \tparam E the unexpected type to make
  /// \param args the arguments to forward
  /// \return the unexpected type
  template<typename E, typename...Args>
  constexpr unexpected_type<E> make_unexpected( Args&&...args );

  //=========================================================================
  // relational operators : class unexpected_type
  //=========================================================================

  template<typename E>
  constexpr bool operator==( const unexpected_type<E>& lhs,
                              const unexpected_type<E>& rhs );
  template<typename E>
  constexpr bool operator!=( const unexpected_type<E>& lhs,
                              const unexpected_type<E>& rhs );
  template<typename E>
  constexpr bool operator<( const unexpected_type<E>& lhs,
                            const unexpected_type<E>& rhs );
  template<typename E>
  constexpr bool operator>( const unexpected_type<E>& lhs,
                            const unexpected_type<E>& rhs );
  template<typename E>
  constexpr bool operator<=( const unexpected_type<E>& lhs,
                              const unexpected_type<E>& rhs );
  template<typename E>
  constexpr bool operator>=( const unexpected_type<E>& lhs,
                              const unexpected_type<E>& rhs );

  //=========================================================================
  // class : unexpect tag
  //=========================================================================

  /// \brief A tag type used for tag-dispatch
  struct unexpect_t
  {
    unexpect_t() = delete;
    constexpr unexpect_t(int){}
  };

  //-------------------------------------------------------------------------

  constexpr unexpect_t unexpect{0};

  //=========================================================================
  // X.Z.4, expected for object types
  //=========================================================================

  namespace detail {

    //=======================================================================
    // expected_base
    //=======================================================================

    template<bool Trivial, typename T, typename E>
    class expected_base;

    template<typename T, typename E>
    class expected_base<true,T,E>
    {
      //-----------------------------------------------------------------------
      // Constructors
      //-----------------------------------------------------------------------
    public:

      constexpr expected_base();
      template<typename...Args>
      constexpr expected_base( in_place_t, Args&&...args );
      template<typename...Args>
      constexpr expected_base( unexpect_t, Args&&...args );

      //---------------------------------------------------------------------
      // Observers
      //---------------------------------------------------------------------
    protected:

      constexpr bool has_value() const noexcept;
      constexpr bool has_error() const noexcept;
      constexpr bool valueless_by_exception() const noexcept;

      //---------------------------------------------------------------------

      constexpr T& get_value() & noexcept;
      constexpr T&& get_value() && noexcept;
      constexpr const T& get_value() const & noexcept;
      constexpr const T&& get_value() const && noexcept;

      //---------------------------------------------------------------------

      constexpr unexpected_type<E>& get_unexpected() & noexcept;
      constexpr unexpected_type<E>&& get_unexpected() && noexcept;
      constexpr const unexpected_type<E>& get_unexpected() const & noexcept;
      constexpr const unexpected_type<E>&& get_unexpected() const && noexcept;

      //---------------------------------------------------------------------
      // Modifiers
      //---------------------------------------------------------------------
    protected:

      template<typename...Args>
      void emplace_value( Args&&...args );
      template<typename...Args>
      void emplace_error( Args&&...args );

      //---------------------------------------------------------------------

      template<typename U>
      void assign_value( U&& value );
      template<typename U>
      void assign_error( U&& error );

      //---------------------------------------------------------------------
      // Protected Member Types
      //---------------------------------------------------------------------
    protected:

      union storage_type
      {
        constexpr storage_type()
          : dummy{}
        {

        }

        template<typename...Args>
        constexpr storage_type( in_place_t, Args&&...args )
          : value( std::forward<Args>(args)... )
        {

        }

        template<typename...Args>
        constexpr storage_type( unexpect_t, Args&&...args )
          : error( std::forward<Args>(args)... )
        {

        }

        empty<void>        dummy;
        T                  value;
        unexpected_type<E> error;
      };

      //---------------------------------------------------------------------
      // Private Members
      //---------------------------------------------------------------------
    private:

      storage_type m_storage;
      tribool      m_has_value;
    };

    //=======================================================================

    template<typename T, typename E>
    class expected_base<false,T,E>
    {
      //---------------------------------------------------------------------
      // Constructors / Destructor
      //---------------------------------------------------------------------
    public:

      constexpr expected_base();
      template<typename...Args>
      constexpr expected_base( in_place_t, Args&&...args );
      template<typename...Args>
      constexpr expected_base( unexpect_t, Args&&...args );

      //---------------------------------------------------------------------

      ~expected_base();

      //---------------------------------------------------------------------
      // Observers
      //---------------------------------------------------------------------
    protected:

      constexpr bool has_value() const noexcept;
      constexpr bool has_error() const noexcept;
      constexpr bool valueless_by_exception() const noexcept;

      //---------------------------------------------------------------------

      constexpr T& get_value() & noexcept;
      constexpr T&& get_value() && noexcept;
      constexpr const T& get_value() const & noexcept;
      constexpr const T&& get_value() const && noexcept;

      //---------------------------------------------------------------------

      constexpr unexpected_type<E>& get_unexpected() & noexcept;
      constexpr unexpected_type<E>&& get_unexpected() && noexcept;
      constexpr const unexpected_type<E>& get_unexpected() const & noexcept;
      constexpr const unexpected_type<E>&& get_unexpected() const && noexcept;

      //---------------------------------------------------------------------
      // Modifiers
      //---------------------------------------------------------------------
    protected:

      void destruct();

      //---------------------------------------------------------------------

      template<typename...Args>
      void emplace_value( Args&&...args );

      template<typename...Args>
      void emplace_error( Args&&...args );

      //---------------------------------------------------------------------

      template<typename U>
      void assign_value( U&& value );

      template<typename U>
      void assign_error( U&& error );

      //---------------------------------------------------------------------
      // Protected Member Types
      //---------------------------------------------------------------------
    protected:

      union storage_type
      {
        constexpr storage_type()
          : dummy{}
        {

        }

        template<typename...Args>
        constexpr storage_type( in_place_t, Args&&...args )
          : value( std::forward<Args>(args)... )
        {

        }

        template<typename...Args>
        constexpr storage_type( unexpect_t, Args&&...args )
          : error( std::forward<Args>(args)... )
        {

        }

        ~storage_type(){}

        empty<void>        dummy;
        T                  value;
        unexpected_type<E> error;
      };

      //---------------------------------------------------------------------
      // Private Members
      //---------------------------------------------------------------------
    private:

      storage_type m_storage;
      tribool      m_has_value;
    };
  } // namespace detail

  ///////////////////////////////////////////////////////////////////////////
  /// \brief
  ///
  /// \tparam T the type
  /// \tparam Error the error type
  ///////////////////////////////////////////////////////////////////////////
  template<typename T, typename E=std::error_condition>
  class expected
    : detail::expected_base<std::is_trivially_destructible<T>::value &&
                            std::is_trivially_destructible<E>::value,T,E>
  {
    using base_type = detail::expected_base<std::is_trivially_destructible<T>::value &&
                                            std::is_trivially_destructible<E>::value,T,E>;

    template<typename U, typename G>
    static constexpr bool is_constructible = std::is_constructible<T, U&>::value &&
                                              std::is_constructible<E, G&>::value;


    template<typename U, typename G>
    static constexpr bool is_copyable = std::is_copy_constructible<U>::value &&
                                        std::is_copy_assignable<U>::value &&
                                        std::is_copy_constructible<G>::value &&
                                        std::is_copy_assignable<G>::value;
    template<typename U, typename G>
    static constexpr bool is_moveable = std::is_move_constructible<U>::value &&
                                        std::is_move_assignable<U>::value &&
                                        std::is_move_constructible<G>::value &&
                                        std::is_move_assignable<G>::value;

    template<typename U, typename V>
    static constexpr bool is_value_assignable = std::is_constructible<U,V>::value &&
                                                std::is_assignable<U,V>::value;

    //-----------------------------------------------------------------------
    // Public Member Types
    //-----------------------------------------------------------------------
  public:

    using value_type = T;
    using error_type = E;

    template<typename U>
    struct rebind
    {
      using type = expected<U, error_type>;
    };

    //-----------------------------------------------------------------------
    // Constructors / Destructor / Assignment
    //-----------------------------------------------------------------------
  public:

    // X.Z.4.1, constructors

    /// \brief Initializes the contained value as if direct-non-list-
    ///        initializing an object of type T with the expression T{}
    ///
    /// \post \code bool(*this) \endcode
    template<typename U=T,typename = std::enable_if_t<std::is_default_constructible<T>::value>>
    constexpr expected();

    //-----------------------------------------------------------------------

    /// \brief Copy-constructs an expected from an existing expected
    ///
    /// \param other the other expected
    expected( enable_overload_if<std::is_copy_constructible<T>::value &&
                                  std::is_copy_constructible<E>::value,
                                  const expected&> other );
    expected( disable_overload_if<std::is_copy_constructible<T>::value &&
                                  std::is_copy_constructible<E>::value,
                                  const expected&> other ) = delete;

    //-----------------------------------------------------------------------

    /// \brief Move-constructs an expected from an existing expected
    ///
    /// \param other the other expected
    expected( enable_overload_if<std::is_move_constructible<T>::value &&
                                  std::is_move_constructible<E>::value,
                                  expected&&> other );
    expected( disable_overload_if<std::is_move_constructible<T>::value &&
                                  std::is_move_constructible<E>::value,
                                  expected&&> other ) = delete;

    //-----------------------------------------------------------------------

    /// \brief Copy converts an expected from an existing expected
    ///
    /// \param other the other expected
#ifndef BIT_DOXYGEN
    template<typename U, typename G, typename = std::enable_if_t<std::is_constructible<T,const U&>::value &&
                                                                  std::is_constructible<E,const G&>::value>>
#else
    template<typename U, typename G>
#endif
    expected( const expected<U,G>& other );

    /// \brief Copy converts an expected from an existing expected
    ///
    /// \param other the other expected
#ifndef BIT_DOXYGEN
    template<typename U, typename G, typename = std::enable_if_t<std::is_constructible<T,U&&>::value &&
                                                                  std::is_constructible<E,G&&>::value>>
#else
    template<typename U, typename G>
#endif
    expected( expected<U,G>&& other );

    //-----------------------------------------------------------------------

    /// \brief Constructs the underlying value of the expected from \p value
    ///
    /// \param value the value to construct the underlying expected
    template<typename U=T,typename = std::enable_if_t<std::is_copy_constructible<U>::value>>
    constexpr expected( const T& value );

    /// \brief Constructs the underlying value of the expected from \p value
    ///
    /// \param value the value to construct the underlying expected
    template<typename U=T,typename = std::enable_if_t<std::is_move_constructible<U>::value>>
    constexpr expected( T&& value );

    //-----------------------------------------------------------------------

    /// \brief Constructs the underlying value of the expected by
    ///        constructing the value in place
    ///
    /// \param args the arguments to forward
    template<typename...Args, typename = std::enable_if_t<std::is_constructible<T,Args...>::value>>
    constexpr explicit expected( in_place_t,
                                  Args&&...args );

    /// \brief Constructs the underlying value of the expected by
    ///        constructing the value in place
    ///
    /// \param ilist an initializer list to forward
    /// \param args the arguments to forward
    template<typename U, typename...Args, typename = std::enable_if_t<std::is_constructible<T,std::initializer_list<U>,Args...>::value>>
    constexpr explicit expected( in_place_t,
                                  std::initializer_list<U> ilist,
                                  Args&&...args );

    //-----------------------------------------------------------------------

    /// \brief Constructs the underlying error from a given unexpected type
    ///
    /// \param unexpected the unexpected type
    template<typename UError=E, typename = std::enable_if_t<std::is_copy_constructible<UError>::value>>
    constexpr expected( unexpected_type<E> const& unexpected );

    /// \brief Constructs the underlying error from a given unexpected type
    ///
    /// \param unexpected the unexpected type
    template<typename Err, typename = std::enable_if_t<std::is_convertible<Err,E>::value>>
    constexpr expected( unexpected_type<Err> const& unexpected );

    //-----------------------------------------------------------------------

    /// \brief Constructs the underlying error of the expected by
    ///        constructing the value in place
    ///
    /// \param args the arguments to forward
    template<typename...Args, typename = std::enable_if_t<std::is_constructible<E, Args...>::value>>
    constexpr explicit expected( unexpect_t,
                                  Args&&...args );

    /// \brief Constructs the underlying error of the expected by
    ///        constructing the value in place
    ///
    /// \param ilist an initializer list to forward
    /// \param args the arguments to forward
    template<typename U, typename...Args, typename = std::enable_if_t<std::is_constructible<E,std::initializer_list<U>,Args...>::value>>
    constexpr explicit expected( unexpect_t,
                                  std::initializer_list<U> ilist,
                                  Args&&...args );

    //-----------------------------------------------------------------------

    // X.Z.4.3, assignment
    template<typename U=T,typename F=E,typename = std::enable_if_t<is_copyable<U,F>>>
    expected& operator=( const expected& other );

    template<typename U=T,typename F=E,typename = std::enable_if_t<is_moveable<U,F>>>
    expected& operator=( expected&& other );

    template<typename U,typename = std::enable_if<is_value_assignable<T,U>>>
    expected& operator=( U&& value );

    expected& operator=( const unexpected_type<E>& unexpected );

    expected& operator=( unexpected_type<E>&& unexpected );

    //-----------------------------------------------------------------------
    // Modifiers
    //-----------------------------------------------------------------------
  public:

    /// \brief Consturcts the underlying value type in-place
    ///
    /// If the expected already contains a value, it is destructed first.
    ///
    /// \param args the arguments to forward to \c T's constructors
    template<typename...Args, typename = std::enable_if_t<std::is_constructible<T,Args...>::value>>
    void emplace( Args&&...args );

    /// \brief Consturcts the underlying value type in-place
    ///
    /// If the expected already contains a value, it is destructed first.
    ///
    /// \param ilist the initializer list of arguments to forward
    /// \param args the arguments to forward to \c T's constructors
    template<typename U, typename...Args, typename = std::enable_if_t<std::is_constructible<T,Args...>::value>>
    void emplace( std::initializer_list<U> ilist, Args&&...args );

    /// \brief Swaps this expected with \p other
    ///
    /// \param other the other expected to swap
    void swap( expected& other );

    //-----------------------------------------------------------------------
    // Observers
    //-----------------------------------------------------------------------
  public:

    /// \brief Queries whether the expected has a value
    ///
    /// \return \c true if the expected has a value
    constexpr bool has_value() const noexcept;

    /// \brief Queries whether the expected has an error
    ///
    /// \return \c true if the expected has an error
    constexpr bool has_error() const noexcept;

    /// \brief Queries whether the expected is valueless_by_exception
    ///
    /// \return \c true if the expected is valueless-by-exception
    constexpr bool valueless_by_exception() const noexcept;

    /// \brief Explicitly convertible to \c bool
    ///
    /// \return \c true if the expected has a value
    constexpr explicit operator bool() const noexcept;

    //-----------------------------------------------------------------------

    constexpr T* operator->();
    constexpr const T* operator->() const;

    constexpr T& operator*() &;
    constexpr const T& operator*() const&;
    constexpr T&& operator*() &&;
    constexpr const T&& operator*() const &&;

    //-----------------------------------------------------------------------

    constexpr const T& value() const&;
    constexpr T& value() &;
    constexpr const T&& value() const &&;
    constexpr T&& value() &&;

    /// \{
    /// \brief Gets the underlying value if there is an value, otherwise
    ///        returns \p default_value
    ///
    /// \param default_value the default value to use
    /// \return the value
    template<typename U>
    constexpr T value_or( U&& default_value ) const &;
    template<typename U>
    constexpr T value_or( U&& default_value ) &&;
    /// \}

    //-----------------------------------------------------------------------

    /// \{
    /// \brief Gets the underlying error
    ///
    /// \throw bad_expected_access<void> if not currently in an error state
    /// \return reference to the underlying error
    constexpr E& error() &;
    constexpr E&& error() &&;
    constexpr const E& error() const &;
    constexpr const E&& error() const &&;
    /// \}

    /// \{
    /// \brief Gets the underlying error if there is an error, otherwise
    ///        returns \p default_value
    ///
    /// \param default_value the default value to use
    /// \return the error
    template<typename U>
    constexpr E error_or( U&& default_value ) const &;
    template<typename U>
    constexpr E error_or( U&& default_value ) &&;
    /// \}

    //-----------------------------------------------------------------------

    /// \{
    /// \brief Gets the underlying expected type
    ///
    /// \throw bad_expected_access<void> if not currently in an error state
    /// \return reference to the unexpected_type
    constexpr unexpected_type<E>& get_unexpected() &;
    constexpr unexpected_type<E>&& get_unexpected() &&;
    constexpr const unexpected_type<E>& get_unexpected() const &;
    constexpr const unexpected_type<E>&& get_unexpected() const &&;
    /// \}

    //------------------------------------------------------------------------
    // Monadic Functionality
    //------------------------------------------------------------------------
  public:

    /// \brief Invokes the function \p fn with this expected as the argument
    ///
    /// If this expected contains an error, the result also contains an error
    /// \param fn the function to invoke with this
    /// \return The result of the function being called
    template<typename Fn,
              typename=std::enable_if_t<conjunction<is_invocable<Fn, const T&>,
                                                    is_expected<invoke_result_t<Fn, const T&>>>::value>>
    invoke_result_t<Fn,const T&> flat_map( Fn&& fn ) const;

    /// \brief Invokes this function \p fn with this expected as the argument
    ///
    /// If this expected contains an error, the result of this function
    /// will contain that error.
    /// Otherwise this function wraps the result and returns it as an expected
    ///
    /// \param fn the function to invoke with this
    /// \return The expected result of the function invoked
    template<typename Fn,
              typename=std::enable_if_t<is_invocable<Fn, const T&>::value>>
    expected<invoke_result_t<Fn,const T&>,E> map( Fn&& fn ) const;

  };

  //=========================================================================
  // X.Z.5, Specialization for void.
  //=========================================================================

  template<typename E>
  class expected<void, E>
    : detail::expected_base<std::is_trivially_destructible<E>::value,empty<void>,E>
  {
    using base_type = detail::expected_base<std::is_trivially_destructible<E>::value,empty<void>,E>;

    //-----------------------------------------------------------------------
    // Public Member Types
    //-----------------------------------------------------------------------
  public:

    using value_type = void;
    using error_type = E;

    template<typename U>
    struct rebind
    {
      typedef expected<U, error_type> type;
    };

    //-----------------------------------------------------------------------
    // Constructors / Destructor / Assignment
    //-----------------------------------------------------------------------
  public:

    /// \brief Default constructs this expected type with no error
    constexpr expected() noexcept;

    //-----------------------------------------------------------------------

    /// \brief Copy-constructs an expected from an existing expected
    ///
    /// \param other the other expected
    expected( enable_overload_if<std::is_copy_constructible<E>::value,
                                  const expected&> other );
    expected( disable_overload_if<std::is_copy_constructible<E>::value,
                                  const expected&> other ) = delete;

    //-----------------------------------------------------------------------

    /// \brief Move-constructs an expected from an existing expected
    ///
    /// \param other the other expected
    expected( enable_overload_if<std::is_move_constructible<E>::value,
                                  expected&&> other );
    expected( disable_overload_if<std::is_move_constructible<E>::value,
                                  expected&&> other ) = delete;

    //-----------------------------------------------------------------------

    /// \brief Copy-converts this expected type from an existing expected
    ///
    /// \param other the other expected to copy
    template<typename G, typename = std::enable_if_t<std::is_constructible<E,const G&>::value>>
    expected( const expected<void,G>& other );

    /// \brief Move-converts this expected type from an existing expected
    ///
    /// \param other the other expected to move
    template<typename G, typename = std::enable_if_t<std::is_constructible<E,G&&>::value>>
    expected( expected<void,G>&& other );

    //-----------------------------------------------------------------------

    /// \brief Constructs this expected type with no error
    constexpr explicit expected( in_place_t );

    //-----------------------------------------------------------------------

    /// \brief Constructs the underlying error from a given unexpected type
    ///
    /// \param unexpected the unexpected type
    template<typename UError=E, typename = std::enable_if_t<std::is_copy_constructible<UError>::value>>
    constexpr expected( unexpected_type<E> const& unexpected );

    /// \brief Constructs the underlying error from a given unexpected type
    ///
    /// \param unexpected the unexpected type
    template<typename Err, typename = std::enable_if_t<std::is_convertible<Err,E>::value>>
    constexpr expected( unexpected_type<Err> const& unexpected );

    //-----------------------------------------------------------------------

    /// \brief Constructs the underlying error of the expected by
    ///        constructing the value in place
    ///
    /// \param args the arguments to forward
    template<typename...Args, typename = std::enable_if_t<std::is_constructible<E, Args...>::value>>
    constexpr explicit expected( unexpect_t,
                                  Args&&...args );

    /// \brief Constructs the underlying error of the expected by
    ///        constructing the value in place
    ///
    /// \param ilist an initializer list to forward
    /// \param args the arguments to forward
    template<typename U, typename...Args, typename = std::enable_if_t<std::is_constructible<E,std::initializer_list<U>,Args...>::value>>
    constexpr explicit expected( unexpect_t,
                                  std::initializer_list<U> ilist,
                                  Args&&...args );

    //-----------------------------------------------------------------------

    /// \brief Copy-assigns an expected from an existing expected
    ///
    /// \param other the other expected
    /// \return reference to \c (*this)
    expected& operator=( const expected& other );

    /// \brief Move-assigns an expected from an existing expected
    ///
    /// \param other the other expected
    /// \return reference to \c (*this)
    expected& operator=( expected&& other );

    //-----------------------------------------------------------------------

    expected& operator=( const unexpected_type<E>& unexpected );

    expected& operator=( unexpected_type<E>&& unexpected );

    //-----------------------------------------------------------------------
    // Modifiers
    //-----------------------------------------------------------------------
  public:

    /// \brief Constructs this expected without an error state
    void emplace();

    /// \brief Swaps this expected with \p other
    ///
    /// \param other the other expected to swap
    void swap( expected& other );

    //-----------------------------------------------------------------------
    // Observers
    //-----------------------------------------------------------------------
  public:

    /// \brief Queries whether the expected has a value
    ///
    /// \return \c true if the expected has a value
    constexpr bool has_value() const noexcept;

    /// \brief Queries whether the expected has an error
    ///
    /// \return \c true if the expected has an error
    constexpr bool has_error() const noexcept;

    /// \brief Queries whether the expected is valueless_by_exception
    ///
    /// \return \c true if the expected is valueless-by-exception
    constexpr bool valueless_by_exception() const noexcept;

    /// \brief Explicitly convertible to \c bool
    ///
    /// \return \c true if the expected has a value
    constexpr explicit operator bool() const noexcept;

    //-----------------------------------------------------------------------

    /// \brief Throws the error, if there is a stored error
    void value() const;

    //-----------------------------------------------------------------------

    /// \{
    /// \brief Gets the underlying error
    ///
    /// \throw bad_expected_access<void> if not currently in an error state
    /// \return reference to the underlying error
    constexpr E& error() &;
    constexpr E&& error() &&;
    constexpr const E& error() const &;
    constexpr const E&& error() const &&;
    /// \}

    /// \{
    /// \brief Gets the underlying error if there is an error, otherwise
    ///        returns \p default_value
    ///
    /// \param default_value the default value to use
    /// \return the error
    template<typename U>
    constexpr E error_or( U&& default_value ) const &;
    template<typename U>
    constexpr E error_or( U&& default_value ) &&;
    /// \}

    //-----------------------------------------------------------------------

    /// \{
    /// \brief Gets the underlying expected type
    ///
    /// \throw bad_expected_access<void> if not currently in an error state
    /// \return reference to the unexpected_type
    constexpr unexpected_type<E>& get_unexpected() &;
    constexpr unexpected_type<E>&& get_unexpected() &&;
    constexpr const unexpected_type<E>& get_unexpected() const &;
    constexpr const unexpected_type<E>&& get_unexpected() const &&;
    /// \}

    //------------------------------------------------------------------------
    // Monadic Functionality
    //------------------------------------------------------------------------
  public:

    /// \brief Invokes the function \p fn with this expected as the argument
    ///
    /// If this expected contains an error, the result also contains an error
    /// \param fn the function to invoke with this
    /// \return The result of the function being called
    template<typename Fn,
              typename=std::enable_if_t<conjunction<is_invocable<Fn>,
                                                    is_expected<invoke_result_t<Fn>>>::value>>
    invoke_result_t<Fn> flat_map( Fn&& fn ) const;

    /// \brief Invokes this function \p fn with this expected as the argument
    ///
    /// If this expected contains an error, the result of this function
    /// will contain that error.
    /// Otherwise this function wraps the result and returns it as an expected
    ///
    /// \param fn the function to invoke with this
    /// \return The expected result of the function invoked
    template<typename Fn,
              typename=std::enable_if_t<is_invocable<Fn>::value>>
    expected<invoke_result_t<Fn>,E> map( Fn&& fn ) const;

  };

  //=========================================================================
  // X.Z.8, Expected relational operators
  //=========================================================================

  template<typename T, typename E>
  constexpr bool operator==( const expected<T,E>& lhs,
                              const expected<T,E>& rhs );
  template<typename T, typename E>
  constexpr bool operator!=( const expected<T,E>& lhs,
                              const expected<T,E>& rhs );
  template<typename T, typename E>
  constexpr bool operator<( const expected<T,E>& lhs,
                            const expected<T,E>& rhs );
  template<typename T, typename E>
  constexpr bool operator>( const expected<T,E>& lhs,
                            const expected<T,E>& rhs );
  template<typename T, typename E>
  constexpr bool operator<=( const expected<T,E>& lhs,
                              const expected<T,E>& rhs );
  template<typename T, typename E>
  constexpr bool operator>=( const expected<T,E>& lhs,
                              const expected<T,E>& rhs );

  //=========================================================================
  // X.Z.9, Comparison with T
  //=========================================================================

  template<typename T, typename E>
  constexpr bool operator==( const expected<T,E>& x, const T& v );
  template<typename T, typename E>
  constexpr bool operator==( const T& v, const expected<T,E>& x );
  template<typename T, typename E>
  constexpr bool operator!=( const expected<T,E>& x, const T& v );
  template<typename T, typename E>
  constexpr bool operator!=( const T& v, const expected<T,E>& x );
  template<typename T, typename E>
  constexpr bool operator<( const expected<T,E>& x, const T& v );
  template<typename T, typename E>
  constexpr bool operator<( const T& v, const expected<T,E>& x );
  template<typename T, typename E>
  constexpr bool operator<=( const expected<T,E>& x, const T& v );
  template<typename T, typename E>
  constexpr bool operator<=( const T& v, const expected<T,E>& x );
  template<typename T, typename E>
  constexpr bool operator>( const expected<T,E>& x, const T& v );
  template<typename T, typename E>
  constexpr bool operator>( const T& v, const expected<T,E>& x );
  template<typename T, typename E>
  constexpr bool operator>=( const expected<T,E>& x, const T& v );
  template<typename T, typename E>
  constexpr bool operator>=( const T& v, const expected<T,E>& x );

  //=========================================================================
  // X.Z.10, Comparison with unexpected_type<E>
  //=========================================================================

  template<typename T, typename E>
  constexpr bool operator==( const expected<T,E>& x,
                              const unexpected_type<E>& e );
  template<typename T, typename E>
  constexpr bool operator==( const unexpected_type<E>& e,
                              const expected<T,E>& x );
  template<typename T, typename E>
  constexpr bool operator!=( const expected<T,E>& x,
                              const unexpected_type<E>& e );
  template<typename T, typename E>
  constexpr bool operator!=( const unexpected_type<E>& e,
                              const expected<T,E>& x );
  template<typename T, typename E>
  constexpr bool operator<( const expected<T,E>& x,
                            const unexpected_type<E>& e );
  template<typename T, typename E>
  constexpr bool operator<( const unexpected_type<E>& e,
                            const expected<T,E>& x );
  template<typename T, typename E>
  constexpr bool operator<=( const expected<T,E>& x,
                              const unexpected_type<E>& e );
  template<typename T, typename E>
  constexpr bool operator<=( const unexpected_type<E>& e,
                              const expected<T,E>& x );
  template<typename T, typename E>
  constexpr bool operator>( const expected<T,E>& x,
                            const unexpected_type<E>& e );
  template<typename T, typename E>
  constexpr bool operator>( const unexpected_type<E>& e,
                            const expected<T,E>& x ) ;
  template<typename T, typename E>
  constexpr bool operator>=( const expected<T,E>& x,
                              const unexpected_type<E>& e );
  template<typename T, typename E>
  constexpr bool operator>=( const unexpected_type<E>& e,
                              const expected<T,E>& x );

  // X.Z.11, Specialized algorithms
  template<typename T, typename E>
  void swap( expected<T,E>& lhs, expected<T,E>& rhs );

} // namespace expect

//=============================================================================
// X.Z.7, class bad_expected_access
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------

template<typename E>
expect::bad_expected_access<E>::bad_expected_access( error_type e )
  : std::logic_error("Found an error instead of the expected value."),
    m_error_value(std::move(e))
{

}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

template<typename E>
typename expect::bad_expected_access<E>::error_type&
  expect::bad_expected_access<E>::error()
  & noexcept
{
  return m_error_value;
}

template<typename E>
const typename expect::bad_expected_access<E>::error_type&
  expect::bad_expected_access<E>::error()
  const & noexcept
{
  return m_error_value;
}

//-----------------------------------------------------------------------------

template<typename E>
typename expect::bad_expected_access<E>::error_type&&
  expect::bad_expected_access<E>::error()
  && noexcept
{
  return std::move(m_error_value);
}

template<typename E>
const typename expect::bad_expected_access<E>::error_type&&
  expect::bad_expected_access<E>::error()
  const && noexcept
{
  return std::move(m_error_value);
}

//=============================================================================

inline expect::bad_expected_access<void>::bad_expected_access()
  : std::logic_error("Bad access to expected type with no value.")
{

}

//=============================================================================
// X.Y.4, unexpected_type
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------

template<typename E>
template<typename E2,typename>
inline constexpr expect::unexpected_type<E>
  ::unexpected_type( const unexpected_type<E2>& other )
  : m_value(other.value())
{

}

template<typename E>
template<typename E2,typename>
inline constexpr expect::unexpected_type<E>
  ::unexpected_type( unexpected_type<E2>&& other )
  : m_value(std::move(other.value()))
{

}

template<typename E>
inline constexpr expect::unexpected_type<E>
  ::unexpected_type( const E& other )
  : m_value(other)
{

}

template<typename E>
inline constexpr expect::unexpected_type<E>
  ::unexpected_type( E&& other )
  : m_value(std::move(other))
{

}

template<typename E>
template<typename...Args, typename>
inline constexpr expect::unexpected_type<E>
  ::unexpected_type( in_place_t, Args&&...args )
  : m_value{ std::forward<Args>(args)... }
{

}

template<typename E>
template<typename U, typename...Args, typename>
inline constexpr expect::unexpected_type<E>
  ::unexpected_type( in_place_t, std::initializer_list<U> ilist, Args&&...args )
  : m_value{ std::move(ilist), std::forward<Args>(args)... }
{

}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

template<typename E>
inline constexpr E& expect::unexpected_type<E>::value()
  &
{
  return m_value;
}

template<typename E>
inline constexpr const E& expect::unexpected_type<E>::value()
  const &
{
  return m_value;
}

//-----------------------------------------------------------------------------

template<typename E>
inline constexpr E&& expect::unexpected_type<E>::value()
  &&
{
  return std::move(m_value);
}

template<typename E>
inline constexpr const E&& expect::unexpected_type<E>::value()
  const &&
{
  return std::move(m_value);
}

//=============================================================================
// X.Y.4, Unexpected factories
//=============================================================================

template<typename E, typename...Args>
inline constexpr expect::unexpected_type<E>
  expect::make_unexpected( Args&&...args )
{
  return unexpected_type<E>( in_place, std::forward<Args>(args)... );
}

//=============================================================================
// X.Y.5, unexpected_type relational operators
//=============================================================================

template<typename E>
inline constexpr bool expect::operator==( const unexpected_type<E>& lhs,
                                            const unexpected_type<E>& rhs )
{
  return lhs.value() == rhs.value();
}

template<typename E>
inline constexpr bool expect::operator!=( const unexpected_type<E>& lhs,
                                            const unexpected_type<E>& rhs )
{
  return lhs.value() != rhs.value();
}

template<typename E>
inline constexpr bool expect::operator<( const unexpected_type<E>& lhs,
                                           const unexpected_type<E>& rhs )
{
  return lhs.value() < rhs.value();
}

template<typename E>
inline constexpr bool expect::operator>( const unexpected_type<E>& lhs,
                                           const unexpected_type<E>& rhs )
{
  return lhs.value() > rhs.value();
}

template<typename E>
inline constexpr bool expect::operator<=( const unexpected_type<E>& lhs,
                                            const unexpected_type<E>& rhs )
{
  return lhs.value() <= rhs.value();
}

template<typename E>
inline constexpr bool expect::operator>=( const unexpected_type<E>& lhs,
                                            const unexpected_type<E>& rhs )
{
  return lhs.value() >= rhs.value();
}

//=============================================================================
// detail::expected_base<true,T,E>
//=============================================================================

//-----------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------

template<typename T, typename E>
inline constexpr expect::detail::expected_base<true,T,E>::expected_base()
  : m_storage(),
    m_has_value(tribool::e_indeterminate)
{

}

template<typename T, typename E>
template<typename...Args>
inline constexpr expect::detail::expected_base<true,T,E>
  ::expected_base( in_place_t, Args&&...args )
  : m_storage( in_place, std::forward<Args>(args)... ),
    m_has_value(tribool::e_true)
{

}

template<typename T, typename E>
template<typename...Args>
inline constexpr expect::detail::expected_base<true,T,E>
  ::expected_base( unexpect_t, Args&&...args )
  : m_storage( unexpect, std::forward<Args>(args)... ),
    m_has_value(tribool::e_false)
{

}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr bool expect::detail::expected_base<true,T,E>
  ::has_value()
  const noexcept
{
  return m_has_value == tribool::e_true;
}

template<typename T, typename E>
inline constexpr bool expect::detail::expected_base<true,T,E>
  ::has_error()
  const noexcept
{
  return m_has_value == tribool::e_false;
}

template<typename T, typename E>
inline constexpr bool expect::detail::expected_base<true,T,E>
  ::valueless_by_exception()
  const noexcept
{
  return m_has_value == tribool::e_indeterminate;
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr T&
  expect::detail::expected_base<true,T,E>::get_value()
  & noexcept
{
  return m_storage.value;
}

template<typename T, typename E>
inline constexpr T&&
  expect::detail::expected_base<true,T,E>::get_value()
  && noexcept
{
  return m_storage.value;
}

template<typename T, typename E>
inline constexpr const T&
  expect::detail::expected_base<true,T,E>::get_value()
  const & noexcept
{
  return m_storage.value;
}

template<typename T, typename E>
inline constexpr const T&&
  expect::detail::expected_base<true,T,E>::get_value()
  const && noexcept
{
  return m_storage.value;
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr expect::unexpected_type<E>&
  expect::detail::expected_base<true,T,E>::get_unexpected()
  & noexcept
{
  return m_storage.error;
}

template<typename T, typename E>
inline constexpr  expect::unexpected_type<E>&&
  expect::detail::expected_base<true,T,E>::get_unexpected()
  && noexcept
{
  return m_storage.error;
}

template<typename T, typename E>
inline constexpr const  expect::unexpected_type<E>&
  expect::detail::expected_base<true,T,E>::get_unexpected()
  const & noexcept
{
  return m_storage.error;
}

template<typename T, typename E>
inline constexpr const  expect::unexpected_type<E>&&
  expect::detail::expected_base<true,T,E>::get_unexpected()
  const && noexcept
{
  return m_storage.error;
}

//-----------------------------------------------------------------------------
// Modifiers
//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename...Args>
inline void expect::detail::expected_base<true,T,E>
  ::emplace_value( Args&&...args )
{
  try {
    new (&m_storage.value) T( std::forward<Args>(args)... );
    m_has_value = tribool::e_true;
  } catch( ... ) {
    m_has_value = tribool::e_indeterminate;
    throw;
  }
}

template<typename T, typename E>
template<typename...Args>
inline void expect::detail::expected_base<true,T,E>
  ::emplace_error( Args&&...args )
{
  try {
    new (&m_storage.error) unexpected_type<E>( std::forward<Args>(args)... );
    m_has_value = tribool::e_false;
  } catch( ... ) {
    m_has_value = tribool::e_indeterminate;
    throw;
  }
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename U>
inline void expect::detail::expected_base<true,T,E>::assign_value( U&& value )
{
  if( m_has_value == tribool::e_true) {
    m_storage.value = std::forward<U>(value);
  } else {
    emplace( std::forward<U>(value) );
  }
}

template<typename T, typename E>
template<typename U>
inline void expect::detail::expected_base<true,T,E>::assign_error( U&& error )
{
  if( m_has_value == tribool::e_false ) {
    m_storage.error = std::forward<U>(error);
  } else {
    emplace_error( std::forward<U>(error) );
  }
}

//=============================================================================
// detail::expected_base<false,T,E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr expect::detail::expected_base<false,T,E>::expected_base()
  : m_storage(),
    m_has_value(tribool::e_indeterminate)
{

}

template<typename T, typename E>
template<typename...Args>
inline constexpr expect::detail::expected_base<false,T,E>
  ::expected_base( in_place_t, Args&&...args )
  : m_storage( in_place, std::forward<Args>(args)... ),
    m_has_value(tribool::e_true)
{

}

template<typename T, typename E>
template<typename...Args>
inline constexpr expect::detail::expected_base<false,T,E>
  ::expected_base( unexpect_t, Args&&...args )
  : m_storage( unexpect, std::forward<Args>(args)... ),
    m_has_value(tribool::e_false)
{

}

//-----------------------------------------------------------------------------

template<typename T, typename E>
inline expect::detail::expected_base<false,T,E>::~expected_base()
{
  destruct();
}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr bool expect::detail::expected_base<false,T,E>::has_value()
  const noexcept
{
  return m_has_value == tribool::e_true;
}

template<typename T, typename E>
constexpr bool expect::detail::expected_base<false,T,E>::has_error()
  const noexcept
{
  return m_has_value == tribool::e_false;
}

template<typename T, typename E>
constexpr bool expect::detail::expected_base<false,T,E>
  ::valueless_by_exception()
  const noexcept
{
  return m_has_value == tribool::e_indeterminate;
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr T&
  expect::detail::expected_base<false,T,E>::get_value()
  & noexcept
{
  return m_storage.value;
}

template<typename T, typename E>
inline constexpr T&&
  expect::detail::expected_base<false,T,E>::get_value()
  && noexcept
{
  return m_storage.value;
}

template<typename T, typename E>
inline constexpr const T&
  expect::detail::expected_base<false,T,E>::get_value()
  const & noexcept
{
  return m_storage.value;
}

template<typename T, typename E>
inline constexpr const T&&
  expect::detail::expected_base<false,T,E>::get_value()
  const && noexcept
{
  return m_storage.value;
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr expect::unexpected_type<E>&
  expect::detail::expected_base<false,T,E>::get_unexpected()
  & noexcept
{
  return m_storage.error;
}

template<typename T, typename E>
inline constexpr  expect::unexpected_type<E>&&
  expect::detail::expected_base<false,T,E>::get_unexpected()
  && noexcept
{
  return m_storage.error;
}

template<typename T, typename E>
inline constexpr const  expect::unexpected_type<E>&
  expect::detail::expected_base<false,T,E>::get_unexpected()
  const & noexcept
{
  return m_storage.error;
}

template<typename T, typename E>
inline constexpr const  expect::unexpected_type<E>&&
  expect::detail::expected_base<false,T,E>::get_unexpected()
  const && noexcept
{
  return m_storage.error;
}

//-----------------------------------------------------------------------------
// Modifiers
//-----------------------------------------------------------------------------

template<typename T, typename E>
inline void expect::detail::expected_base<false,T,E>::destruct()
{
  if( m_has_value == tribool::e_true) {
    m_storage.value.~T();
  } else if( m_has_value == tribool::e_false ) {
    m_storage.error.~E();
  }
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename...Args>
inline void expect::detail::expected_base<false,T,E>
  ::emplace_value( Args&&...args )
{
  destruct();
  try {
    new (&m_storage.value) T{ std::forward<Args>(args)... };
    m_has_value = tribool::e_true;
  } catch( ... ) {
    m_has_value = tribool::e_indeterminate;
    throw;
  }
}

template<typename T, typename E>
template<typename...Args>
void expect::detail::expected_base<false,T,E>
  ::emplace_error( Args&&...args )
{
  destruct();
  try {
    new (&m_storage.error) unexpected_type<E>( std::forward<Args>(args)... );
    m_has_value = tribool::e_false;
  } catch( ... ) {
    m_has_value = tribool::e_indeterminate;
    throw;
  }
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename U>
inline void expect::detail::expected_base<false,T,E>
::assign_value( U&& value )
{
  if( m_has_value == tribool::e_false ) {
    m_storage.value = std::forward<U>(value);
  } else {
    emplace( std::forward<U>(value) );
  }
}

template<typename T, typename E>
template<typename U>
inline void expect::detail::expected_base<false,T,E>
  ::assign_error( U&& error )
{
  if( m_has_value == tribool::e_true ) {
    m_storage.error = std::forward<U>(error);
  } else {
    emplace_error( std::forward<U>(error) );
  }
}

//=============================================================================
// expected<T,E>
//=============================================================================

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename U, typename>
inline constexpr expect::expected<T,E>::expected()
  : base_type( in_place )
{

}

//-----------------------------------------------------------------------------

template<typename T, typename E>
inline expect::expected<T,E>
  ::expected( enable_overload_if<std::is_copy_constructible<T>::value &&
                                 std::is_copy_constructible<E>::value,
                                 const expected&> other )
  : base_type()
{
  if( other.has_value() ) {
    base_type::emplace_value( other.get_value() );
  } else if ( other.has_error() ) {
    base_type::emplace_error( other.get_unexpected() );
  }
}

template<typename T, typename E>
inline expect::expected<T,E>
  ::expected( enable_overload_if<std::is_move_constructible<T>::value &&
                                 std::is_move_constructible<E>::value,
                                 expected&&> other )
  : base_type()
{
  if( other.has_value() ) {
    base_type::emplace_value( std::move(other.get_value()) );
  } else if ( other.has_error() ) {
    base_type::emplace_error( std::move(other.get_unexpected()) );
  }
}

//-----------------------------------------------------------------------

template<typename T, typename E>
template<typename U, typename G, typename>
inline expect::expected<T,E>::expected( const expected<U,G>& other )
  : base_type()
{
  if( other.has_value() ) {
    base_type::emplace_value( other.get_value() );
  } else if ( other.has_error() ) {
    base_type::emplace_error( other.get_unexpected() );
  }
}

template<typename T, typename E>
template<typename U, typename G, typename>
inline expect::expected<T,E>::expected( expected<U,G>&& other )
  : base_type()
{
  if( other.has_value() ) {
    base_type::emplace_value( std::move(other.get_value()) );
  } else if ( other.has_error() ) {
    base_type::emplace_error( std::move(other.get_unexpected()) );
  }
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename U, typename>
inline constexpr expect::expected<T,E>::expected( const T& value )
  : base_type( in_place, value )
{

}

template<typename T, typename E>
template<typename U, typename>
constexpr expect::expected<T,E>::expected( T&& value )
  : base_type( in_place, std::forward<T>(value) )
{

}

//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename...Args, typename>
inline constexpr expect::expected<T,E>
  ::expected( in_place_t, Args&&...args )
  : base_type( in_place, std::forward<Args>(args)... )
{

}

template<typename T, typename E>
template<typename U, typename...Args, typename>
inline constexpr expect::expected<T,E>
  ::expected( in_place_t, std::initializer_list<U> ilist, Args&&...args )
  : base_type( in_place, std::move(ilist), std::forward<Args>(args)... )
{

}

//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename UError, typename>
inline constexpr expect::expected<T,E>
  ::expected( unexpected_type<E> const& unexpected )
  : base_type( unexpect, unexpected.value() )
{

}

template<typename T, typename E>
template<typename Err, typename>
inline constexpr expect::expected<T,E>
  ::expected( unexpected_type<Err> const& unexpected )
  : base_type( unexpect, unexpected.value() )
{

}

//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename...Args, typename>
inline constexpr expect::expected<T,E>
  ::expected( unexpect_t, Args&&...args )
  : base_type( unexpect, std::forward<Args>(args)... )
{

}

template<typename T, typename E>
template<typename U, typename...Args, typename>
inline constexpr expect::expected<T,E>
  ::expected( unexpect_t, std::initializer_list<U> ilist, Args&&...args )
  : base_type( unexpect, std::move(ilist), std::forward<Args>(args)... )
{

}

//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename U, typename F, typename>
inline expect::expected<T,E>& expect::expected<T,E>
  ::operator=( const expected& other )
{
  if( other.has_value() ) {
    base_type::assign_value(other.get_value());
  } else if( other.has_error() ) {
    base_type::assign_error(other.get_unexpected());
  }

  return (*this);
}

template<typename T, typename E>
template<typename U, typename F, typename>
inline expect::expected<T,E>& expect::expected<T,E>
  ::operator=( expected&& other )
{
  if( other.has_value() ) {
    base_type::assign_value( std::move(other.get_value()) );
  } else if( other.has_error() ) {
    base_type::assign_error( std::move(other.get_unexpected()) );
  }

  return (*this);
}

template<typename T, typename E>
template<typename U,typename>
inline expect::expected<T,E>&
  expect::expected<T,E>::operator=( U&& value )
{
  base_type::assign_value( std::forward<U>(value) );

  return (*this);
}

template<typename T, typename E>
inline expect::expected<T,E>&
  expect::expected<T,E>::operator=( const unexpected_type<E>& unexpected )
{
  base_type::assign_error( unexpected.value() );

  return (*this);
}

template<typename T, typename E>
inline expect::expected<T,E>&
  expect::expected<T,E>::operator=( unexpected_type<E>&& unexpected )
{
  base_type::assign_error( std::move(unexpected.value()) );

  return (*this);
}

//-----------------------------------------------------------------------------
// Modifiers
//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename...Args, typename>
inline void expect::expected<T,E>::emplace( Args&&...args )
{
  base_type::emplace_value( std::forward<Args>(args)... );
}

template<typename T, typename E>
template<typename U, typename...Args, typename>
inline void expect::expected<T,E>::emplace( std::initializer_list<U> ilist,
                                              Args&&...args )
{
  base_type::emplace_value( std::move(ilist), std::forward<Args>(args)... );
}

template<typename T, typename E>
inline void expect::expected<T,E>::swap( expected& other )
{

}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr bool expect::expected<T,E>::has_value()
  const noexcept
{
  return base_type::has_value();
}

template<typename T, typename E>
inline constexpr bool expect::expected<T,E>::has_error()
  const noexcept
{
  return base_type::has_error();
}

template<typename T, typename E>
inline constexpr bool expect::expected<T,E>::valueless_by_exception()
  const noexcept
{
  return base_type::valueless_by_exception();
}

template<typename T, typename E>
inline constexpr expect::expected<T,E>::operator bool()
  const noexcept
{
  return has_value();
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr T* expect::expected<T,E>::operator->()
{
  return &base_type::get_value();
}

template<typename T, typename E>
inline constexpr const T* expect::expected<T,E>::operator->()
  const
{
  return &base_type::get_value();
}

template<typename T, typename E>
inline constexpr T& expect::expected<T,E>::operator*()
  &
{
  return base_type::get_value();
}

template<typename T, typename E>
inline constexpr T&& expect::expected<T,E>::operator*()
  &&
{
  return base_type::get_value();
}

template<typename T, typename E>
inline constexpr const T& expect::expected<T,E>::operator*()
  const &
{
  return base_type::get_value();
}

template<typename T, typename E>
inline constexpr const T&& expect::expected<T,E>::operator*()
  const &&
{
  return base_type::get_value();
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr T& expect::expected<T,E>::value()
  &
{
  if( has_error() ) {
    throw bad_expected_access<E>( base_type::get_unexpected().value() );
  } else if ( valueless_by_exception() ) {
    throw bad_expected_access<void>();
  }
  return base_type::get_value();
}

template<typename T, typename E>
inline constexpr T&& expect::expected<T,E>::value()
  &&
{
  if( has_error() ) {
    throw bad_expected_access<E>( base_type::get_unexpected().value() );
  } else if ( valueless_by_exception() ) {
    throw bad_expected_access<void>();
  }
  return std::move(base_type::get_value());
}

template<typename T, typename E>
inline constexpr const T& expect::expected<T,E>::value()
  const &
{
  if( has_error() ) {
    throw bad_expected_access<E>( base_type::get_unexpected().value() );
  } else if ( valueless_by_exception() ) {
    throw bad_expected_access<void>();
  }
  return base_type::get_value();
}

template<typename T, typename E>
inline constexpr const T&& expect::expected<T,E>::value()
  const &&
{
  if( has_error() ) {
    throw bad_expected_access<E>( base_type::get_unexpected().value() );
  } else if ( valueless_by_exception() ) {
    throw bad_expected_access<void>();
  }
  return std::move(base_type::get_value());
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename U>
inline constexpr T expect::expected<T,E>::value_or( U&& default_value )
  const &
{
  return bool(*this) ? base_type::get_value() : std::forward<U>(default_value);
}

template<typename T, typename E>
template<typename U>
inline constexpr T expect::expected<T,E>::value_or( U&& default_value )
  &&
{
  return bool(*this) ? base_type::get_value() : std::forward<U>(default_value);
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
constexpr E& expect::expected<T,E>::error()
  &
{
  return get_unexpected().value();
}

template<typename T, typename E>
constexpr E&& expect::expected<T,E>::error()
  &&
{
  return std::move(get_unexpected().value());
}
template<typename T, typename E>
constexpr const E& expect::expected<T,E>::error()
  const &
{
  return get_unexpected().value();
}
template<typename T, typename E>
constexpr const E&& expect::expected<T,E>::error()
  const &&
{
  return std::move(get_unexpected().value());
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename U>
inline constexpr E expect::expected<T,E>::error_or( U&& default_value )
  const &
{
  return !bool(*this) ? base_type::get_unexpected().value() : std::forward<U>(default_value);
}

template<typename T, typename E>
template<typename U>
inline constexpr E expect::expected<T,E>::error_or( U&& default_value )
  &&
{
  return !bool(*this) ? base_type::get_unexpected().value() : std::forward<U>(default_value);
}

//-----------------------------------------------------------------------------

template<typename T, typename E>
inline constexpr expect::unexpected_type<E>&
  expect::expected<T,E>::get_unexpected()
  &
{
  if( !has_error() ) {
    throw bad_expected_access<void>();
  }

  return base_type::get_unexpected();
}

template<typename T, typename E>
inline constexpr expect::unexpected_type<E>&&
  expect::expected<T,E>::get_unexpected()
  &&
{
  if( !has_error() ) {
    throw bad_expected_access<void>();
  }

  return std::move(base_type::get_unexpected());
}

template<typename T, typename E>
inline constexpr const expect::unexpected_type<E>&
  expect::expected<T,E>::get_unexpected()
  const &
{
  if( !has_error() ) {
    throw bad_expected_access<void>();
  }

  return base_type::get_unexpected();
}

template<typename T, typename E>
inline constexpr const expect::unexpected_type<E>&&
  expect::expected<T,E>::get_unexpected()
  const &&
{
  if( !has_error() ) {
    throw bad_expected_access<void>();
  }

  return std::move(base_type::get_unexpected());
}


//-----------------------------------------------------------------------------
// Monadic Functions
//-----------------------------------------------------------------------------

template<typename T, typename E>
template<typename Fn, typename>
expect::invoke_result_t<Fn,const T&>
  expect::expected<T,E>::flat_map( Fn&& fn )
  const
{
  if( has_value() ) return std::forward<Fn>(fn);
  else if( has_error() ) return { get_unexpected() };
  throw bad_expected_access<void>{};
}

template<typename T, typename E>
template<typename Fn, typename>
expect::expected<expect::invoke_result_t<Fn,const T&>,E>
  expect::expected<T,E>::map( Fn&& fn )
  const
{
  if( has_value() ) return { std::forward<Fn>(fn) };
  else if( has_error() ) return { get_unexpected() };
  throw bad_expected_access<void>{};
}

//=============================================================================
// expected<void,E>
//=============================================================================

template<typename E>
inline constexpr expect::expected<void,E>::expected()
  noexcept
  : base_type( in_place )
{

}


//-----------------------------------------------------------------------------

template<typename E>
inline expect::expected<void,E>
  ::expected( enable_overload_if<std::is_copy_constructible<E>::value,
                                 const expected&> other )
  : base_type()
{
  if ( other.has_error() ) {
    base_type::emplace_error( other.get_unexpected() );
  }
}

template<typename E>
inline expect::expected<void,E>
  ::expected( enable_overload_if<std::is_move_constructible<E>::value,
                                 expected&&> other )
  : base_type()
{
  if ( other.has_error() ) {
    base_type::emplace_error( std::move(other.get_unexpected()) );
  }
}

//-----------------------------------------------------------------------------

template<typename E>
template<typename G, typename>
inline expect::expected<void,E>::expected( const expected<void,G>& other )
  : base_type()
{
  if ( other.has_error() ) {
    base_type::emplace_error( other.get_unexpected() );
  }
}

template<typename E>
template<typename G, typename>
inline expect::expected<void,E>::expected( expected<void,G>&& other )
  : base_type()
{
  if ( other.has_error() ) {
    base_type::emplace_error( std::move(other.get_unexpected()) );

  }
}

//-----------------------------------------------------------------------------

template<typename E>
inline constexpr expect::expected<void,E>::expected( in_place_t )
  : base_type( in_place )
{

}

//-----------------------------------------------------------------------------

template<typename E>
template<typename UError, typename>
inline constexpr expect::expected<void,E>
  ::expected( unexpected_type<E> const& unexpected )
  : base_type( unexpect, unexpected )
{

}

template<typename E>
template<typename Err, typename>
inline constexpr expect::expected<void,E>
  ::expected( unexpected_type<Err> const& unexpected )
  : base_type( unexpect, std::move(unexpected) )
{

}

//-----------------------------------------------------------------------------

template<typename E>
template<typename...Args, typename>
inline constexpr expect::expected<void,E>
  ::expected( unexpect_t, Args&&...args )
  : base_type( unexpect, std::forward<Args>(args)... )
{

}

template<typename E>
template<typename U, typename...Args, typename>
inline constexpr expect::expected<void,E>
  ::expected( unexpect_t, std::initializer_list<U> ilist, Args&&...args )
  : base_type( unexpect, std::forward<Args>(args)... )
{

}

//-----------------------------------------------------------------------------

template<typename E>
inline expect::expected<void,E>&
  expect::expected<void,E>::operator=( const expected& other )
{

  if( other.has_value() ) {
    emplace();
  } else if( other.has_error() ) {
    base_type::assign_error( other.get_unexpected() );
  }

  return (*this);
}

template<typename E>
inline expect::expected<void,E>&
  expect::expected<void,E>::operator=( expected&& other )
{
  if( other.has_value() ) {
    emplace();
  } else if( other.has_error() ) {
    base_type::assign_error( std::move(other.get_unexpected()) );
  }

  return (*this);
}

//-----------------------------------------------------------------------------

template<typename E>
inline expect::expected<void,E>&
  expect::expected<void,E>::operator=( const unexpected_type<E>& unexpected )
{
  base_type::assign_error( unexpected );

  return (*this);
}

template<typename E>
inline expect::expected<void,E>&
  expect::expected<void,E>::operator=( unexpected_type<E>&& unexpected )
{
  base_type::assign_error( std::move(unexpected) );

  return (*this);
}

//-----------------------------------------------------------------------------
// Modifiers
//-----------------------------------------------------------------------------

template<typename E>
inline void expect::expected<void,E>::emplace()
{
  base_type::emplace_value();
}

template<typename E>
inline void expect::expected<void,E>::swap( expected& other )
{

}

//-----------------------------------------------------------------------------
// Observers
//-----------------------------------------------------------------------------

template<typename E>
inline constexpr bool expect::expected<void,E>::has_value()
  const noexcept
{
  return base_type::has_value();
}

template<typename E>
inline constexpr bool expect::expected<void,E>::has_error()
  const noexcept
{
  return base_type::has_error();
}

template<typename E>
inline constexpr bool expect::expected<void,E>::valueless_by_exception()
  const noexcept
{
  return base_type::valueless_by_exception();
}

template<typename E>
inline constexpr expect::expected<void,E>::operator bool()
  const noexcept
{
  return has_value();
}

//-----------------------------------------------------------------------

template<typename E>
inline void expect::expected<void,E>::value()
  const
{
  if( has_error() ) {
    throw bad_expected_access<E>( base_type::get_unexpected().value() );
  } else if ( valueless_by_exception() ) {
    throw bad_expected_access<void>();
  }
#endif
}

//-----------------------------------------------------------------------

template<typename E>
inline constexpr E& expect::expected<void,E>::error()
  &
{
  return get_unexpected().value();
}

template<typename E>
inline constexpr E&& expect::expected<void,E>::error()
  &&
{
  return std::move(get_unexpected().value());
}

template<typename E>
inline constexpr const E& expect::expected<void,E>::error()
  const &
{
  return get_unexpected().value();
}

template<typename E>
inline constexpr const E&& expect::expected<void,E>::error()
  const &&
{
  return std::move(get_unexpected().value());
}

//-----------------------------------------------------------------------

template<typename E>
template<typename U>
inline constexpr E expect::expected<void,E>::error_or( U&& default_value )
  const &
{
  return !bool(*this) ? base_type::get_unexpected().value() : std::forward<U>(default_value);
}

template<typename E>
template<typename U>
inline constexpr E expect::expected<void,E>::error_or( U&& default_value )
  &&
{
  return !bool(*this) ? base_type::get_unexpected().value() : std::forward<U>(default_value);
}

//-----------------------------------------------------------------------

template<typename E>
inline constexpr expect::unexpected_type<E>&
  expect::expected<void,E>::get_unexpected()
  &
{
  if( !has_error() ) {
    throw bad_expected_access<void>();
  }

  return base_type::get_unexpected();
}

template<typename E>
inline constexpr expect::unexpected_type<E>&&
  expect::expected<void,E>::get_unexpected()
  &&
{
  if( !has_error() ) {
    throw bad_expected_access<void>();
  }

  return std::move(base_type::get_unexpected());
}

template<typename E>
inline constexpr const expect::unexpected_type<E>&
  expect::expected<void,E>::get_unexpected()
  const &
{
  if( !has_error() ) {
    throw bad_expected_access<void>();
  }

  return base_type::get_unexpected();
}

template<typename E>
inline constexpr const expect::unexpected_type<E>&&
  expect::expected<void,E>::get_unexpected()
  const &&
{
  if( !has_error() ) {
    throw bad_expected_access<void>();
  }

  return std::move(base_type::get_unexpected());
}

//-----------------------------------------------------------------------------
// Monadic Functions
//-----------------------------------------------------------------------------

template<typename E>
template<typename Fn, typename>
expect::invoke_result_t<Fn>
  expect::expected<void,E>::flat_map( Fn&& fn )
  const
{
  if( has_value() ) return std::forward<Fn>(fn);
  else if( has_error() ) return { get_unexpected() };
  throw bad_expected_access<void>{};
}

template<typename E>
template<typename Fn, typename>
expect::expected<expect::invoke_result_t<Fn>,E>
  expect::expected<void,E>::map( Fn&& fn )
  const
{
  if( has_value() ) return { std::forward<Fn>(fn) };
  else if( has_error() ) return { get_unexpected() };
  throw bad_expected_access<void>{};
}

//=============================================================================
// X.Z.8, Expected relational operators
//=============================================================================

template<typename T, typename E>
inline constexpr bool
  expect::operator==( const expected<T,E>& lhs, const expected<T,E>& rhs )
{
  return lhs.has_value() != rhs.has_value() ? false :
         lhs.has_error() ? lhs.get_unexpected() == rhs.get_unexpected() :
         lhs.has_value() ? *lhs == *rhs : true;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator!=( const expected<T,E>& lhs, const expected<T,E>& rhs )
{
  return lhs.has_value() != rhs.has_value() ? true :
         lhs.has_error() ? lhs.get_unexpected() != rhs.get_unexpected() :
         lhs.has_value() ? *lhs != *rhs : false;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator<( const expected<T,E>& lhs, const expected<T,E>& rhs )
{
  return lhs.has_value() && rhs.has_value() ? *lhs < *rhs :
         lhs.has_error() && rhs.has_error() ? lhs.get_unexpected() < rhs.get_unexpected() :
         lhs.has_value() && !rhs.has_value() ? true :
        !lhs.has_value() && rhs.has_value() ? false :
         lhs.valueless_by_exception() && rhs.valueless_by_exception() ? false :
         false;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator>( const expected<T,E>& lhs, const expected<T,E>& rhs )
{
  return lhs.has_value() && rhs.has_value() ? *lhs > *rhs :
         lhs.has_error() && rhs.has_error() ? lhs.get_unexpected() > rhs.get_unexpected() :
         lhs.has_value() && !rhs.has_value() ? false :
        !lhs.has_value() && rhs.has_value() ? true :
         lhs.valueless_by_exception() && rhs.valueless_by_exception() ? false :
         false;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator<=( const expected<T,E>& lhs, const expected<T,E>& rhs )
{
  return lhs.has_value() && rhs.has_value() ? *lhs <= *rhs :
         lhs.has_error() && rhs.has_error() ? lhs.get_unexpected() <= rhs.get_unexpected() :
         lhs.has_value() && !rhs.has_value() ? true :
        !lhs.has_value() && rhs.has_value() ? false :
         lhs.valueless_by_exception() && rhs.valueless_by_exception() ? true :
         false;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator>=( const expected<T,E>& lhs, const expected<T,E>& rhs )
{
  return lhs.has_value() && rhs.has_value() ? *lhs >= *rhs :
         lhs.has_error() && rhs.has_error() ? lhs.get_unexpected() >= rhs.get_unexpected() :
         lhs.has_value() && !rhs.has_value() ? false :
        !lhs.has_value() && rhs.has_value() ? true :
         lhs.valueless_by_exception() && rhs.valueless_by_exception() ? true :
         false;

}

//=============================================================================
// X.Z.9, Comparison with T
//=============================================================================

template<typename T, typename E>
inline constexpr bool
  expect::operator==( const expected<T,E>& x, const T& v )
{
  return static_cast<bool>(x) ? *x == v : false;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator==( const T& v, const expected<T,E>& x )
{
  return static_cast<bool>(x) ? v == *x : false;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator!=( const expected<T,E>& x, const T& v )
{
  return static_cast<bool>(x) ? *x != v : true;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator!=( const T& v, const expected<T,E>& x )
{
  return static_cast<bool>(x) ? v != *x : true;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator<( const expected<T,E>& x, const T& v )
{
  return static_cast<bool>(x) ? *x < v : false;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator<( const T& v, const expected<T,E>& x )
{
  return static_cast<bool>(x) ? v < *x : true;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator<=( const expected<T,E>& x, const T& v )
{
  return static_cast<bool>(x) ? *x <= v : false;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator<=( const T& v, const expected<T,E>& x )
{
  return static_cast<bool>(x) ? v <= *x : true;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator>( const expected<T,E>& x, const T& v )
{
  return static_cast<bool>(x) ? *x > v : true;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator>( const T& v, const expected<T,E>& x )
{
  return static_cast<bool>(x) ? v > *x : false;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator>=( const expected<T,E>& x, const T& v )
{
  return static_cast<bool>(x) ? *x >= v : true;
}

template<typename T, typename E>
inline constexpr bool
  expect::operator>=( const T& v, const expected<T,E>& x )
{
  return static_cast<bool>(x) ? v >= *x : false;
}

//=============================================================================
// X.Z.10, Comparison with unexpected_type<E>
//=============================================================================

template<typename T, typename E>
inline constexpr bool expect::operator==( const expected<T,E>& x,
                                            const unexpected_type<E>& e )
{
  return static_cast<bool>(x) ? true : x.get_unexpected() == e;
}

template<typename T, typename E>
inline constexpr bool expect::operator==( const unexpected_type<E>& e,
                                            const expected<T,E>& x )
{
  return static_cast<bool>(x) ? true : e== x.get_unexpected();
}

template<typename T, typename E>
inline constexpr bool expect::operator!=( const expected<T,E>& x,
                                            const unexpected_type<E>& e)
{
  return static_cast<bool>(x) ? false : x.get_unexpected() != e;
}

template<typename T, typename E>
inline constexpr bool expect::operator!=( const unexpected_type<E>& e,
                                            const expected<T,E>& x )
{
  return static_cast<bool>(x) ? false : e != x.get_unexpected();
}

template<typename T, typename E>
inline constexpr bool expect::operator<( const expected<T,E>& x,
                                           const unexpected_type<E>& e )
{
  return static_cast<bool>(x) ? true : x.get_unexpected() < e;
}

template<typename T, typename E>
inline constexpr bool expect::operator<( const unexpected_type<E>& e,
                                           const expected<T,E>& x )
{
  return static_cast<bool>(x) ? false : e < x.get_unexpected();
}

template<typename T, typename E>
inline constexpr bool expect::operator<=( const expected<T,E>& x,
                                            const unexpected_type<E>& e )
{
  return static_cast<bool>(x) ? true : x.get_unexpected() <= e;
}

template<typename T, typename E>
inline constexpr bool expect::operator<=( const unexpected_type<E>& e,
                                            const expected<T,E>& x )
{
  return static_cast<bool>(x) ? false : e <= x.get_unexpected();
}

template<typename T, typename E>
inline constexpr bool expect::operator>( const expected<T,E>& x,
                                           const unexpected_type<E>& e )
{
  return static_cast<bool>(x) ? false : x.get_unexpected() > e;
}

template<typename T, typename E>
inline constexpr bool expect::operator>( const unexpected_type<E>& e,
                                           const expected<T,E>& x )
{
  return static_cast<bool>(x) ? true : e > x.get_unexpected();
}

template<typename T, typename E>
inline constexpr bool expect::operator>=( const expected<T,E>& x,
                                            const unexpected_type<E>& e )
{
  return static_cast<bool>(x) ? false : x.get_unexpected() >= e;
}

template<typename T, typename E>
inline constexpr bool expect::operator>=( const unexpected_type<E>& e,
                                            const expected<T,E>& x )
{
  return static_cast<bool>(x) ? true : e >= x.get_unexpected();
}

//-----------------------------------------------------------------------------

// X.Z.11, Specialized algorithms

template<typename T, typename E>
inline void expect::swap( expected<T,E>& lhs, expected<T,E>& rhs )
{
  lhs.swap(rhs);
}

#endif /* EXPECT_EXPECT_HPP */
