// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/slideio/scene.hpp"
#include "transformation.hpp"
#include "transformer_def.hpp"

namespace slideio
{
     std::shared_ptr<slideio::Scene> SLIDEIO_TRANSFORMER_EXPORTS transformScene(std::shared_ptr<slideio::Scene> scene, Transformation& transform);
}
