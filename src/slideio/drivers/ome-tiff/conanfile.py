from conan import ConanFile

# A conanfile.py rather than a conanfile.txt purely so that libjpeg can be
# required with force=True, as imagetools, svs, pke, afi and converter already
# do. libtiff/4.6.0 pulls libjpeg/9e transitively, and a plain requirement for
# 9f alongside it is a version conflict conan refuses to resolve on its own; a
# conanfile.txt has no way to express the override. Keeping one libjpeg per
# graph matters here: the ndpi driver already brings a second, incompatible
# jpeg implementation into the process, and two copies of the same symbol names
# make which one answers a call depend on load order.
class OmeTiffConan(ConanFile):
    name = "ome-tiff"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("opencv/4.14.0")
        self.requires("libtiff/4.6.0")
        self.requires("libjpeg/9f", force=True)
        self.requires("libwebp/1.3.2")
        self.requires("libpng/1.6.53")
        self.requires("openjpeg/2.5.2")
        self.requires("tinyxml2/9.0.0")
