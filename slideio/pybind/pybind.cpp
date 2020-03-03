#include "SlideW.h"
#include "globalW.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

namespace py = pybind11;

PYBIND11_MODULE(slideio, m) {
    m.doc() = "Reading of medical images";
    m.def("open_slide",&openSlideW,
        py::arg("file_path"),
        py::arg("driver_id"),
        "Opens an image slide. Returns slide object.");
    m.def("get_driver_ids", &getDriverIDsW,
        "Returns list of driver ids");
    py::class_<SlideW, std::shared_ptr<SlideW>>(m, "Slide")
        .def("get_scene", &SlideW::getScene, py::arg("index"))
        .def_property_readonly("numb_scenes", &SlideW::getNumbScenes)
        .def_property_readonly("file_path", &SlideW::getFilePath);
    py::class_<SceneW, std::shared_ptr<SceneW>>(m, "Scene")
        .def_property_readonly("name",&SceneW::getName)
        .def_property_readonly("file_path", &SceneW::getFilePath)
        .def_property_readonly("rect", &SceneW::getRect)
        .def("get_channel_data_type", &SceneW::getChannelDataType, py::arg("index"))
        .def("read_block", &SceneW::readBlock, 
            py::arg("rect") = std::tuple<int,int,int,int>(0,0,0,0),
            py::arg("size")=std::tuple<int,int>(0,0),
            py::arg("channel_indices") = std::vector<int>(),
            py::arg("slices")=std::tuple<int,int>(0,1),
            py::arg("frames")=std::tuple<int,int>(0,1));
}