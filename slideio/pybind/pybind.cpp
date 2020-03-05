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
        .def("get_scene", &PySlide::getScene, py::arg("index"))
        .def_property_readonly("numb_scenes", &PySlide::getNumbScenes)
        .def_property_readonly("file_path", &PySlide::getFilePath);
    py::class_<PyScene, std::shared_ptr<PyScene>>(m, "Scene")
        .def_property_readonly("name",&PyScene::getName)
        .def_property_readonly("file_path", &PyScene::getFilePath)
        .def_property_readonly("rect", &PyScene::getRect)
        .def("get_channel_data_type", &PyScene::getChannelDataType, py::arg("index"))
        .def("read_block", &PyScene::readBlock, 
            py::arg("rect") = std::tuple<int,int,int,int>(0,0,0,0),
            py::arg("size")=std::tuple<int,int>(0,0),
            py::arg("channel_indices") = std::vector<int>(),
            py::arg("slices")=std::tuple<int,int>(0,1),
            py::arg("frames")=std::tuple<int,int>(0,1));
}