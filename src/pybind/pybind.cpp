#include "pyslide.hpp"
#include "pyglobals.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

namespace py = pybind11;

PYBIND11_MODULE(slideiopybind, m) {
    m.doc() = R"delimiter(
        Module for reading of medical images.
    )delimiter";
    m.def("open_slide",&pyOpenSlide,
        py::arg("file_path"),
        py::arg("driver_id"),
        "Opens an image slide. Returns Slide object.");
    m.def("get_driver_ids", &pyGetDriverIDs,
        "Returns list of driver ids");
    m.def("compare_images", &pyCompareImages,
        py::arg("left"),
        py::arg("right"));
    py::class_<PySlide, std::shared_ptr<PySlide>>(m, "Slide")
        .def("get_scene", &PySlide::getScene, py::arg("index"), "Returns a Scene object by index")
        .def("get_aux_image", &PySlide::getAuxImage, py::arg("image_name"), "Returns an auxiliary image object by name")
        .def("get_aux_image_names", &PySlide::getAuxImageNames, "Returns list of aux images")
        .def_property_readonly("num_scenes", &PySlide::getNumScenes, "Number of scenes in the slide")
        .def_property_readonly("num_aux_images", &PySlide::getNumAuxImages, "Number of auxiliary images")
        .def_property_readonly("raw_metadata", &PySlide::getRawMetadata, "Slide raw metadata")
        .def_property_readonly("file_path", &PySlide::getFilePath, "File path to the image");
    py::class_<PyScene, std::shared_ptr<PyScene>>(m, "Scene")
        .def_property_readonly("name",&PyScene::getName, "Scene name")
        .def_property_readonly("resolution",&PyScene::getResolution, "Scene resolution in x and y directions")
        .def_property_readonly("z_resolution",&PyScene::getZSliceResolution, "Scene resolution in z direction")
        .def_property_readonly("t_resolution",&PyScene::getTFrameResolution, "Time frame resolution")
        .def_property_readonly("magnification",&PyScene::getMagnification, "Scanning magnification")
        .def_property_readonly("num_z_slices",&PyScene::getNumZSlices, "Number of slices in z direction")
        .def_property_readonly("num_t_frames",&PyScene::getNumTFrames, "Number of time frames")
        .def_property_readonly("compression",&PyScene::getCompression, "Compression type")
        .def_property_readonly("file_path", &PyScene::getFilePath, "File path to the scene")
        .def_property_readonly("rect", &PyScene::getRect, "Scene rectangle")
        .def_property_readonly("num_channels", &PyScene::getNumChannels, "Number of channels in the scene")
        .def_property_readonly("num_aux_images", &PyScene::getNumAuxImages, "Number of auxiliary images")
        .def("get_channel_data_type", &PyScene::getChannelDataType, py::arg("index"), "Returns datatype of a channel by index")
        .def("get_channel_name", &PyScene::getChannelName, py::arg("index"), "Returns channel name (if any)")
        .def("get_aux_image", &PyScene::getAuxImage, py::arg("image_name"), "Returns an auxiliary image object by name")
        .def("get_aux_image_names", &PyScene::getAuxImageNames, "Returns list of aux images")
        .def("read_block", &PyScene::readBlock,
            py::arg("rect") = std::tuple<int,int,int,int>(0,0,0,0),
            py::arg("size")=std::tuple<int,int>(0,0),
            py::arg("channel_indices") = std::vector<int>(),
            py::arg("slices")=std::tuple<int,int>(0,1),
            py::arg("frames")=std::tuple<int,int>(0,1),
            R"del(
            Reads rectangular block of the scene with optional rescaling.

            Args:
                rect: block rectangle, defined as a tuple (x, y, widht, height), where x,y - pixel coordinates of the left top corner of the block, width, height - block width and height
                size: size of the block after rescaling (0,0) - no scaling.
                channel_indices: array of channel indices to be retrieved. [] - all channels.
                slices: range of z slices (first, last+1) to be retrieved. (0,3) for 0,1,2 slices. (0,0) for the first slice only.
                frames: range of time frames (first, last+1) to be retrieved.

            Returns:
                numpy array with pixel values
            )del"
            );
    py::enum_<slideio::Compression>(m,"Compression")
        .value("Unknown",slideio::Compression::Unknown)
        .value("Uncompressed",slideio::Compression::Uncompressed)
        .value("Jpeg",slideio::Compression::Jpeg)
        .value("JpegXR",slideio::Compression::JpegXR)
        .value("Png",slideio::Compression::Png)
        .value("Jpeg2000",slideio::Compression::Jpeg2000)
        .value("LZW",slideio::Compression::LZW)
        .value("HuffmanRL",slideio::Compression::HuffmanRL)
        .value("CCITT_T4",slideio::Compression::CCITT_T4)
        .value("CCITT_T6",slideio::Compression::CCITT_T6)
        .value("LempelZivWelch",slideio::Compression::LempelZivWelch)
        .value("JpegOld",slideio::Compression::JpegOld)
        .value("Zlib",slideio::Compression::Zlib)
        .value("JBIG85",slideio::Compression::JBIG85)
        .value("JBIG43",slideio::Compression::JBIG43)
        .value("NextRLE",slideio::Compression::NextRLE)
        .value("PackBits",slideio::Compression::PackBits)
        .value("ThunderScanRLE",slideio::Compression::ThunderScanRLE)
        .value("RasterPadding",slideio::Compression::RasterPadding)
        .value("RLE_LW",slideio::Compression::RLE_LW)
        .value("RLE_HC",slideio::Compression::RLE_HC)
        .value("RLE_BL",slideio::Compression::RLE_BL)
        .value("PKZIP",slideio::Compression::PKZIP)
        .value("KodakDCS",slideio::Compression::KodakDCS)
        .value("JBIG",slideio::Compression::JBIG)
        .value("NikonNEF",slideio::Compression::NikonNEF)
        .value("JBIG2",slideio::Compression::JBIG2)
        .value("GIF",slideio::Compression::GIF)
        .value("BIGGIF",slideio::Compression::BIGGIF)
        .export_values();
}