#!/usr/bin/env python
from conans import ConanFile, CMake

class ExpectedConanTest(ConanFile):
  settings = "os", "compiler", "arch", "build_type"
  generators = "cmake"

  def build(self):
    cmake = CMake(self)
    cmake.configure()
    cmake.build()

  def imports(self):
    pass

  def test(self):
    pass
