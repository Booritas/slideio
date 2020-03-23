// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#include "slideio/core/cvslide.hpp"
#include "slideio/core/imagedrivermanager.hpp"
#include "slideio/core/cvglobals.hpp"

#include <string>

using namespace slideio;

std::shared_ptr<CVSlide> slideio::cvOpenSlide(const std::string& filePath, const std::string& driver)
{
    return ImageDriverManager::openSlide(filePath, driver);
}

std::vector<std::string> slideio::cvGetDriverIDs()
{
    return ImageDriverManager::getDriverIDs();
}

int slideio::cvGetDataTypeSize(DataType dt)
{
    switch(dt)
    {
    case DataType::DT_Byte: return 1;
    case DataType::DT_Int8: return 1;
    case DataType::DT_Int16: return 2;
    case DataType::DT_Float16: return 2;
    case DataType::DT_Int32: return 4;
    case DataType::DT_Float32: return 4;
    case DataType::DT_Float64: return 8;
    case DataType::DT_UInt16: return 2;
    }
    throw std::runtime_error("Unsupported data type.");
}
