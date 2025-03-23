// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/transformer.hpp"
#include "slideio/transformer/transformations.hpp"
#include "slideio/transformer/transformerscene.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/transformer/transformationtype.hpp"
#include "slideio/transformer/wrappers.hpp"
#include "slideio/transformer/filters.hpp"

using namespace slideio;

std::shared_ptr<Scene> slideio::transformScene(std::shared_ptr<slideio::Scene> scene, Transformation& transform)
{
    auto ptr = makeTransformationCopy(transform);
    std::list<std::shared_ptr<Transformation>> transforms;
    transforms.push_back(ptr);
    std::shared_ptr<CVScene> transformedCVScene(new TransformerScene(scene->getCVScene(), transforms));
    std::shared_ptr<Scene> transformerScene(new Scene(transformedCVScene));
    return transformerScene;
}

std::shared_ptr<Scene> slideio::transformSceneEx(std::shared_ptr<Scene> scene, std::list<std::shared_ptr<Transformation>>& transforms)
{
    std::shared_ptr<CVScene> transformedCVScene(new TransformerScene(scene->getCVScene(), transforms));
    std::shared_ptr<slideio::Scene> transformerScene(new Scene(transformedCVScene));
    return transformerScene;
}


static std::shared_ptr<Transformation> transformFromWrapper(std::shared_ptr<slideio::Scene> scene, TransformationWrapper* wrap) {
    switch (wrap->getType())
    {
    case TransformationType::ScharrFilter:
        return static_cast<ScharrFilterWrap*>(wrap)->getFilter();
    case TransformationType::SobelFilter:
        return static_cast<SobelFilterWrap*>(wrap)->getFilter();
    case TransformationType::GaussianBlurFilter:
        return static_cast<GaussianBlurFilterWrap*>(wrap)->getFilter();
    case TransformationType::MedianBlurFilter:
        return static_cast<MedianBlurFilterWrap*>(wrap)->getFilter();
    case TransformationType::BilateralFilter:
        return static_cast<BilateralFilterWrap*>(wrap)->getFilter();
    case TransformationType::CannyFilter:
        return static_cast<CannyFilterWrap*>(wrap)->getFilter();
    case TransformationType::LaplacianFilter:
        return static_cast<LaplacianFilterWrap*>(wrap)->getFilter();
    case TransformationType::ColorTransformation:
        return static_cast<ColorTransformationWrap*>(wrap)->getFilter();
    default:
        RAISE_RUNTIME_ERROR << "Unknown transformation type: " << wrap->getType();
    }
}

std::shared_ptr<Scene> slideio::transformScene(std::shared_ptr<Scene> scene, TransformationWrapper& wrapper)
{
    std::shared_ptr<Transformation> transform = transformFromWrapper(scene, &wrapper);
    std::list<std::shared_ptr<Transformation>> transforms;
    transforms.push_back(transform);
    return transformSceneEx(scene, transforms);
}

std::shared_ptr<Scene> slideio::transformSceneEx(std::shared_ptr<Scene> scene, std::list<std::shared_ptr<TransformationWrapper>>& wrappers)
{
    std::list<std::shared_ptr<Transformation>> transforms;
    for (auto& wrap : wrappers)
    {
        std::shared_ptr<Transformation> transform = transformFromWrapper(scene, wrap.get());
        transforms.push_back(transform);
    }
    return transformSceneEx(scene, transforms);
}