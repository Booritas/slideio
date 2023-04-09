// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "pyscene.hpp"
#include "pyconverter.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/converter/converter.hpp"
#include "slideio/converter/converterparameters.hpp"

namespace py = pybind11;

using namespace slideio;
ConverterParameters* pyCreateConverterParameters(ImageFormat format, Compression encoding)
{
    ConverterParameters* params(nullptr);
    if (format == ImageFormat::SVS) {
        if (encoding == Compression::Jpeg) {
            params = new SVSJpegConverterParameters;
        }
        else if (encoding == Compression::Jpeg2000) {
            params = new SVSJp2KConverterParameters;
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

void pyConvertFile(std::shared_ptr<PyScene>& pyScene, ConverterParameters* parameters, const std::string& filePath)
{
    std::shared_ptr<slideio::Scene> scene = extractScene(pyScene);
    slideio::convertScene(scene, *parameters, filePath);
}

void pyConvertFileEx(std::shared_ptr<PyScene>& pyScene, ConverterParameters* parameters, const std::string& filePath, py::function callback)
{
    const std::function<void(int)>& cb = callback;
    std::shared_ptr<slideio::Scene> scene = extractScene(pyScene);
    slideio::convertScene(scene, *parameters, filePath, cb);
}
