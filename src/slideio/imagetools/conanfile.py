from conan import ConanFile


class ImageToolsRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    default_options = {"glog:shared": True}

    def requirements(self):
        self.requires("boost/1.81.0@slideio/stable")
        self.requires("sqlite3/3.38.5@slideio/stable")
        self.requires("libxml2/2.9.10@slideio/stable")
        self.requires("glog/0.6.0@slideio/stable")
        self.requires("opencv/4.1.1@slideio/stable")
        self.requires("zlib/1.3.1")
        self.requires("xz_utils/5.4.2")
        self.requires("libtiff/4.4.0")
        self.requires("libjpeg/9e")
        self.requires("libwebp/1.2.4")
        self.requires("libpng/1.6.38")
        self.requires("openjpeg/2.5.0")
        self.requires("jpegxrcodec/1.0.3@slideio/stable")
        self.requires("libiconv/1.17")
        self.requires("libdeflate/1.17")
        if self.settings.os == "Windows":
            self.requires("gdal/3.5.2")
        else:
            self.requires("gdal/3.4.3@slideio/stable")
            self.requires("openssl/3.1.0")
