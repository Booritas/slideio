// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "pyslide.hpp"
#include "pyglobals.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>

#include "pyconverter.hpp"
#include "pytransformation.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/transformer/wrappers.hpp"
#include "slideio/transformer/colorspace.hpp"

namespace py = pybind11;

PYBIND11_MODULE(slideiopybind, m) {
    m.doc() = R"delimiter(
        Module for reading of medical images.
    )delimiter";
    m.def("open_slide", &pyOpenSlide,
        py::arg("file_path"),
        py::arg("driver_id"),
        "Opens an image slide. Returns Slide object.");
    m.def("set_log_level", &pySetLogLevel,
        py::arg("log_level"),
        "Sets log level for the library.");
    m.def("get_driver_ids", &pyGetDriverIDs,
        "Returns list of driver ids");
    m.def("convert_scene", &pyConvertFile,
        py::arg("scene"),
        py::arg("parameters"),
        py::arg("file_path"));
    m.def("convert_scene_ex", &pyConvertFileEx,
        py::arg("scene"),
        py::arg("parameters"),
        py::arg("file_path"),
        py::arg("callback"));
    m.def("transform_scene", &pyTransformScene,
        py::arg("scene"),
        py::arg("parameters"));
    py::class_<PySlide, std::shared_ptr<PySlide>>(m, "Slide")
        .def("get_scene", &PySlide::getScene, py::arg("index"), "Returns a Scene object by index")
        .def("get_aux_image", &PySlide::getAuxImage, py::arg("image_name"), "Returns an auxiliary image object by name")
        .def("get_aux_image_names", &PySlide::getAuxImageNames, "Returns list of aux images")
        .def_property_readonly("num_scenes", &PySlide::getNumScenes, "Number of scenes in the slide")
        .def_property_readonly("num_aux_images", &PySlide::getNumAuxImages, "Number of auxiliary images")
        .def_property_readonly("raw_metadata", &PySlide::getRawMetadata, "Slide raw metadata")
        .def_property_readonly("file_path", &PySlide::getFilePath, "File path to the image");
    py::class_<PyScene, std::shared_ptr<PyScene>>(m, "Scene")
        .def_property_readonly("name", &PyScene::getName, "Scene name")
        .def_property_readonly("resolution", &PyScene::getResolution, "Scene resolution in x and y directions")
        .def_property_readonly("z_resolution", &PyScene::getZSliceResolution, "Scene resolution in z direction")
        .def_property_readonly("t_resolution", &PyScene::getTFrameResolution, "Time frame resolution")
        .def_property_readonly("magnification", &PyScene::getMagnification, "Scanning magnification")
        .def_property_readonly("num_z_slices", &PyScene::getNumZSlices, "Number of slices in z direction")
        .def_property_readonly("num_t_frames", &PyScene::getNumTFrames, "Number of time frames")
        .def_property_readonly("compression", &PyScene::getCompression, "Compression type")
        .def_property_readonly("file_path", &PyScene::getFilePath, "File path to the scene")
        .def_property_readonly("rect", &PyScene::getRect, "Scene rectangle")
        .def_property_readonly("num_channels", &PyScene::getNumChannels, "Number of channels in the scene")
        .def_property_readonly("num_aux_images", &PyScene::getNumAuxImages, "Number of auxiliary images")
        .def("get_raw_metadata", &PyScene::getRawMetadata, "Scene raw metadata")
        .def("get_channel_data_type", &PyScene::getChannelDataType, py::arg("index"), "Returns datatype of a channel by index")
        .def("get_channel_name", &PyScene::getChannelName, py::arg("index"), "Returns channel name (if any)")
        .def("get_aux_image", &PyScene::getAuxImage, py::arg("image_name"), "Returns an auxiliary image object by name")
        .def("get_aux_image_names", &PyScene::getAuxImageNames, "Returns list of aux images")
        .def_property_readonly("num_zoom_levels", &PyScene::getNumZoomLevels, "Number of levels in the scene image pyramid")
        .def("get_zoom_level_info", &PyScene::getZoomLevelInfo, py::arg("index"), "Returns zoom level info by index")
        .def("read_block", &PyScene::readBlock,
            py::arg("rect") = std::tuple<int, int, int, int>(0, 0, 0, 0),
            py::arg("size") = std::tuple<int, int>(0, 0),
            py::arg("channel_indices") = std::vector<int>(),
            py::arg("slices") = std::tuple<int, int>(0, 1),
            py::arg("frames") = std::tuple<int, int>(0, 1),
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
        )
        .def("__repr__", &PyScene::toString);
    py::enum_<slideio::Compression>(m, "Compression")
        .value("Unknown", slideio::Compression::Unknown)
        .value("Uncompressed", slideio::Compression::Uncompressed)
        .value("Jpeg", slideio::Compression::Jpeg)
        .value("JpegXR", slideio::Compression::JpegXR)
        .value("Png", slideio::Compression::Png)
        .value("Jpeg2000", slideio::Compression::Jpeg2000)
        .value("LZW", slideio::Compression::LZW)
        .value("HuffmanRL", slideio::Compression::HuffmanRL)
        .value("CCITT_T4", slideio::Compression::CCITT_T4)
        .value("CCITT_T6", slideio::Compression::CCITT_T6)
        .value("JpegOld", slideio::Compression::JpegOld)
        .value("Zlib", slideio::Compression::Zlib)
        .value("JBIG85", slideio::Compression::JBIG85)
        .value("JBIG43", slideio::Compression::JBIG43)
        .value("NextRLE", slideio::Compression::NextRLE)
        .value("PackBits", slideio::Compression::PackBits)
        .value("ThunderScanRLE", slideio::Compression::ThunderScanRLE)
        .value("RasterPadding", slideio::Compression::RasterPadding)
        .value("RLE_LW", slideio::Compression::RLE_LW)
        .value("RLE_HC", slideio::Compression::RLE_HC)
        .value("RLE_BL", slideio::Compression::RLE_BL)
        .value("PKZIP", slideio::Compression::PKZIP)
        .value("KodakDCS", slideio::Compression::KodakDCS)
        .value("JBIG", slideio::Compression::JBIG)
        .value("NikonNEF", slideio::Compression::NikonNEF)
        .value("JBIG2", slideio::Compression::JBIG2)
        .value("GIF", slideio::Compression::GIF)
        .value("BIGGIF", slideio::Compression::BIGGIF)
        .export_values();
    py::class_<slideio::Rectangle>(m, "Rectangle")
        .def_readwrite("x", &slideio::Rectangle::x)
        .def_readwrite("y", &slideio::Rectangle::y)
        .def_readwrite("width", &slideio::Rectangle::width)
        .def_readwrite("height", &slideio::Rectangle::height);
    py::enum_<slideio::ImageFormat>(m, "ImageFormat")
        .value("Unknown", slideio::ImageFormat::Unknown)
        .value("SVS", slideio::ImageFormat::SVS);
    py::class_<slideio::ConverterParameters>(m, "ConverterParameters")
        .def_property_readonly("format", &slideio::ConverterParameters::getFormat, "Format of output file")
        .def_property("rect", &slideio::ConverterParameters::getRect, &slideio::ConverterParameters::setRect, "Scene region")
        .def_property("z_slice", &slideio::ConverterParameters::getZSlice, &slideio::ConverterParameters::setZSlice, "Z slice")
        .def_property("t_frame", &slideio::ConverterParameters::getTFrame, &slideio::ConverterParameters::setTFrame, "Time frame");
    py::class_<slideio::SVSConverterParameters, slideio::ConverterParameters>(m, "SVSParameters")
        .def_property("tile_width", &slideio::SVSConverterParameters::getTileWidth, &slideio::SVSConverterParameters::setTileWidth, "Width of tiles in pixels")
        .def_property("tile_height", &slideio::SVSConverterParameters::getTileHeight, &slideio::SVSConverterParameters::setTileHeight, "Height of tiles in pixels")
        .def_property("num_zoom_levels", &slideio::SVSConverterParameters::getNumZoomLevels, &slideio::SVSConverterParameters::setNumZoomLevels, "Number of zoom levels")
        .def_property_readonly("encoding", &slideio::SVSConverterParameters::getEncoding, "Tile encoding parameters");
    py::class_<slideio::SVSJpegConverterParameters, slideio::SVSConverterParameters, slideio::ConverterParameters>(m, "SVSJpegParameters")
        .def(py::init<>())
        .def_property("quality", &slideio::SVSJpegConverterParameters::getQuality, &slideio::SVSJpegConverterParameters::setQuality, "Quality of JPEG encoding");
    py::class_<slideio::SVSJp2KConverterParameters, slideio::SVSConverterParameters, slideio::ConverterParameters>(m, "SVSJp2KParameters")
        .def(py::init<>())
        .def_property("compression_rate", &slideio::SVSJp2KConverterParameters::getCompressionRate, &slideio::SVSJp2KConverterParameters::setCompressionRate, "Compression rate of JPEG200 encoding");
    py::class_<slideio::ColorTransformationWrap>(m, "ColorTransformation")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::ColorTransformationWrap::getType, "Type of transformation")
        .def_property("color_space", &slideio::ColorTransformationWrap::getColorSpace, &slideio::ColorTransformationWrap::setColorSpace, "Target color space");
    py::enum_<slideio::ColorSpace>(m, "ColorSpace")
        .value("GRAY", slideio::ColorSpace::GRAY)
        .value("HSV", slideio::ColorSpace::HSV)
        .value("HLS", slideio::ColorSpace::HLS)
        .value("YCbCr", slideio::ColorSpace::YCbCr)
        .value("XYZ", slideio::ColorSpace::XYZ)
        .value("Lab", slideio::ColorSpace::LAB)
        .value("Luv", slideio::ColorSpace::LUV)
        .value("YUV", slideio::ColorSpace::YUV);
    py::enum_<slideio::DataType>(m, "DataType")
        .value("Byte", slideio::DataType::DT_Byte)
        .value("Int8", slideio::DataType::DT_Int8)
        .value("Int16", slideio::DataType::DT_Int16)
        .value("Float16", slideio::DataType::DT_Float16)
        .value("Int32", slideio::DataType::DT_Int32)
        .value("Float32", slideio::DataType::DT_Float32)
        .value("Float64", slideio::DataType::DT_Float64)
        .value("UInt16", slideio::DataType::DT_UInt16)
        .value("Unknown", slideio::DataType::DT_Unknown)
        .value("None", slideio::DataType::DT_None);
    py::class_<slideio::GaussianBlurFilterWrap>(m, "GaussianBlurFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::GaussianBlurFilterWrap::getType, "Type of transformation")
        .def_property("kernel_size_x", &slideio::GaussianBlurFilterWrap::getKernelSizeX, &slideio::GaussianBlurFilterWrap::setKernelSizeX, "Kernel size along x-axis")
        .def_property("kernel_size_y", &slideio::GaussianBlurFilterWrap::getKernelSizeY, &slideio::GaussianBlurFilterWrap::setKernelSizeY, "Kernel size along y-axis")
        .def_property("sigma_x", &slideio::GaussianBlurFilterWrap::getSigmaX, &slideio::GaussianBlurFilterWrap::setSigmaX, "Sigma along x-axis")
        .def_property("sigma_y", &slideio::GaussianBlurFilterWrap::getSigmaY, &slideio::GaussianBlurFilterWrap::setSigmaY, "Sigma along y-axis");
    py::class_<slideio::MedianBlurFilterWrap>(m, "MedianBlurFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::MedianBlurFilterWrap::getType, "Type of transformation")
        .def_property("kernel_size", &slideio::MedianBlurFilterWrap::getKernelSize, &slideio::MedianBlurFilterWrap::setKernelSize, "Kernel size");
    py::class_<slideio::SobelFilterWrap>(m, "SobelFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::SobelFilterWrap::getType, "Type of transformation")
        .def_property("kernel_size", &slideio::SobelFilterWrap::getKernelSize, &slideio::SobelFilterWrap::setKernelSize, "Kernel size")
        .def_property("dx", &slideio::SobelFilterWrap::getDx, &slideio::SobelFilterWrap::setDx, "Derivative order along x-axis")
        .def_property("dy", &slideio::SobelFilterWrap::getDy, &slideio::SobelFilterWrap::setDy, "Derivative order along y-axis")
        .def_property("depth", &slideio::SobelFilterWrap::getDepth, &slideio::SobelFilterWrap::setDepth, "Depth of output image")
        .def_property("scale", &slideio::SobelFilterWrap::getScale, &slideio::SobelFilterWrap::setScale, "Scale factor")
        .def_property("delta", &slideio::SobelFilterWrap::getDelta, &slideio::SobelFilterWrap::setDelta, "Delta value");
    py::class_<slideio::ScharrFilterWrap>(m, "ScharrFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::ScharrFilterWrap::getType, "Type of transformation")
        .def_property("dx", &slideio::ScharrFilterWrap::getDx, &slideio::ScharrFilterWrap::setDx, "Derivative order along x-axis")
        .def_property("dy", &slideio::ScharrFilterWrap::getDy, &slideio::ScharrFilterWrap::setDy, "Derivative order along y-axis")
        .def_property("depth", &slideio::ScharrFilterWrap::getDepth, &slideio::ScharrFilterWrap::setDepth, "Depth of output image")
        .def_property("scale", &slideio::ScharrFilterWrap::getScale, &slideio::ScharrFilterWrap::setScale, "Scale factor")
        .def_property("delta", &slideio::ScharrFilterWrap::getDelta, &slideio::ScharrFilterWrap::setDelta, "Delta value");
    py::class_<slideio::LaplacianFilterWrap>(m, "LaplacianFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::LaplacianFilterWrap::getType, "Type of transformation")
        .def_property("kernel_size", &slideio::LaplacianFilterWrap::getKernelSize, &slideio::LaplacianFilterWrap::setKernelSize, "Kernel size")
        .def_property("depth", &slideio::LaplacianFilterWrap::getDepth, &slideio::LaplacianFilterWrap::setDepth, "Depth of output image")
        .def_property("scale", &slideio::LaplacianFilterWrap::getScale, &slideio::LaplacianFilterWrap::setScale, "Scale factor")
        .def_property("delta", &slideio::LaplacianFilterWrap::getDelta, &slideio::LaplacianFilterWrap::setDelta, "Delta value");
    py::class_<slideio::BilateralFilterWrap>(m, "BilateralFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::BilateralFilterWrap::getType, "Type of transformation")
        .def_property("diameter", &slideio::BilateralFilterWrap::getDiameter, &slideio::BilateralFilterWrap::setDiameter, "Diameter of each pixel neighborhood")
        .def_property("sigma_color", &slideio::BilateralFilterWrap::getSigmaColor, &slideio::BilateralFilterWrap::setSigmaColor, "Filter sigma in the color space")
        .def_property("sigma_space", &slideio::BilateralFilterWrap::getSigmaSpace, &slideio::BilateralFilterWrap::setSigmaSpace, "Filter sigma in the coordinate space");
    py::class_<slideio::CannyFilterWrap>(m, "CannyFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::CannyFilterWrap::getType, "Type of transformation")
        .def_property("threshold1", &slideio::CannyFilterWrap::getThreshold1, &slideio::CannyFilterWrap::setThreshold1, "First threshold for the hysteresis procedure")
        .def_property("threshold2", &slideio::CannyFilterWrap::getThreshold2, &slideio::CannyFilterWrap::setThreshold2, "Second threshold for the hysteresis procedure")
        .def_property("aperture_size", &slideio::CannyFilterWrap::getApertureSize, &slideio::CannyFilterWrap::setApertureSize, "Aperture size for the Sobel operator")
        .def_property("l2gradient", &slideio::CannyFilterWrap::getL2Gradient, &slideio::CannyFilterWrap::setL2Gradient, "Indicates, whether L2 norm should be used");
    py::class_<slideio::LevelInfo>(m, "LevelInfo")
        .def_property_readonly("size", &slideio::LevelInfo::getSize, "Size of the level")
        .def_property_readonly("tile_size", &slideio::LevelInfo::getTileSize, "Size of the tile")
        .def_property_readonly("level", &slideio::LevelInfo::getLevel, "Level index")
        .def_property_readonly("scale", &slideio::LevelInfo::getScale, "Scale coefficient")
        .def_property_readonly("magnification", &slideio::LevelInfo::getMagnification, "Level magnification")
        .def("__repr__", &slideio::LevelInfo::toString);
    py::class_<slideio::Size>(m, "Size")
        .def(py::init<int, int>())
        .def_readwrite("width", &slideio::Size::width)
        .def_readwrite("height", &slideio::Size::height);
}