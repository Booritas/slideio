from conan import ConanFile

class AfiConan(ConanFile):
    name = "afi"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("sqlite3/3.44.2")
        self.requires("glog/0.7.1")
        self.requires("opencv/4.10.0@slideio/stable")
        self.requires("zlib/1.3.1")
        self.requires("xz_utils/5.4.5")
        self.requires("libtiff/4.6.0")
        self.requires("libjpeg/9f", force=True)
        self.requires("libwebp/1.3.2")
        self.requires("libpng/1.6.53")
        self.requires("openjpeg/2.5.2")
        self.requires("tinyxml2/9.0.0")
        self.requires("libiconv/1.17")
