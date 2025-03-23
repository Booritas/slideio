// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "transformations.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/transformer/colortransformation.hpp"
#include "slideio/transformer/gaussianblurfilter.hpp"
#include "slideio/transformer/laplacianfilter.hpp"
#include "slideio/transformer/medianblurfilter.hpp"
#include "slideio/transformer/scharrfilter.hpp"
#include "slideio/transformer/sobelfilter.hpp"
#include "slideio/transformer/cannyfilter.hpp"
#include "slideio/transformer/bilateralfilter.hpp"
#include "slideio/transformer/wrappers.hpp"

using namespace slideio;

std::shared_ptr<Transformation> slideio::makeTransformationCopy(const Transformation& source)
{
    TransformationType type = source.getType();
    switch (type) {
    case TransformationType::GaussianBlurFilter:
        {
            GaussianBlurFilter& filter = (GaussianBlurFilter&)source;
            std::shared_ptr<TransformationEx> transformation(new GaussianBlurFilter(filter));
            return transformation;
        }
    case TransformationType::MedianBlurFilter:
        {
            MedianBlurFilter& filter = (MedianBlurFilter&)source;
            std::shared_ptr<TransformationEx> transformation(new MedianBlurFilter(filter));
            return transformation;
        }
    case TransformationType::SobelFilter:
        {
            SobelFilter& filter = (SobelFilter&)source;
            std::shared_ptr<TransformationEx> transformation(new SobelFilter(filter));
            return transformation;
        }
    case TransformationType::ScharrFilter:
        {
            ScharrFilter& filter = (ScharrFilter&)source;
            std::shared_ptr<TransformationEx> transformation(new ScharrFilter(filter));
            return transformation;
        }
    case TransformationType::LaplacianFilter:
        {
            LaplacianFilter& filter = (LaplacianFilter&)source;
            std::shared_ptr<TransformationEx> transformation(new LaplacianFilter(filter));
            return transformation;
        }
    case TransformationType::BilateralFilter:
        {
            BilateralFilter& filter = (BilateralFilter&)source;
            std::shared_ptr<TransformationEx> transformation(new BilateralFilter(filter));
            return transformation;
        }
    case TransformationType::CannyFilter:
        {
            CannyFilter& filter = (CannyFilter&)source;
            std::shared_ptr<TransformationEx> transformation(new CannyFilter(filter));
            return transformation;
        }
    case TransformationType::ColorTransformation:
        {
            ColorTransformation& filter = (ColorTransformation&)source;
            std::shared_ptr<TransformationEx> transformation(new ColorTransformation(filter));
            return transformation;
        }
    default:
        RAISE_RUNTIME_ERROR << "Unsupported transformation type " << (int)type << ".";
    }
}

std::shared_ptr<TransformationWrapper> slideio::makeTransformationCopy(const TransformationWrapper& source) {
    TransformationType type = source.getType();
    switch (type) {
    case TransformationType::GaussianBlurFilter:
        {
            const GaussianBlurFilterWrap& filter = (const GaussianBlurFilterWrap&)source;
            std::shared_ptr<TransformationWrapper> transformation(new GaussianBlurFilterWrap(filter));
            return transformation;
        }
    case TransformationType::MedianBlurFilter:
        {
            const MedianBlurFilterWrap& filter = (const MedianBlurFilterWrap&)source;
            std::shared_ptr<TransformationWrapper> transformation(new MedianBlurFilterWrap(filter));
            return transformation;
        }
    case TransformationType::SobelFilter:
        {
            const SobelFilterWrap& filter = (const SobelFilterWrap&)source;
            std::shared_ptr<TransformationWrapper> transformation(new SobelFilterWrap(filter));
            return transformation;
        }
    case TransformationType::ScharrFilter:
        {
            const ScharrFilterWrap& filter = (const ScharrFilterWrap&)source;
            std::shared_ptr<TransformationWrapper> transformation(new ScharrFilterWrap(filter));
            return transformation;
        }
    case TransformationType::LaplacianFilter:
        {
            const LaplacianFilterWrap& filter = (const LaplacianFilterWrap&)source;
            std::shared_ptr<TransformationWrapper> transformation(new LaplacianFilterWrap(filter));
            return transformation;
        }
    case TransformationType::BilateralFilter:
        {
            const BilateralFilterWrap& filter = (const BilateralFilterWrap&)source;
            std::shared_ptr<TransformationWrapper> transformation(new BilateralFilterWrap(filter));
            return transformation;
        }
    case TransformationType::CannyFilter:
        {
            const CannyFilterWrap& filter = (const CannyFilterWrap&)source;
            std::shared_ptr<TransformationWrapper> transformation(new CannyFilterWrap(filter));
            return transformation;
        }
    case TransformationType::ColorTransformation:
        {
            const ColorTransformationWrap& filter = (const ColorTransformationWrap&)source;
            std::shared_ptr<TransformationWrapper> transformation(new ColorTransformationWrap(filter));
            return transformation;
        }
    default:
        RAISE_RUNTIME_ERROR << "Unsupported transformation type " << (int)type << ".";
    }
}
