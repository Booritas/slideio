from conan import ConanFile

class LifConan(ConanFile):
    name = "lif"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("opencv/4.14.0")
        self.requires("tinyxml2/9.0.0")
