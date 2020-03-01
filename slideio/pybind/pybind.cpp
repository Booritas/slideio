#include "slideio/slideio.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

namespace py = pybind11;

PYBIND11_MODULE(slideio, m) {
    m.doc() = "Reading of medical images";
    m.def("open_slide",&slideio::openSlide,py::arg("file_path"), py::arg("driver_id"));
    m.def("get_driver_ids", &slideio::getDriverIDs);
    py::class_<slideio::Slide>(m, "Slide")
        .def("get_numb_scenes", &slideio::Slide::getNumbScenes)
        .def("get_file_path", &slideio::Slide::getFilePath);
}