// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <functional>

namespace slideio
{
    namespace converter
    {
        typedef const std::function<void(int)>& ConverterCallback;
    }
}
