﻿// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/czi/cziimagedriver.hpp"
#include "slideio/drivers/czi/czislide.hpp"
#include "slideio/core/tools/tools.hpp"

using namespace slideio;

std::string CZIImageDriver::filePathPattern = "*.czi";

CZIImageDriver::CZIImageDriver()
{
}

CZIImageDriver::~CZIImageDriver()
{
}

std::string CZIImageDriver::getID() const
{
    return "CZI";
}

std::shared_ptr<CVSlide> CZIImageDriver::openFile(const std::string& filePath)
{
    Tools::throwIfPathNotExist(filePath, "CZIImageDriver::openFile");
    std::shared_ptr<CVSlide> ptr(new CZISlide(filePath));
    return ptr;
}

std::string CZIImageDriver::getFileSpecs() const
{
    static std::string pattern("*.czi");
    return pattern;
}
