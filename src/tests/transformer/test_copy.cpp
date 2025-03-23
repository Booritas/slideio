#include <gtest/gtest.h>
#include "slideio/transformer/transformations.hpp"
#include "slideio/transformer/gaussianblurfilter.hpp"
#include "slideio/transformer/medianblurfilter.hpp"
#include "slideio/transformer/sobelfilter.hpp"
#include "slideio/transformer/scharrfilter.hpp"
#include "slideio/transformer/laplacianfilter.hpp"
#include "slideio/transformer/bilateralfilter.hpp"
#include "slideio/transformer/cannyfilter.hpp"
#include "slideio/transformer/colortransformation.hpp"
#include "slideio/transformer/wrappers.hpp"

using namespace slideio;

TEST(FilterCopy, MakeTransformationCopy_GaussianBlurFilter) {
    GaussianBlurFilter original;
    original.setKernelSizeX(15);
    original.setKernelSizeY(25);
    original.setSigmaX(1.5);
    original.setSigmaY(2.5);

    std::shared_ptr<Transformation> copy = makeTransformationCopy(original);
    GaussianBlurFilter* copiedFilter = dynamic_cast<GaussianBlurFilter*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getKernelSizeX(), original.getKernelSizeX());
    EXPECT_EQ(copiedFilter->getKernelSizeY(), original.getKernelSizeY());
    EXPECT_EQ(copiedFilter->getSigmaX(), original.getSigmaX());
    EXPECT_EQ(copiedFilter->getSigmaY(), original.getSigmaY());
}

TEST(FilterCopy, MakeTransformationCopy_MedianBlurFilter) {
    MedianBlurFilter original;
    original.setKernelSize(3);

    std::shared_ptr<Transformation> copy = makeTransformationCopy(original);
    MedianBlurFilter* copiedFilter = dynamic_cast<MedianBlurFilter*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getKernelSize(), original.getKernelSize());
}

TEST(FilterCopy, MakeTransformationCopy_SobelFilter) {
    SobelFilter original;
    original.setDx(1);
    original.setDy(0);
    original.setDelta(4);
    original.setScale(2);
    original.setDepth(DataType::DT_Float64);

    std::shared_ptr<Transformation> copy = makeTransformationCopy(original);
    SobelFilter* copiedFilter = dynamic_cast<SobelFilter*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getDx(), original.getDx());
    EXPECT_EQ(copiedFilter->getDy(), original.getDy());
    EXPECT_EQ(copiedFilter->getDelta(), original.getDelta());
    EXPECT_EQ(copiedFilter->getScale(), original.getScale());
    EXPECT_EQ(copiedFilter->getDepth(), original.getDepth());
}

TEST(FilterCopy, MakeTransformationCopy_ScharrFilter) {
    ScharrFilter original;
    original.setDx(1);
    original.setDy(0);
    original.setDepth(DataType::DT_Float64);
    original.setDelta(4);
    original.setScale(2);

    std::shared_ptr<Transformation> copy = makeTransformationCopy(original);
    ScharrFilter* copiedFilter = dynamic_cast<ScharrFilter*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getDx(), original.getDx());
    EXPECT_EQ(copiedFilter->getDy(), original.getDy());
    EXPECT_EQ(copiedFilter->getDelta(), original.getDelta());
    EXPECT_EQ(copiedFilter->getScale(), original.getScale());
    EXPECT_EQ(copiedFilter->getDepth(), original.getDepth());
}

TEST(FilterCopy, MakeTransformationCopy_LaplacianFilter) {
    LaplacianFilter original;
    original.setKernelSize(3);
    original.setDelta(4);
    original.setScale(2);
    original.setDepth(DataType::DT_Float64);


    std::shared_ptr<Transformation> copy = makeTransformationCopy(original);
    LaplacianFilter* copiedFilter = dynamic_cast<LaplacianFilter*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getKernelSize(), original.getKernelSize());
    EXPECT_EQ(copiedFilter->getDelta(), original.getDelta());
    EXPECT_EQ(copiedFilter->getScale(), original.getScale());
    EXPECT_EQ(copiedFilter->getDepth(), original.getDepth());
}

TEST(FilterCopy, MakeTransformationCopy_BilateralFilter) {
    BilateralFilter original;
    original.setDiameter(9);
    original.setSigmaColor(75);
    original.setSigmaSpace(75);

    std::shared_ptr<Transformation> copy = makeTransformationCopy(original);
    BilateralFilter* copiedFilter = dynamic_cast<BilateralFilter*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getDiameter(), original.getDiameter());
    EXPECT_EQ(copiedFilter->getSigmaColor(), original.getSigmaColor());
    EXPECT_EQ(copiedFilter->getSigmaSpace(), original.getSigmaSpace());
}

TEST(FilterCopy, MakeTransformationCopy_CannyFilter) {
    CannyFilter original;
    original.setThreshold1(50);
    original.setThreshold2(150);
    original.setApertureSize(3);
    original.setL2Gradient(true);
    

    std::shared_ptr<Transformation> copy = makeTransformationCopy(original);
    CannyFilter* copiedFilter = dynamic_cast<CannyFilter*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getThreshold1(), original.getThreshold1());
    EXPECT_EQ(copiedFilter->getThreshold2(), original.getThreshold2());
    EXPECT_EQ(copiedFilter->getApertureSize(), original.getApertureSize());
    EXPECT_EQ(copiedFilter->getL2Gradient(), original.getL2Gradient());
}

TEST(FilterCopy, MakeTransformationCopy_ColorTransformation) {
    ColorTransformation original;
    original.setColorSpace(ColorSpace::HLS);

    std::shared_ptr<Transformation> copy = makeTransformationCopy(original);
    ColorTransformation* copiedFilter = dynamic_cast<ColorTransformation*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getColorSpace(), original.getColorSpace());
}


TEST(FilterWrapperCopy, MakeTransformationCopy_GaussianBlurFilter) {
    GaussianBlurFilterWrap original;
    original.setKernelSizeX(15);
    original.setKernelSizeY(25);
    original.setSigmaX(1.5);
    original.setSigmaY(2.5);

    std::shared_ptr<TransformationWrapper> copy = makeTransformationCopy(original);
    GaussianBlurFilterWrap* copiedFilter = dynamic_cast<GaussianBlurFilterWrap*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getKernelSizeX(), original.getKernelSizeX());
    EXPECT_EQ(copiedFilter->getKernelSizeY(), original.getKernelSizeY());
    EXPECT_EQ(copiedFilter->getSigmaX(), original.getSigmaX());
    EXPECT_EQ(copiedFilter->getSigmaY(), original.getSigmaY());
}

TEST(FilterWrapperCopy, MakeTransformationCopy_MedianBlurFilter) {
    MedianBlurFilterWrap original;
    original.setKernelSize(3);

    std::shared_ptr<TransformationWrapper> copy = makeTransformationCopy(original);
    MedianBlurFilterWrap* copiedFilter = dynamic_cast<MedianBlurFilterWrap*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getKernelSize(), original.getKernelSize());
}

TEST(FilterWrapperCopy, MakeTransformationCopy_SobelFilter) {
    SobelFilterWrap original;
    original.setDx(1);
    original.setDy(0);
    original.setDelta(4);
    original.setScale(2);
    original.setDepth(DataType::DT_Float64);

    std::shared_ptr<TransformationWrapper> copy = makeTransformationCopy(original);
    SobelFilterWrap* copiedFilter = dynamic_cast<SobelFilterWrap*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getDx(), original.getDx());
    EXPECT_EQ(copiedFilter->getDy(), original.getDy());
    EXPECT_EQ(copiedFilter->getDelta(), original.getDelta());
    EXPECT_EQ(copiedFilter->getScale(), original.getScale());
    EXPECT_EQ(copiedFilter->getDepth(), original.getDepth());
}

TEST(FilterWrapperCopy, MakeTransformationCopy_ScharrFilter) {
    ScharrFilterWrap original;
    original.setDx(1);
    original.setDy(0);
    original.setDepth(DataType::DT_Float64);
    original.setDelta(4);
    original.setScale(2);

    std::shared_ptr<TransformationWrapper> copy = makeTransformationCopy(original);
    ScharrFilterWrap* copiedFilter = dynamic_cast<ScharrFilterWrap*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getDx(), original.getDx());
    EXPECT_EQ(copiedFilter->getDy(), original.getDy());
    EXPECT_EQ(copiedFilter->getDelta(), original.getDelta());
    EXPECT_EQ(copiedFilter->getScale(), original.getScale());
    EXPECT_EQ(copiedFilter->getDepth(), original.getDepth());
}

TEST(FilterWrapperCopy, MakeTransformationCopy_LaplacianFilter) {
    LaplacianFilterWrap original;
    original.setKernelSize(3);
    original.setDelta(4);
    original.setScale(2);
    original.setDepth(DataType::DT_Float64);


    std::shared_ptr<TransformationWrapper> copy = makeTransformationCopy(original);
    LaplacianFilterWrap* copiedFilter = dynamic_cast<LaplacianFilterWrap*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getKernelSize(), original.getKernelSize());
    EXPECT_EQ(copiedFilter->getDelta(), original.getDelta());
    EXPECT_EQ(copiedFilter->getScale(), original.getScale());
    EXPECT_EQ(copiedFilter->getDepth(), original.getDepth());
}

TEST(FilterWrapperCopy, MakeTransformationCopy_BilateralFilter) {
    BilateralFilterWrap original;
    original.setDiameter(9);
    original.setSigmaColor(75);
    original.setSigmaSpace(75);

    std::shared_ptr<TransformationWrapper> copy = makeTransformationCopy(original);
    BilateralFilterWrap* copiedFilter = dynamic_cast<BilateralFilterWrap*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getDiameter(), original.getDiameter());
    EXPECT_EQ(copiedFilter->getSigmaColor(), original.getSigmaColor());
    EXPECT_EQ(copiedFilter->getSigmaSpace(), original.getSigmaSpace());
}

TEST(FilterWrapperCopy, MakeTransformationCopy_CannyFilter) {
    CannyFilterWrap original;
    original.setThreshold1(50);
    original.setThreshold2(150);
    original.setApertureSize(3);
    original.setL2Gradient(true);


    std::shared_ptr<TransformationWrapper> copy = makeTransformationCopy(original);
    CannyFilterWrap* copiedFilter = dynamic_cast<CannyFilterWrap*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getThreshold1(), original.getThreshold1());
    EXPECT_EQ(copiedFilter->getThreshold2(), original.getThreshold2());
    EXPECT_EQ(copiedFilter->getApertureSize(), original.getApertureSize());
    EXPECT_EQ(copiedFilter->getL2Gradient(), original.getL2Gradient());
}

TEST(FilterWrapperCopy, MakeTransformationCopy_ColorTransformation) {
    ColorTransformationWrap original;
    original.setColorSpace(ColorSpace::HLS);

    std::shared_ptr<TransformationWrapper> copy = makeTransformationCopy(original);
    ColorTransformationWrap* copiedFilter = dynamic_cast<ColorTransformationWrap*>(copy.get());

    ASSERT_NE(copiedFilter, nullptr);
    EXPECT_EQ(copiedFilter->getColorSpace(), original.getColorSpace());
}
