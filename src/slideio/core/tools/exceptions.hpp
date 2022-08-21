// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/tools/log.hpp"

#include <iostream>
#include <sstream>
#include <string.h>


namespace slideio {
    struct RuntimeError : public std::exception {
        template <typename T>
        RuntimeError& operator << (T rhs) {
            m_innerStream << rhs;
            return *this;
        }
        RuntimeError() = default;
        RuntimeError(RuntimeError& rhs) {
            std::string message = rhs.m_innerStream.str();
            if(!m_shown) {
                SLIDEIO_LOG(error) << message;
            }
            m_innerStream << message;
        }
        virtual const char* what() const noexcept {
            m_message = m_innerStream.str();
            return m_message.c_str();
        }
    private:
        std::stringstream m_innerStream;
        mutable std::string m_message;
        bool m_shown = false;
    };
}

#define RAISE_RUNTIME_ERROR throw slideio::RuntimeError() << __FILE__ << ":" << __LINE__ << ":"