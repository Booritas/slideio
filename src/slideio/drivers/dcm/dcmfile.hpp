// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_dcmsfil_HPP
#define OPENCV_slideio_dcmfile_HPP
#include "slideio/slideio_def.hpp"
#include <string>
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

class DcmDataset;
class DcmFileFormat;
class DcmTagKey;

namespace slideio
{
    class SLIDEIO_EXPORTS DCMFile
    {
    public:
        DCMFile(const std::string& filePath);
        void init();
        int getWidth() const {
            return m_width;
        }
        int getHeight() const {
            return m_height;
        }
        int getNumSlices() const {
            return m_slices;
        }
        const std::string getFilePath() const {
            return m_filePath;
        }
        const std::string& getSeriesUID() const{
            return m_seriesUID;
        }
        int getInstanceNumber() const {
            return m_instanceNumber;
        }
        int getNumChannels() const {
            return m_numChannels;
        }
        const std::string& getSeriesDescription() const {
            return m_seriesDescription;
        }
    private:
        DcmDataset* getDataset() const;
        DcmDataset* getValidDataset() const;
        bool getIntTag(const DcmTagKey& tag, int& value, int pos = 0) const;
        bool getStringTag(const DcmTagKey& tag, std::string& value) const;
    private:
        std::string m_filePath;
        std::shared_ptr<DcmFileFormat> m_file;
        int m_width = 0;
        int m_height = 0;
        int m_slices = 1;
        int m_instanceNumber = -1;
        std::string m_seriesUID;
        std::string m_seriesDescription;
        int m_numChannels = 0;
    };
}

#endif