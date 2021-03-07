// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmfile.hpp"
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <boost/format.hpp>

using namespace slideio;

DCMFile::DCMFile(const std::string& filePath):
                                m_filePath(filePath)
{
    m_file.reset(new DcmFileFormat);
}

void DCMFile::init()
{
    OFCondition status = m_file->loadFile(m_filePath.c_str());
    if (status.bad()) {
        throw std::runtime_error(std::string("DCMImageDriver: Cannot open file:") + m_filePath);
    }
    DcmDataset* dataset = getValidDataset();

    if(!getIntTag(DCM_Columns, m_width)) {
        throw std::runtime_error(std::string("DCMImageDriver: Cannot extract image width for the file:") + m_filePath);
    }
    if (!getIntTag(DCM_Rows, m_height)) {
        throw std::runtime_error(std::string("DCMImageDriver: Cannot extract image height for the file:") + m_filePath);
    }
    if (!getIntTag(DCM_NumberOfFrames, m_slices)) {
        m_slices = 1;
    }
    if(!getStringTag(DCM_SeriesInstanceUID, m_seriesUID)) {
        m_seriesUID = "Unknown series";
    }
    if(!getIntTag(DCM_InstanceNumber, m_instanceNumber)) {
        m_instanceNumber = -1;
    }
    if (!getIntTag(DCM_SamplesPerPixel, m_numChannels)) {
        m_numChannels = 1;
    }
}

DcmDataset* DCMFile::getDataset() const
{
    return m_file ? m_file->getDataset() : nullptr;
}

DcmDataset* DCMFile::getValidDataset() const
{
    if(!m_file) {
        throw std::runtime_error(
            (boost::format("DCMImageDriver: uninitialized DICOM file: %1%") % m_filePath).str());
    }
    DcmDataset* dataset = m_file->getDataset();
    if (!dataset) {
        throw std::runtime_error(
            (boost::format("DCMImageDriver: cannot retrieve DICOM dataset from file: %1%") % m_filePath).str());
    }
    return dataset;
}

bool DCMFile::getIntTag(const DcmTagKey& tag, int& value, int pos) const
{
    bool ok(false);
    DcmElement* element = nullptr;
    DcmDataset* dataset =getValidDataset();

    OFCondition cond = dataset->findAndGetElement(tag, element);
    if (cond != EC_Normal) {
        element = nullptr;
    }

    if (nullptr == element) {
        value = -1;
        return false;
    }

    Uint16 valueU16;
    if (EC_Normal == element->getUint16(valueU16, pos)) {
        value = valueU16;
        return true;
    }

    Sint16 valueS16;
    if (EC_Normal == element->getSint16(valueS16, pos)) {
        value = valueS16;
        return true;
    }

    Uint32 valueU32;
    if (EC_Normal == element->getUint32(valueU32, pos)) {
        value = valueU32;
        return true;
    }

    Sint32 valueS32;
    if (EC_Normal == element->getSint32(valueS32, pos)) {
        value = valueS32;
        return true;
    }

    value = -1;
    return false;
}

bool DCMFile::getStringTag(const DcmTagKey& tag, std::string& value) const
{
    bool ok(false);
    bool bRet(false);
    DcmDataset* dataset = getValidDataset();
    OFString dstrVal;
    if (dataset->findAndGetOFString(tag, dstrVal).good()) {
        value = dstrVal.c_str();
        ok = true;
    }
    return ok;
}
