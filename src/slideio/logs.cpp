// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

namespace logging = boost::log;
namespace keywords = boost::log::keywords;

class InitLog
{
public:
    InitLog() {
        logging::register_simple_formatter_factory<logging::trivial::severity_level, char>("Severity");
        logging::core::get()->set_filter( 
            logging::trivial::severity >= logging::trivial::error
        );
        logging::add_common_attributes();
    }
};

InitLog logInit;