// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/transformer/transformer_def.hpp"
#include <memory>

namespace slideio
{
	class Transformation;
	class TransformationWrapper;
	std::shared_ptr<Transformation> SLIDEIO_TRANSFORMER_EXPORTS makeTransformationCopy(const Transformation& source);
	std::shared_ptr<TransformationWrapper> SLIDEIO_TRANSFORMER_EXPORTS makeTransformationCopy(const TransformationWrapper& source);
}
