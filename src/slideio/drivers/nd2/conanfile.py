from conan import ConanFile

class Nd2Conan(ConanFile):
    name = "nd2"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("opencv/4.14.0")
        self.requires("tinyxml2/9.0.0")
        self.requires("zlib/1.3.1")
        self.requires("nlohmann_json/3.11.3")
