import slideiopybind as sld
from enum import Enum

# class Compression(Enum):
#     Unknown = sld.Unknown
#     Uncompressed = sld.Uncompressed
#     Jpeg = sld.Jpeg
#     JpegXR = sld.JpegXR
#     Png = sld.Png
#     Jpeg2000 = sld.Jpeg2000
#     LZW = sld.LZW
#     HuffmanRL = sld.HuffmanRL
#     CCITT_T4 = sld.CCITT_T4
#     CCITT_T6 = sld.CCITT_T6
#     LempelZivWelch = sld.LempelZivWelch
#     JpegOld = sld.JpegOld
#     Zlib = sld.Zlib
#     JBIG85 = sld.JBIG85
#     JBIG43 = sld.JBIG43
#     NextRLE = sld.NextRLE
#     PackBits = sld.PackBits
#     ThunderScanRLE = sld.ThunderScanRLE
#     RasterPadding = sld.RasterPadding
#     RLE_LW = sld.RLE_LW
#     RLE_HC = sld.RLE_HC
#     RLE_BL = sld.RLE_BL
#     PKZIP = sld.PKZIP
#     KodakDCS = sld.KodakDCS
#     JBIG = sld.JBIG
#     NikonNEF = sld.NikonNEF
#     JBIG2 = sld.JBIG2
#     GIF = sld.GIF
#     BIGGIF = sld.BIGGIF
 

class Scene:
    def __init__(self, slide, index:int):
        self.scene = slide.get_scene(index)

    def __del__(self):
        if self.scene is not None:
            del self.scene
            self.scene = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.__del__()

    @property
    def name(self):
        return self.scene.name

    @property
    def compression(self):
        return self.scene.compression

    @property
    def file_path(self):
        return self.scene.file_path

    @property
    def magnification(self):
        return self.scene.magnification

    @property
    def num_channels(self):
        return self.scene.num_channels

    @property
    def num_t_frames(self):
        return self.scene.num_t_frames

    @property
    def num_z_slices(self):
        return self.scene.num_z_slices

    @property
    def rect(self):
        return self.scene.rect

    @property
    def size(self):
        rc = self.scene.rect
        return (rc[2], rc[3])
    
    @property
    def origin(self):
        rc = self.scene.rect
        return (rc[0], rc[1])

    @property
    def resolution(self):
        return self.scene.resolution

    @property
    def t_resolution(self):
        return  self.scene.t_resolution

    @property
    def z_resolution(self):
        return self.scene.z_resolution

    def read_block(self, rect=(0,0,0,0), size=(0,0), channel_indices=[], slices=(0,1), frames=(0,1)):
        return self.scene.read_block(rect, size, channel_indices, slices, frames)

    def get_channel_data_type(self, channel):
        return self.scene.get_channel_data_type(channel)

    def get_channel_name(self, channel):
        return self.scene.get_channel_name(channel)


class Slide:
    def __init__(self, path:str, driver:str):
        self.slide = sld.open_slide(path, driver)

    def __del__(self):
        if hasattr(self, 'slide'):
            if self.slide is not None:
                del self.slide
                self.slide = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.__del__()

    def get_scene(self, index):
        """Return slide scene by index"""
        scene = Scene(self.slide, index)
        return scene
    
    @property
    def num_scenes(self) -> int:
        """Number of scenes in the slide"""
        return self.slide.num_scenes

    @property
    def raw_metadata(self) -> str:
        """Raw metadata extracted from the slide"""
        return self.slide.raw_metadata

    @property
    def file_path(self):
        return self.slide.file_path

def open_slide(path:str, driver:str):
    """Returns an instance of a slide object"""
    slide = Slide(path, driver)
    return slide

def get_driver_ids():
    return sld.get_driver_ids()