// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "pyscene.hpp"
#include "pyconverter.hpp"
#include "slideio/base/exceptions.hpp"

std::shared_ptr<PyConverterParameters> createConverterParameters(ConverterFormat format, ConverterEncoding encoding)
{
    std::shared_ptr<PyConverterParameters> params;
    if (format == ConverterFormat::SVS) {
        if (encoding == ConverterEncoding::JPEG) {
            params.reset(new PySVSJpegConverterParameters);
        }
        else if (encoding == ConverterEncoding::JPEG2000) {
            params.reset(new PySVSJp2KConverterParameters);
        }
        else {
            RAISE_RUNTIME_ERROR << "Unknown encoding for SVS m_format: " << (int)encoding;
        }
    }
    else {
        RAISE_RUNTIME_ERROR << "Unknown m_format: " << (int)format;
    }
    return params;
}

void pyConvertFile(std::shared_ptr<PyScene>& pyScene, std::shared_ptr<PyConverterParameters>& parameters, const std::string& filePath)
{
    std::shared_ptr<slideio::Scene> scene = extractScene(pyScene);
    //slideio::convertScene(scene, parameters, filePath);
}
