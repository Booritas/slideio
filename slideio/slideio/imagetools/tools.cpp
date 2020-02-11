// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include "slideio/imagetools/tools.hpp"
#if defined(WIN32)
#include <Shlwapi.h>
#else
#include <fnmatch.h>
#endif
using namespace slideio;

bool Tools::matchPattern(const std::string& path, const std::string& pattern)
{
    //TODO:: Implement Linux version for fnmatch
    bool ret(false);
#if defined(WIN32)
    ret = PathMatchSpecA(path.c_str(), pattern.c_str())!=0;
#else
    ret = fnmatch(pattern.c_str(), path.c_str(), FNM_NOESCAPE)==0;
#endif

    return ret;
}
