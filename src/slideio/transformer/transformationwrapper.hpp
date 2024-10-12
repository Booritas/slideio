// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

namespace slideio 
{
    enum class TransformationType;
    class TransformationWrapper
    {
    public:
        virtual TransformationType getType() const = 0;
        virtual ~TransformationWrapper() = default;
    };
}