// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Transformation and TransformationWrapper are public and subclassable. A subclass
// defined outside SlideIO that reports a built-in TransformationType is not that
// built-in class, and is rejected with a RuntimeError rather than downcast to it
// (issue #89).
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include "tests/testlib/testscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/slideio/sceneinternal.hpp"
#include "slideio/transformer/transformation.hpp"
#include "slideio/transformer/transformationtype.hpp"
#include "slideio/transformer/transformationwrapper.hpp"
#include "slideio/transformer/transformations.hpp"
#include "slideio/transformer/transformer.hpp"

using namespace slideio;

namespace
{
    const std::vector<TransformationType> builtInTypes{
        TransformationType::ColorTransformation,
        TransformationType::GaussianBlurFilter,
        TransformationType::MedianBlurFilter,
        TransformationType::SobelFilter,
        TransformationType::ScharrFilter,
        TransformationType::LaplacianFilter,
        TransformationType::BilateralFilter,
        TransformationType::CannyFilter,
        TransformationType::ColorManagement,
    };

    // The padding is zeroed and larger than any built-in class, so the unchecked
    // downcast this guards against reads zeros rather than past the object.
    class ImpostorTransformation : public Transformation
    {
    public:
        explicit ImpostorTransformation(TransformationType type) : m_type(type) {}
        TransformationType getType() const override { return m_type; }
    private:
        TransformationType m_type;
        char m_padding[4096] = {};
    };

    class ImpostorWrapper : public TransformationWrapper
    {
    public:
        explicit ImpostorWrapper(TransformationType type) : m_type(type) {}
        TransformationType getType() const override { return m_type; }
    private:
        TransformationType m_type;
        char m_padding[4096] = {};
    };

    std::shared_ptr<Scene> originScene()
    {
        auto scene = std::make_shared<TestScene>();
        scene->setRect(cv::Rect(0, 0, 100, 100));
        scene->setNumChannels(3);
        scene->setChannelDataType(DataType::DT_Byte);
        return SceneInternal::createScene(scene);
    }

    // The call raises slideio::RuntimeError saying the object is not the built-in
    // class its type names -- not some later, unrelated failure.
    template <typename Call>
    ::testing::AssertionResult rejectsImpostor(Call call)
    {
        try {
            call();
        }
        catch (const RuntimeError& error) {
            const std::string message = error.what();
            if (message.find("not the built-in") != std::string::npos) {
                return ::testing::AssertionSuccess();
            }
            return ::testing::AssertionFailure() << "RuntimeError for another reason: " << message;
        }
        catch (const std::exception& error) {
            return ::testing::AssertionFailure() << "threw something other than RuntimeError: " << error.what();
        }
        return ::testing::AssertionFailure() << "threw nothing";
    }
}

TEST(TransformationImpostor, makeTransformationCopyRejectsAnImpostor)
{
    for (TransformationType type : builtInTypes) {
        ImpostorTransformation impostor(type);
        EXPECT_TRUE(rejectsImpostor([&] { makeTransformationCopy(impostor); })) << type;
    }
}

TEST(TransformationImpostor, transformSceneRejectsAnImpostor)
{
    auto scene = originScene();
    for (TransformationType type : builtInTypes) {
        ImpostorTransformation impostor(type);
        EXPECT_TRUE(rejectsImpostor([&] { transformScene(scene, impostor); })) << type;
    }
}

TEST(TransformationImpostor, makeTransformationCopyRejectsAnImpostorWrapper)
{
    for (TransformationType type : builtInTypes) {
        ImpostorWrapper impostor(type);
        EXPECT_TRUE(rejectsImpostor([&] { makeTransformationCopy(impostor); })) << type;
    }
}

TEST(TransformationImpostor, transformSceneRejectsAnImpostorWrapper)
{
    auto scene = originScene();
    for (TransformationType type : builtInTypes) {
        ImpostorWrapper impostor(type);
        EXPECT_TRUE(rejectsImpostor([&] { transformScene(scene, impostor); })) << type;
    }
}

TEST(TransformationImpostor, transformationHasAVirtualDestructor)
{
    // Deleting a subclass through a Transformation* is otherwise undefined.
    EXPECT_TRUE(std::has_virtual_destructor<Transformation>::value);
}
