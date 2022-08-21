// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#if defined(WIN32)
#if defined(SLIDEIO_ZVI_API)
#define SLIDEIO_ZVI_EXPORTS __declspec(dllexport)
#else
#define SLIDEIO_ZVI_EXPORTS __declspec(dllimport)
#endif
#else
#define SLIDEIO_ZVI_EXPORTS
#endif

