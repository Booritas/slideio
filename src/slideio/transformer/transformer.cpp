// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "transformer.hpp"
#include "slideio/slideio/scene.hpp"
#include "transformations.hpp"
#include "transformerscene.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;

std::shared_ptr<slideio::Scene> slideio::transformScene(std::shared_ptr<slideio::Scene> scene, Transformation& transform)
{
    auto ptr = makeTransformationCopy(transform);
    std::list<std::shared_ptr<Transformation>> transforms;
    transforms.push_back(ptr);
    std::shared_ptr<CVScene> transformedCVScene(new TransformerScene(scene->getCVScene(), transforms));
    std::shared_ptr<slideio::Scene> transformerScene(new Scene(transformedCVScene));
    return transformerScene;
}

std::shared_ptr<Scene> slideio::transformSceneEx(std::shared_ptr<Scene> scene, std::list<std::shared_ptr<Transformation>>& transforms)
{
    std::shared_ptr<CVScene> transformedCVScene(new TransformerScene(scene->getCVScene(), transforms));
    std::shared_ptr<slideio::Scene> transformerScene(new Scene(transformedCVScene));
    return transformerScene;
}
