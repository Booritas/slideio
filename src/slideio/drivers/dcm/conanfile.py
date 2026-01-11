from conan import ConanFile

class DCMConan(ConanFile):
    name = "dcm"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"
    def requirements(self):
        self.requires("sqlite3/3.44.2")
        self.requires("glog/0.7.1")
        self.requires("opencv/4.10.0@slideio/stable")
        self.requires("xz_utils/5.4.5")
        self.requires("zlib/1.3.1")
        self.requires("dcmtk/3.6.9")
        self.requires("icu/76.1@slideio/stable", force=True)
        self.requires("libwebp/1.3.2")
        self.requires("openjpeg/2.5.2")
