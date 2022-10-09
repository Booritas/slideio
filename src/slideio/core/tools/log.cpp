// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "log.hpp"

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

using namespace slideio;
namespace logging = boost::log;
namespace keywords = boost::log::keywords;


#ifdef _DEBUG
const logging::trivial::severity_level defaultLogLevel = logging::trivial::warning;
#else
const logging::trivial::severity_level defaultLogLevel = logging::trivial::error;
#endif


static logging::trivial::severity_level sioToBoostLogLevel(Log::Level sioLevel)
{
    switch(sioLevel) {

    case Log::Level::trace: return logging::trivial::severity_level::trace;
    case Log::Level::debug: return logging::trivial::severity_level::debug;
    case Log::Level::info: return logging::trivial::severity_level::info;
    case Log::Level::warning: return logging::trivial::severity_level::warning;
    case Log::Level::error: return logging::trivial::severity_level::error;
    case Log::Level::fatal: return logging::trivial::severity_level::fatal;
    default: ;
        return logging::trivial::severity_level::fatal;
    }
    return logging::trivial::severity_level::fatal;
}

Log::Log()
{
    logging::register_simple_formatter_factory<logging::trivial::severity_level, char>("Severity");
    logging::core::get()->set_filter(
        logging::trivial::severity >= defaultLogLevel
    );
    logging::add_common_attributes();
}

Log& Log::getInstance()
{
    static Log instance;
    return instance;
}

void Log::setLogLevel(Level level)
{
    logging::register_simple_formatter_factory<logging::trivial::severity_level, char>("Severity");
    logging::core::get()->set_filter(
        logging::trivial::severity >= sioToBoostLogLevel(level)
    );
}

void Log::setLogFilePath(const std::string& path)
{
    logging::add_file_log
    (
        keywords::file_name = path,
        keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%"
    );
}
