// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/transformation.hpp"
#include "pyscene.hpp"
#include "pytransformation.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/transformer/transformer.hpp"
#include "slideio/transformer/transformations.hpp"

namespace py = pybind11;
using namespace slideio;


std::shared_ptr<PyScene> pyTransformScene(std::shared_ptr<PyScene>& pyScene, const pybind11::list& source)
{
    std::list<std::shared_ptr<Transformation>> transformations;
    for (const auto& obj : source) {
        if (!py::isinstance<Transformation>(obj)) {
            RAISE_RUNTIME_ERROR << "Invalid transformation object received in the transformation list";
        }
        const Transformation& transformation = py::cast<Transformation&>(obj);
        auto ptr = makeTransformationCopy(transformation);
        transformations.push_back(ptr);
    }
    if (transformations.empty()) {
        RAISE_RUNTIME_ERROR << "Empty transformation list received.";
    }
    std::shared_ptr<slideio::Scene> scene = extractScene(pyScene);
    std::list<std::shared_ptr<Transformation>> params;
    std::shared_ptr<Scene> transformScene = slideio::transformSceneEx(scene, transformations);
    std::shared_ptr<PyScene> wrapper(new PyScene(transformScene, nullptr));
    return wrapper;
}
