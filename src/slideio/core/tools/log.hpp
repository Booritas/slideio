// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <string>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/logger.hpp>


#define SLIDEIO_LOG(lvl) slideio::Log::getInstance()

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
        void setLogLevel(Level level);
        void setLogFilePath(const std::string& path);
        template<typename T>
        Log& operator << (const T& data)
        {
            m_stream << data;
            return *this;
        }
        // this is the type of std::cout
        typedef std::basic_ostream<char, std::char_traits<char> > CoutType;

        // this is the function signature of std::endl
        typedef CoutType& (*StandardEndLine)(CoutType&);

        // define an operator<< to take in std::endl
        Log& operator<<(StandardEndLine manip)
        {
            // call the function, but we cannot return it's value
            manip(std::cout);

            return *this;
        }    private:
        Log();
        std::ostream& m_stream;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
