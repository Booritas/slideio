// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/transformationtype.hpp"
#include <ostream>
using namespace slideio;

std::ostream& slideio::operator << (std::ostream& os, TransformationType type) {
    switch (type) {
    case TransformationType::Unknown:
        os << "Unknown";
        break;
    case TransformationType::ColorTransformation:
        os << "ColorTransformation";
        break;
    case TransformationType::GaussianBlurFilter:
        os << "GaussianBlurFilter";
        break;
    case TransformationType::MedianBlurFilter:
        os << "MedianBlurFilter";
        break;
    case TransformationType::SobelFilter:
        os << "SobelFilter";
        break;
    case TransformationType::ScharrFilter:
        os << "ScharrFilter";
        break;
    case TransformationType::LaplacianFilter:
        os << "LaplacianFilter";
        break;
    case TransformationType::BilateralFilter:
        os << "BilateralFilter";
        break;
    case TransformationType::CannyFilter:
        os << "CannyFilter";
        break;
    default:
        os << "Unknown";
        break;
    }
    return os;
}
