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

        // Reported for a dimension whose order the file has not stated. It cannot be
        // 0: that is the order the format gives X, so a zero default would make an
        // unrecorded dimension indistinguishable from the first real one.
        const int UNSET_DIMENSION_ORDER = -1;

        class IDimensionOrder
        {
        public:
            virtual int getDimensionOrder(vsi::Dimensions dim) const = 0;
        };
    }

}   