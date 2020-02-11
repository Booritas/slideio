#include <pybind11/pybind11.h>
#include "slideio/slideio.hpp"

namespace py = pybind11;

PYBIND11_MODULE(slideio, m) {
    m.doc() = "Reading of medical images";
    m.def("open_slide",&slideio::openSlide);
    py::class_<slideio::Slide>(m, "Slide")
        .def("get_numb_scenes", &slideio::Slide::getNumbScenes)
        .def("get_file_path", &slideio::Slide::getFilePath);
}