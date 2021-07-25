// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/slideio_def.hpp"
#include <string>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/trivial.hpp>

#define SLIDEIO_LOG BOOST_LOG_TRIVIAL

namespace slideio {
    class SLIDEIO_EXPORTS Log
    {
    public:
        Log();
        enum class Level
        {
            trace,
            debug,
            info,
            warning,
            error,
            fatal
        };
        static void setLogLevel(Level level);
        static void setLogFilePath(const std::string& path);
    };
}
