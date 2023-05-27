// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "pyscene.hpp"
#include "pytransformation.hpp"
#include "slideio/transformer/transformer.hpp"
#include "slideio/transformer/transformation.hpp"

namespace py = pybind11;

using namespace slideio;

std::shared_ptr<PyScene> pyTransformScene(std::shared_ptr<PyScene>& pyScene,  slideio::Transformation*  params)
{
    std::shared_ptr<slideio::Scene> scene = extractScene(pyScene);
    std::shared_ptr<Scene> transformScene = slideio::transformScene(scene, *params);
    std::shared_ptr<PyScene> wrapper(new PyScene(transformScene, nullptr));
    return wrapper;
}
