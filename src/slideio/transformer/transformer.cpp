// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "transformer.hpp"

#include "colortransformerscene.hpp"
#include "convolutionfilterscene.hpp"
#include "slideio/slideio/scene.hpp"
#include "transformation.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;

std::shared_ptr<slideio::Scene> slideio::transformScene(std::shared_ptr<slideio::Scene> scene,
                                                        Transformation& transform)
{
    switch (transform.getType()) {
    case TransformationType::GaussianBlurFilter:
    case TransformationType::MedianBlurFilter:
    case TransformationType::SobelFilter:
    case TransformationType::ScharrFilter:
        {
            std::shared_ptr<CVScene> transformedCVScene(
                new ConvolutionFilterScene(scene->getCVScene(), transform));
            std::shared_ptr<slideio::Scene> transformedScene(new Scene(transformedCVScene));
            return transformedScene;
        }
    case TransformationType::ColorTransformation:
        {
            if (scene->getNumChannels() != 3
                || scene->getChannelDataType(0) != DataType::DT_Byte
                || scene->getChannelDataType(1) != DataType::DT_Byte
                || scene->getChannelDataType(2) != DataType::DT_Byte) {
                RAISE_RUNTIME_ERROR << "Color transformation is applicable only for RGB images";
            }
            std::shared_ptr<CVScene> transformedCVScene(
                new ColorTransformerScene(scene->getCVScene(), static_cast<ColorTransformation&>(transform)));
            std::shared_ptr<slideio::Scene> transformedScene(new Scene(transformedCVScene));
            return transformedScene;
        }
    }
    RAISE_RUNTIME_ERROR << "Unsupported transformation type " << (int)transform.getType() << ".";
    return nullptr;
}
