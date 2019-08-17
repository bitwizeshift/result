cmake_minimum_required(VERSION 3.1)

#.rst:
# AddelfContainmentTest
# ---------
#
# Creates a C or C++ library that includes each specified header file
# independently to ensure that each header carries no expected ordering.
#
# This is used to avoid accidental dependencies based on the order of
# inclusion.
#
# ::
#
#     add_self_containment_test(
#       <name>
#       TARGET <target>
#       [EXTENSION ext]
#       HEADERS [headers]...
#     )
#
#     <target>      - The name of the target to create
#     [C|CXX]       - The language check. By default, this is CXX
#     [headers]...  - List of headers to compile
#
function(add_self_containment_test name)

  ############################### Setup output ###############################

  cmake_parse_arguments("CONTAINMENT" "" "TARGET;EXTENSION;DESTINATION" "HEADERS" ${ARGN})

  if( TARGET "${name}" )
    message(FATAL_ERROR "Specified target name already exists as a valid CMake target.")
  endif()

  if( CONTAINMENT_HEADERS )
    set(header_files ${CONTAINMENT_HEADERS})
  else()
    message(FATAL_ERROR "No HEADERS specified")
  endif()

  if( CONTAINMENT_EXTENSION )
    set(extension "${CONTAINMENT_EXTENSION}" )
  else()
    set(extension "cpp")
  endif()

  if( CONTAINMENT_DESTINATION )
    set(output_dir "${CONTAINMENT_DESTINATION}")
  else()
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/${name}")
  endif()

  if( NOT "${CONTAINMENT_TARGET}" STREQUAL "" AND TARGET "${CONTAINMENT_TARGET}")
    set(target "${CONTAINMENT_TARGET}")
  elseif( NOT TARGET "${CONTAINMENT_TARGET}" )
    message(FATAL_ERROR "Specified TARGET is not a valid CMake target")
  elseif( "${CONTAINMENT_TARGET}" NOT STREQUAL "" )
    message(FATAL_ERROR "TARGET not specified")
  endif()

  ############################### Create files ###############################

  set(source_files)
  foreach( header ${header_files} )

    get_filename_component(path_segment "${header}" PATH)
    get_filename_component(absolute_header "${header}" ABSOLUTE)
    file(RELATIVE_PATH relative_header "${output_dir}/${path_segment}" "${absolute_header}")

    set(output_path "${output_dir}/${header}.${extension}")

    if( NOT EXISTS "${output_path}" OR "${absolute_header}" IS_NEWER_THAN "${output_path}" )

      file(WRITE "${output_path}" "#include \"${relative_header}\" // IWYU pragma: keep")

    endif()

    set_property( DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${output_path}")
    set_property( DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${output_path}")

    list(APPEND source_files "${output_path}")

  endforeach()

  ###################### Create self-containment Target ######################

  add_library("${name}" OBJECT
    ${source_files}
    ${header_files}
  )

  # Iterate over the various CMake target properties, and propagate them
  # manually
  set(properties
    INCLUDE_DIRECTORIES
    SYSTEM_INCLUDE_DIRECTORIES
    LINK_LIBRARIES
    COMPILE_OPTIONS
    COMPILE_FEATURES
    COMPILE_DEFINITIONS
  )

  # Append each property as a generator-expression
  foreach( property ${properties} )
    set_property(
      TARGET "${name}"
      APPEND PROPERTY "${property}"
      "$<TARGET_PROPERTY:${target},INTERFACE_${property}>"
    )
  endforeach()

endfunction()
