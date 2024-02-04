// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
namespace slideio
{
    namespace vsi
    {
        enum class Dimensions
        {
            X = 0,
            Y = 1,
            Z = 2,
            C = 3,
            T = 4,
            L = 5,
            P = 6
        };
        const int MAX_DIMENSIONS = 7;
    }
}   