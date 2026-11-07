from conan import ConanFile

class MrxsConan(ConanFile):
    name = "mrxs"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("opencv/4.14.0")
        # Inflates StitchingIntensityLayer position maps (zlib-wrapped DEFLATE).
        self.requires("zlib/1.3.1")
