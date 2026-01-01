from conan import ConanFile

class GdalRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    def requirements(self):
        self.requires("glog/0.7.1")
        self.requires("opencv/4.10.0@slideio/stable")
        self.requires("nlohmann_json/3.11.3") 