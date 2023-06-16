// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "transformations.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;

std::shared_ptr<Transformation> slideio::makeTransformationCopy(const Transformation& source)
{
    TransformationType type = source.getType();
    switch (type) {
    case TransformationType::GaussianBlurFilter:
        {
            GaussianBlurFilter& filter = (GaussianBlurFilter&)source;
            std::shared_ptr<Transformation> transformation(new GaussianBlurFilter(filter));
            return transformation;
        }
    case TransformationType::MedianBlurFilter:
        {
            MedianBlurFilter& filter = (MedianBlurFilter&)source;
            std::shared_ptr<Transformation> transformation(new MedianBlurFilter(filter));
            return transformation;
        }
    case TransformationType::SobelFilter:
        {
            SobelFilter& filter = (SobelFilter&)source;
            std::shared_ptr<Transformation> transformation(new SobelFilter(filter));
            return transformation;
        }
    case TransformationType::ScharrFilter:
        {
            ScharrFilter& filter = (ScharrFilter&)source;
            std::shared_ptr<Transformation> transformation(new ScharrFilter(filter));
            return transformation;
        }
    case TransformationType::LaplacianFilter:
        {
            LaplacianFilter& filter = (LaplacianFilter&)source;
            std::shared_ptr<Transformation> transformation(new LaplacianFilter(filter));
            return transformation;
        }
    case TransformationType::BilateralFilter:
        {
            BilateralFilter& filter = (BilateralFilter&)source;
            std::shared_ptr<Transformation> transformation(new BilateralFilter(filter));
            return transformation;
        }
    case TransformationType::CannyFilter:
        {
            CannyFilter& filter = (CannyFilter&)source;
            std::shared_ptr<Transformation> transformation(new CannyFilter(filter));
            return transformation;
        }
    case TransformationType::ColorTransformation:
        {
            ColorTransformation& filter = (ColorTransformation&)source;
            std::shared_ptr<Transformation> transformation(new ColorTransformation(filter));
            return transformation;
        }
    default:
        RAISE_RUNTIME_ERROR << "Unsupported transformation type " << (int)type << ".";
    }
}
