// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Compile-only guard for design spec section 4.5.1. <wingdi.h> defines ERROR as
// 0. If SLIDEIO_LOG ever stops token-pasting its severity, the uses below stop
// compiling. That is the entire point of this file - it is cheap, and it pins
// the one property of the macro that is easy to refactor away by accident.
#include <gtest/gtest.h>

#include <list>
#include <string>
#include <vector>

// Emulate the windows.h / wingdi.h macro set, deliberately BEFORE log.hpp.
#define ERROR 0
#define INFO 0
#define WARNING 0

#include "slideio/core/log.hpp"

TEST(LogMacroCollision, compilesWithWindowsSeverityMacrosDefined)
{
    // These must compile even though ERROR, INFO and WARNING are macros.
    SLIDEIO_LOG(INFO) << "info under macro pressure";
    SLIDEIO_LOG(WARNING) << "warning under macro pressure";
    SLIDEIO_LOG(ERROR) << "error under macro pressure";
    SUCCEED();
}

// Guards the lexical-precedence constraint on the container operator<< overloads
// in log.hpp: they are not ADL-reachable (their argument types live in std), so
// they only work while they appear before LogStream in that header. If someone
// moves them into a separately-included header, this stops compiling.
TEST(LogMacroCollision, compilesWithContainerStreaming)
{
    const std::vector<std::string> vectorOfStrings{"alpha", "beta"};
    const std::list<std::string> listOfStrings{"gamma", "delta"};
    SLIDEIO_LOG(INFO) << vectorOfStrings;
    SLIDEIO_LOG(INFO) << listOfStrings;
    SUCCEED();
}

#undef ERROR
#undef INFO
#undef WARNING
