#include "pyslide.hpp"
#include "pyglobals.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

namespace py = pybind11;

PYBIND11_MODULE(slideio, m) {
    m.doc() = "Reading of medical images";
    m.def("open_slide",&pyOpenSlide,
        py::arg("file_path"),
        py::arg("driver_id"),
        "Opens an image slide. Returns slide object.");
    m.def("get_driver_ids", &pyGetDriverIDs,
        "Returns list of driver ids");
    py::class_<PySlide, std::shared_ptr<PySlide>>(m, "Slide")
        .def("get_scene", &PySlide::getScene, py::arg("index"), "Returns a Scene object by index")
        .def_property_readonly("num_scenes", &PySlide::getNumScenes, "Number of scenes in the slide")
        .def_property_readonly("file_path", &PySlide::getFilePath, "File path to the image");
    py::class_<PyScene, std::shared_ptr<PyScene>>(m, "Scene")
        .def_property_readonly("name",&PyScene::getName, "Scene name")
        .def_property_readonly("resolution",&PyScene::getResolution, "Scene resolution in x and y directions")
        .def_property_readonly("z_resolution",&PyScene::getZSliceResolution, "Scene resolution in z direction")
        .def_property_readonly("t_resolution",&PyScene::getTFrameResolution, "Time frame resolution")
        .def_property_readonly("magnification",&PyScene::getMagnification, "Scanning magnification")
        .def_property_readonly("num_z_slices",&PyScene::getNumZSlices, "Number of slices in z direction")
        .def_property_readonly("num_t_frames",&PyScene::getNumTFrames, "Number of time frames")
        .def_property_readonly("file_path", &PyScene::getFilePath, "File path to the scene")
        .def_property_readonly("rect", &PyScene::getRect, "Scene rectangle")
        .def_property_readonly("num_channels", &PyScene::getNumChannels, "Number of channels in the scene")
        .def("get_channel_data_type", &PyScene::getChannelDataType, py::arg("index"), "Returns datatype of a channel by index")
        .def("get_channel_name", &PyScene::getChannelName, py::arg("index"), "Returns channel name (if any)")
        .def("read_block", &PyScene::readBlock, 
            py::arg("rect") = std::tuple<int,int,int,int>(0,0,0,0),
            py::arg("size")=std::tuple<int,int>(0,0),
            py::arg("channel_indices") = std::vector<int>(),
            py::arg("slices")=std::tuple<int,int>(0,1),
            py::arg("frames")=std::tuple<int,int>(0,1),
            "Reads rectangular block of the scene with rescaling (optional)");
}