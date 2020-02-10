#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(slideio, m) {
    m.doc() = "Reading of medical images";
}