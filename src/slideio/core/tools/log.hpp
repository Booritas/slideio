// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <string>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/logger.hpp>


#define SLIDEIO_LOG(lvl)\
    BOOST_LOG_STREAM_WITH_PARAMS(Log::getInstance().getLog(),\
        (::boost::log::keywords::severity = ::boost::log::trivial::lvl))

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio {
    class SLIDEIO_CORE_EXPORTS Log
    {
    public:
        enum class Level
        {
            trace,
            debug,
            info,
            warning,
            error,
            fatal
        };
        static Log& getInstance();
        Log(Log const&) = delete;
        void operator = (Log const&) = delete;
        boost::log::sources::logger& getLog() {
            return m_logger;
        }
        void setLogLevel(Level level);
        void setLogFilePath(const std::string& path);
    private:
        Log();
        boost::log::sources::logger m_logger;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
