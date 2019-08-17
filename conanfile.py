#!/usr/bin/env python3

from conans import ConanFile, CMake

class ExpectedConan(ConanFile):

    # Package Info
    name = "Expected"
    version = "0.0.1"
    description = "Expect the unexpected"
    url = "https://github.com/bitwizeshift/Expected"
    author = "Matthew Rodusek <matthew.rodusek@gmail.com>"
    license = "MIT"

    # Sources
    exports = ( "LICENSE" )
    exports_sources = ( "CMakeLists.txt",
                        "cmake/*",
                        "include/*",
                        "LICENSE" )

    # Settings
    options = {}
    default_options = {}
    generators = "cmake"

    # Dependencies
    build_requires = ("Catch2/2.7.1@catchorg/stable")


    def configure_cmake(self):
        cmake = CMake(self)

        cmake.configure()
        return cmake


    def build(self):
        cmake = self.configure_cmake()
        cmake.build()
        # cmake.test()


    def package(self):
        cmake = self.configure_cmake()
        cmake.install()

        self.copy(pattern="LICENSE", dst="licenses")


    def package_id(self):
        self.info.header_only()
