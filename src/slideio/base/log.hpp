// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/base/slideio_base_def.hpp"
#include <string>
#if defined(_MSC_VER)
#if !defined(NOMINMAX)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#if !defined(GLOG_NO_ABBREVIATED_SEVERITIES)
#define GLOG_NO_ABBREVIATED_SEVERITIES
#endif
#include <glog/stl_logging.h>
#include <glog/logging.h>



#define SLIDEIO_LOG LOG