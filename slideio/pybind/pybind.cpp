#include "slideio/slideio.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

namespace py = pybind11;
using namespace slideio;

PYBIND11_MODULE(slideio, m) {
    m.doc() = "Reading of medical images";
    m.def("open_slide",&openSlide,py::arg("file_path"), py::arg("driver_id"));
    m.def("get_driver_ids", &getDriverIDs);
    py::class_<slideio::Slide, std::shared_ptr<Slide>>(m, "Slide")
        .def("get_scene", &Slide::getScene, py::arg("index"))
        .def_property_readonly("numb_scenes", &Slide::getNumbScenes)
        .def_property_readonly("file_path", &Slide::getFilePath);
    py::class_<slideio::Scene, std::shared_ptr<Scene>>(m, "Scene")
        .def_property_readonly("name",&Scene::getName)
        .def_property_readonly("file_path", &Scene::getFilePath)
        .def_property_readonly("rect", &Scene::getRect);
}