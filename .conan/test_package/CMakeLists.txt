cmake_minimum_required(VERSION 3.8)

project(Result.ConanTestPackage)

include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
conan_set_find_paths()

find_package(Result REQUIRED)

set(source_files
  src/main.cpp
)

add_executable(${PROJECT_NAME}
  ${source_files}
)
target_link_libraries(${PROJECT_NAME}
  PRIVATE Result::Result
)
