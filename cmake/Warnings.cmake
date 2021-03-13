if(
    CMAKE_CXX_COMPILER_ID MATCHES "Clang"
    AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"
)
  set(
      project_warnings

      # clang-cl does not appear to implement '-pedantic' or 'pedantic-errors',
      # so this case needs to be handled specifically
      -Wall -Werror -Wextra

      # clang-cl treats '-Wall' like '-Weverything' currently; so there are a
      # few warnings we need to manually disable.

      # Disable unnecessary compatibility and 'newline-eof'. This is a modern
      # C++ library -- so these serve no purpose
      -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-newline-eof

      # Disable warnings on padding. This is a great warning, but not good when
      # coupled with -Werror
      -Wno-padded

      # Several utilities require static objects that have exit-time
      # destructors, including 'error_category' types from the standard.
      -Wno-exit-time-destructors

      # This is erroring on valid uses of the standard library, like
      # std::lock_guard.
      -Wno-ctad-maybe-unsupported

      # This is a good warning *in general*, but not when using MSVC's standard
      # library, which requires defining "_" prefixed macros just to have it
      # operate like the C++ standard dictates.
      -Wno-reserved-id-macro

      # Don't warn on ignored attributes; this breaks builds using
      # dllexport/dllimport on classes that may also define inline functions
      -Wno-ignored-attributes
  )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|^GNU$")
  set(
      project_warnings
      -Wall -Werror -Wextra -pedantic -pedantic-errors

      # Xcode 11.2 seems to enable -Wnewline-eof with '-Wextra'. Disable this,
      # since it's a legacy requirement not needed in C++11 onward.
      -Wno-newline-eof

      # Don't warn on ignored attributes; this breaks builds using
      # dllexport/dllimport on classes that may also define inline functions
      -Wno-ignored-attributes
  )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(project_warnings /W4 /WX)
endif()
