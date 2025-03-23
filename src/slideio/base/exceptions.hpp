// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/base/slideio_base_def.hpp"
#include <iostream>
#include <sstream>
#include <string.h>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251 4275)
#endif

namespace slideio {
    struct SLIDEIO_BASE_EXPORTS RuntimeError : public std::exception {
        template <typename T>
        RuntimeError& operator << (T rhs) {
            m_innerStream << rhs;
            return *this;
        }
        RuntimeError() = default;
        RuntimeError(RuntimeError& rhs) {
            std::string message = rhs.m_innerStream.str();
            if(!m_shown) {
                log(message);
            }
            m_innerStream << message;
        }
        virtual const char* what() const noexcept {
            m_message = m_innerStream.str();
            return m_message.c_str();
        }
    private:
        void log(const std::string& message);
    private:
        std::stringstream m_innerStream;
        mutable std::string m_message;
        bool m_shown = false;
    };
}

#define RAISE_RUNTIME_ERROR throw slideio::RuntimeError() << __FILE__ << ":" << __LINE__ << ":"

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
