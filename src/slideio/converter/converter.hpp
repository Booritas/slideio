// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_converter_HPP
#define OPENCV_slideio_converter_HPP

#include <map>

#include "convertercallback.hpp"
#include "slideio/converter/converter_def.hpp"
#include "slideio/slideio/scene.hpp"

const std::string DRIVER = "DRIVER";

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class ConverterParameters;
    void SLIDEIO_CONVERTER_EXPORTS convertScene(std::shared_ptr<slideio::Scene> inputScene,
                                                ConverterParameters& parameters,
                                                const std::string& outputPath,
                                                ConverterCallback cb=nullptr);
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif