from conan import ConanFile

class ConverterConan(ConanFile):
    name = "converter"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    def requirements(self):
        self.requires("sqlite3/3.44.2")
        self.requires("opencv/4.14.0")
        self.requires("zlib/1.3.1")
        self.requires("tinyxml2/9.0.0")
        self.requires("libjpeg/9f", force=True)
        self.requires("libtiff/4.6.0")
        self.requires("xz_utils/5.4.5")
        self.requires("libwebp/1.3.2")
