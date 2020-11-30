cmake_minimum_required(VERSION 3.8)

project(Expected.ConanTestPackage)

include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
conan_set_find_paths()

find_package(Expected REQUIRED)

set(source_files
  src/main.cpp
)

add_executable(${PROJECT_NAME}
  ${source_files}
)
target_link_libraries(${PROJECT_NAME}
  PRIVATE Expected::Expected
)
