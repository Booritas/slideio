// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "colortransformation.hpp"
#include "gaussianblurfilter.hpp"
#include "laplacianfilter.hpp"
#include "medianblurfilter.hpp"
#include "scharrfilter.hpp"
#include "sobelfilter.hpp"
#include "laplacianfilter.hpp"
#include "cannyfilter.hpp"
#include "bilateralfilter.hpp"

namespace slideio
{
	std::shared_ptr<slideio::Transformation> SLIDEIO_TRANSFORMER_EXPORTS makeTransformationCopy(const Transformation& source);
}
