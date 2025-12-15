// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/converter/tiffstructure.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;
using namespace slideio::converter;

const TiffDirectoryStructure& TiffPageStructure::getSubDirectory(int index) const {
    if (index >= static_cast<int>(m_subDirectories.size())) {
        RAISE_RUNTIME_ERROR << "TiffPageStructure: subdirectory index out of range!";
    }
    return m_subDirectories.at(index);
}

TiffDirectoryStructure& TiffPageStructure::getSubDirectory(int index) {
    if (index >= static_cast<int>(m_subDirectories.size())) {
        RAISE_RUNTIME_ERROR << "TiffPageStructure: subdirectory index out of range!";
    }
    return m_subDirectories.at(index);
}
