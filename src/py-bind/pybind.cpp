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
#include "slideio/transformer/transformations.hpp"

namespace py = pybind11;

PYBIND11_MODULE(slideiopybind, m) {
    m.doc() = R"delimiter(
        Module for reading of medical images.
    )delimiter";
    m.def("open_slide",&pyOpenSlide,
        py::arg("file_path"),
        py::arg("driver_id"),
        "Opens an image slide. Returns Slide object.");
    m.def("set_log_level",&pySetLogLevel,
          py::arg("log_level"),
          "Sets log level for the library.");
    m.def("get_driver_ids", &pyGetDriverIDs,
        "Returns list of driver ids");
    m.def("compare_images", &pyCompareImages,
        py::arg("left"),
        py::arg("right"));
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
        .def("get_raw_metadata", &PyScene::getRawMetadata, "Scene raw metadata")
        .def("get_channel_data_type", &PyScene::getChannelDataType, py::arg("index"), "Returns datatype of a channel by index")
        .def("get_channel_name", &PyScene::getChannelName, py::arg("index"), "Returns channel name (if any)")
        .def("get_aux_image", &PyScene::getAuxImage, py::arg("image_name"), "Returns an auxiliary image object by name")
        .def("get_aux_image_names", &PyScene::getAuxImageNames, "Returns list of aux images")
        .def("get_num_levels", &PyScene::getNumZoomLevels, "Returns number of levels in the scene")
        .def("get_level_info", &PyScene::getZoomLevelInfo, py::arg("index"), "Returns level info by index")
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
    py::class_<slideio::Rectangle>(m, "Rectangle")
        .def_readwrite("x",&slideio::Rectangle::x)
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
    py::class_<slideio::Transformation>(m, "Transformation")
        .def_property_readonly("type", &slideio::Transformation::getType, "Type of transformation");
    py::class_<slideio::ColorTransformation, slideio::Transformation>(m, "ColorTransformation")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::ColorTransformation::getType, "Type of transformation")
        .def_property("color_space", &slideio::ColorTransformation::getColorSpace, &slideio::ColorTransformation::setColorSpace, "Target color space");
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
    py::class_<slideio::GaussianBlurFilter, slideio::Transformation>(m, "GaussianBlurFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::GaussianBlurFilter::getType, "Type of transformation")
        .def_property("kernel_size_x", &slideio::GaussianBlurFilter::getKernelSizeX, &slideio::GaussianBlurFilter::setKernelSizeX, "Kernel size along x-axis")
        .def_property("kernel_size_y", &slideio::GaussianBlurFilter::getKernelSizeY, &slideio::GaussianBlurFilter::setKernelSizeY, "Kernel size along y-axis")
        .def_property("sigma_x", &slideio::GaussianBlurFilter::getSigmaX, &slideio::GaussianBlurFilter::setSigmaX, "Sigma along x-axis")
        .def_property("sigma_y", &slideio::GaussianBlurFilter::getSigmaY, &slideio::GaussianBlurFilter::setSigmaY, "Sigma along y-axis");
    py::class_<slideio::MedianBlurFilter, slideio::Transformation>(m, "MedianBlurFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::MedianBlurFilter::getType, "Type of transformation")
        .def_property("kernel_size", &slideio::MedianBlurFilter::getKernelSize, &slideio::MedianBlurFilter::setKernelSize, "Kernel size");
    py::class_<slideio::SobelFilter, slideio::Transformation>(m, "SobelFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::SobelFilter::getType, "Type of transformation")
        .def_property("kernel_size", &slideio::SobelFilter::getKernelSize, &slideio::SobelFilter::setKernelSize, "Kernel size")
        .def_property("dx", &slideio::SobelFilter::getDx, &slideio::SobelFilter::setDx, "Derivative order along x-axis")
        .def_property("dy", &slideio::SobelFilter::getDy, &slideio::SobelFilter::setDy, "Derivative order along y-axis")
        .def_property("depth", &slideio::SobelFilter::getDepth, &slideio::SobelFilter::setDepth, "Depth of output image")
        .def_property("scale", &slideio::SobelFilter::getScale, &slideio::SobelFilter::setScale, "Scale factor")
        .def_property("delta", &slideio::SobelFilter::getDelta, &slideio::SobelFilter::setDelta, "Delta value");
    py::class_<slideio::ScharrFilter, slideio::Transformation>(m, "ScharrFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::ScharrFilter::getType, "Type of transformation")
        .def_property("dx", &slideio::ScharrFilter::getDx, &slideio::ScharrFilter::setDx, "Derivative order along x-axis")
        .def_property("dy", &slideio::ScharrFilter::getDy, &slideio::ScharrFilter::setDy, "Derivative order along y-axis")
        .def_property("depth", &slideio::ScharrFilter::getDepth, &slideio::ScharrFilter::setDepth, "Depth of output image")
        .def_property("scale", &slideio::ScharrFilter::getScale, &slideio::ScharrFilter::setScale, "Scale factor")
        .def_property("delta", &slideio::ScharrFilter::getDelta, &slideio::ScharrFilter::setDelta, "Delta value");
    py::class_<slideio::LaplacianFilter, slideio::Transformation>(m, "LaplacianFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::LaplacianFilter::getType, "Type of transformation")
        .def_property("kernel_size", &slideio::LaplacianFilter::getKernelSize, &slideio::LaplacianFilter::setKernelSize, "Kernel size")
        .def_property("depth", &slideio::LaplacianFilter::getDepth, &slideio::LaplacianFilter::setDepth, "Depth of output image")
        .def_property("scale", &slideio::LaplacianFilter::getScale, &slideio::LaplacianFilter::setScale, "Scale factor")
        .def_property("delta", &slideio::LaplacianFilter::getDelta, &slideio::LaplacianFilter::setDelta, "Delta value");
    py::class_<slideio::BilateralFilter, slideio::Transformation>(m, "BilateralFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::BilateralFilter::getType, "Type of transformation")
        .def_property("diameter", &slideio::BilateralFilter::getDiameter, &slideio::BilateralFilter::setDiameter, "Diameter of each pixel neighborhood")
        .def_property("sigma_color", &slideio::BilateralFilter::getSigmaColor, &slideio::BilateralFilter::setSigmaColor, "Filter sigma in the color space")
        .def_property("sigma_space", &slideio::BilateralFilter::getSigmaSpace, &slideio::BilateralFilter::setSigmaSpace, "Filter sigma in the coordinate space");
    py::class_<slideio::CannyFilter, slideio::Transformation>(m, "CannyFilter")
        .def(py::init<>())
        .def_property_readonly("type", &slideio::CannyFilter::getType, "Type of transformation")
        .def_property("threshold1", &slideio::CannyFilter::getThreshold1, &slideio::CannyFilter::setThreshold1, "First threshold for the hysteresis procedure")
        .def_property("threshold2", &slideio::CannyFilter::getThreshold2, &slideio::CannyFilter::setThreshold2, "Second threshold for the hysteresis procedure")
        .def_property("aperture_size", &slideio::CannyFilter::getApertureSize, &slideio::CannyFilter::setApertureSize, "Aperture size for the Sobel operator")
        .def_property("l2gradient", &slideio::CannyFilter::getL2Gradient, &slideio::CannyFilter::setL2Gradient, "Indicates, whether L2 norm should be used");
}