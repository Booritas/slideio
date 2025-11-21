from conan import ConanFile

class GdalRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    def requirements(self):
        self.requires("sqlite3/3.44.2")
        self.requires("glog/0.7.1")
        self.requires("opencv/4.10.0@slideio/stable")
        self.requires("zlib/1.2.13")
        self.requires("xz_utils/5.4.5")
        self.requires("libtiff/4.6.0")
        self.requires("libjpeg/9f", force=True)
        self.requires("libwebp/1.3.2")
        self.requires("libpng/1.6.40")
        self.requires("openjpeg/2.5.2")
        self.requires("jpegxrcodec/1.0.3@slideio/stable")
        self.requires("libiconv/1.17")
        self.requires("libdeflate/1.19")
        self.requires("gdal/3.8.3")
        self.requires("nlohmann_json/3.11.3")
        self.requires("libgeotiff/1.7.4", override=True)
 