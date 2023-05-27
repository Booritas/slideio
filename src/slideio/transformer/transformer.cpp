// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "transformer.hpp"

#include "colortransformerscene.hpp"
#include "slideio/slideio/scene.hpp"
#include "transformation.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;

std::shared_ptr<slideio::Scene> slideio::transformScene(std::shared_ptr<slideio::Scene> scene, Transformation& transform)\
{
    if (transform.getType() == TransformationType::ColorTransformation) {
        if (scene->getNumChannels() != 3
            || scene->getChannelDataType(0) != DataType::DT_Byte
            || scene->getChannelDataType(1) != DataType::DT_Byte
            || scene->getChannelDataType(2) != DataType::DT_Byte) {
            RAISE_RUNTIME_ERROR << "Color transformation is applicable only for RGB images";
        }
        std::shared_ptr<CVScene> transformedCVScene(new ColorTransformerScene(scene->getCVScene(), static_cast<ColorTransformation&>(transform)));
        std::shared_ptr<slideio::Scene> transformedScene(new Scene(transformedCVScene));
        return transformedScene;
    }
    return nullptr;
}
