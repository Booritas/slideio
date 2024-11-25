from conan import ConanFile

class GdalRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    default_options = {"glog:shared": True}
    def requirements(self):
        self.requires("boost/1.86.0")
        self.requires("sqlite3/3.38.5")
        self.requires("glog/0.6.0@slideio/stable")
        self.requires("opencv/4.10.0@slideio/stable")
        self.requires("zlib/1.2.13")
        self.requires("xz_utils/5.4.2")
        self.requires("libtiff/4.4.0")
        self.requires("libjpeg/9e")
        self.requires("libwebp/1.2.4")
        self.requires("libpng/1.6.38")
        self.requires("openjpeg/2.5.0")
        self.requires("jpegxrcodec/1.0.3@slideio/stable")
        self.requires("libiconv/1.17")
        self.requires("libdeflate/1.17")
        self.requires("gdal/3.8.3")
 