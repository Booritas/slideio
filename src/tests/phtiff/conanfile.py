from conan import ConanFile

# See src/slideio/drivers/ome-tiff/conanfile.py: a .py rather than a .txt so
# that libjpeg/9f can override the libjpeg/9e that libtiff/4.6.0 requires
# transitively, which a conanfile.txt cannot express.
class PhTiffTestsConan(ConanFile):
    name = "phtiff-tests"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("gtest/1.17.0")
        self.requires("sqlite3/3.44.2")
        self.requires("opencv/4.14.0")
        self.requires("zlib/1.3.1")
        self.requires("xz_utils/5.4.5")
        self.requires("libtiff/4.6.0")
        self.requires("libjpeg/9f", force=True)
        self.requires("libwebp/1.3.2")
        self.requires("libpng/1.6.53")
        self.requires("openjpeg/2.5.2")
        self.requires("libiconv/1.17")
        self.requires("tinyxml2/9.0.0")
