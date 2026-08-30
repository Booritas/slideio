from conan import ConanFile

class GdalRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    def requirements(self):
        self.requires("opencv/4.14.0")
        self.requires("nlohmann_json/3.11.3")
