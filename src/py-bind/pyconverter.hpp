// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <memory>
#include <pybind11/pybind11.h>

namespace slideio
{
    class ConverterParameters;
}

class PyScene;

void pyConvertFile(std::shared_ptr<PyScene>& pyScene, slideio::ConverterParameters*  params, const std::string& filePath);
void pyConvertFileEx(std::shared_ptr<PyScene>& pyScene, slideio::ConverterParameters* params, const std::string& filePath, pybind11::function callback);
