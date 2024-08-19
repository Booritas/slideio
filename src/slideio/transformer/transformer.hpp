// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include <memory>
#include <list>

namespace slideio
{
    class Scene;
    class Transformation;
    class TransformationWrapper;
    std::shared_ptr<slideio::Scene> SLIDEIO_TRANSFORMER_EXPORTS transformScene(std::shared_ptr<Scene> scene, Transformation& transform);
    std::shared_ptr<slideio::Scene> SLIDEIO_TRANSFORMER_EXPORTS transformSceneEx(std::shared_ptr<Scene> scene, std::list<std::shared_ptr<Transformation>>& transforms);
    std::shared_ptr<slideio::Scene> SLIDEIO_TRANSFORMER_EXPORTS transformScene(std::shared_ptr<Scene> scene, TransformationWrapper& transform);
    std::shared_ptr<slideio::Scene> SLIDEIO_TRANSFORMER_EXPORTS transformSceneEx(std::shared_ptr<Scene> scene, std::list<std::shared_ptr<TransformationWrapper>>& transforms);
}
