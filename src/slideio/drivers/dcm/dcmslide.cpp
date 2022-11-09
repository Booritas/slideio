// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmslide.hpp"
#include "slideio/drivers/dcm/dcmscene.hpp"
#include "slideio/core/base.hpp"
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>

#include <dcmdata/dcdeftag.h>
#include <dcmdata/dcmetinf.h>

#include <dcmtk/dcmdata/dcdicdir.h>

using namespace slideio;
namespace fs = boost::filesystem;

struct Series
{
    std::vector<std::shared_ptr<DCMFile>> files;
};

DCMSlide::DCMSlide(const std::string& filePath) : m_srcPath(filePath)
{
    SLIDEIO_LOG(INFO) << "DCMSlide::constructor-begin: " << m_srcPath;
    init();
    SLIDEIO_LOG(INFO) << "DCMSlide::constructor-end: " << m_srcPath;
}

int DCMSlide::getNumScenes() const
{
    return static_cast<int>(m_scenes.size());
}

std::string DCMSlide::getFilePath() const
{
    return m_srcPath;
}

std::shared_ptr<CVScene> DCMSlide::getScene(int index) const
{
    return m_scenes[index];
}

void DCMSlide::initFromFile() {
    SLIDEIO_LOG(INFO) << "DCMSlide::initFromFile-begin: initialize DCMSlide from file: " << m_srcPath;

    std::shared_ptr<DCMScene> scene(new DCMScene);
    std::shared_ptr<DCMFile> file(new DCMFile(m_srcPath));
    file->init();
    scene->addFile(file);
    scene->init();
    m_scenes.push_back(scene);

    SLIDEIO_LOG(INFO) << "DCMSlide::initFromFile-end: initialize DCMSlide from file: " << m_srcPath;
}

void DCMSlide::processSeries(std::vector<std::shared_ptr<DCMFile>>& files, bool keepOrder)
{
    SLIDEIO_LOG(INFO) << "DCMSlide::processSeries-begin: initialize DCMSlide from file: " << m_srcPath;

    auto compare = [](std::shared_ptr <DCMFile> left, std::shared_ptr < DCMFile> right)->bool {
        if (left->getWidth() < right->getWidth()) {
            return true;
        }
        if (left->getHeight() < right->getHeight()) {
            return true;
        }
        if (left->getInstanceNumber() < right->getInstanceNumber()) {
            return true;
        }
        return false;
    };
    auto equal = [](const std::shared_ptr<DCMFile>& left, const std::shared_ptr<DCMFile>& right) {
        if (left->getWidth() != right->getWidth()) {
            return false;
        }
        if (left->getHeight() != right->getHeight()) {
            return false;
        }
        if (left->getInstanceNumber()<0 || right->getInstanceNumber()<0) {
            return false;
        }
        return true;
    };

    if(!files.empty()) {
        if(!keepOrder)
        {
            std::sort(files.begin(), files.end(), compare);
        }
        std::vector<DCMScene> scenes;
        std::shared_ptr<DCMFile> firstFile = files[0];
        std::shared_ptr<DCMScene> scene(new DCMScene);
        scene->addFile(firstFile);
        for(auto itFile = ++files.begin(); itFile<files.end(); ++itFile) {
            std::shared_ptr<DCMFile> file = *itFile;
            if(keepOrder || equal(firstFile, file)) {
                scene->addFile(file);
            }
            else {
                try {
                    scene->init();
                    m_scenes.push_back(scene);
                }
                catch(std::exception& ex) {
                    SLIDEIO_LOG(WARNING) << "DCMSlide::processSeries: initialization of scene:" << file->getFilePath() << ". Error: " << ex.what();
                }
                scene.reset(new DCMScene);
                scene->addFile(file);
            }
        }
        try {
            scene->init();
            m_scenes.push_back(scene);
        }
        catch (std::exception& ex) {
            SLIDEIO_LOG(WARNING) << "DCMSlide::processSeries: error initialization of scene: " << scene->getFilePath() << ". Error: " << ex.what();
        }
    }
    SLIDEIO_LOG(INFO) << "DCMSlide::processSeries-end: initialize DCMSlide from file: " << m_srcPath;
}

void DCMSlide::initFromDir()
{
    SLIDEIO_LOG(INFO) << "DCMSlide::initFromDir-begin: initialize DCMSlide from directory: " << m_srcPath;
    fs::recursive_directory_iterator dir(m_srcPath), end;
    std::map<std::string, std::shared_ptr<Series>> seriesMap;
    for (; dir != end; ++dir) {
        if(fs::is_regular_file(dir->path())) {
            try {
                std::shared_ptr<DCMFile> file(new DCMFile(dir->path().string()));
                file->init();
                const std::string& seriesUID = file->getSeriesUID();
                auto itScene = seriesMap.find(seriesUID);
                if(itScene==seriesMap.end()) {
                    std::shared_ptr<Series> series(new Series);
                    series->files.push_back(file);
                    seriesMap[seriesUID] = series;
                }
                else {
                    itScene->second->files.push_back(file);
                }
            }
            catch(std::exception& ex) {
                SLIDEIO_LOG(ERROR) << "DCMSlide::initFromDir: No valid DICOM files found in the directory: " << m_srcPath << ". Error: " << ex.what();
            }
        }
    }
    for(auto&& itSeries : seriesMap) {
        auto series = itSeries.second;
        processSeries(series->files);
    }
    if(getNumScenes()==0) {
        SLIDEIO_LOG(ERROR) << "DCMSlide::initFromDir: No valid DICOM files found in the directory: " << m_srcPath;
        throw std::runtime_error(
            (boost::format("DCMImageDriver: No valid DICOM files found in the directory %1%") % m_srcPath).str());
    }
    SLIDEIO_LOG(INFO) << "DCMSlide::initFromDir-end: initialize DCMSlide from directory: " << m_srcPath;
}

void DCMSlide::initFromDicomDirFile()
{
    SLIDEIO_LOG(INFO) << "DCMSlide::initFromDicomDirFile: attempt to initialize slide from as from DicomDir file : " << m_srcPath;
    bool ok(false);
    DcmDicomDir dicomdir(m_srcPath.c_str());
    DcmDirectoryRecord& rec = dicomdir.getRootRecord();
    DcmDirectoryRecord* patientRecord = nullptr;
    int patientIndex = 0;
    fs::path filePath(m_srcPath);
    fs::path directoryPath = filePath.parent_path();
    for(int patientIndex=0; (patientRecord = rec.getSub(patientIndex))!=nullptr; ++patientIndex)
    {
        DcmDirectoryRecord* studyRecord = nullptr;
        for (int studyIndex = 0; (studyRecord = patientRecord->getSub(studyIndex)) != nullptr; ++studyIndex)
        {
            DcmDirectoryRecord* seriesRecord = nullptr;
            for (int seriesIndex = 0; (seriesRecord = studyRecord->getSub(seriesIndex)) != nullptr; ++seriesIndex)
            {
                std::vector <std::shared_ptr<DCMFile>> series;
                DcmDirectoryRecord* imageRecord = nullptr;
                for (int imageIndex = 0; (imageRecord = seriesRecord->getSub(imageIndex)) != nullptr; ++imageIndex)
                {
                    OFString fileId;
                    if(imageRecord->findAndGetOFStringArray(DCM_ReferencedFileID, fileId, true).good())
                    {
                        try
                        {
                            std::string fileName = fileId.c_str();
                            boost::replace_all(fileName, "\\", "/");
                            filePath = directoryPath / fileName;
                            std::string str = filePath.string();
                            std::shared_ptr<DCMFile> dcm(new DCMFile(str));
                            dcm->init();
                            series.push_back(dcm);
                        }
                        catch(std::exception& ex)
                        {
                            SLIDEIO_LOG(WARNING) << "DCMImageDriver: Error by processing DICOMDIR for file:" << ex.what();
                        }
                    }
                }
                if (!series.empty())
                {
                    processSeries(series, true);
                    ok = true;
                }
            }
        }
    }
    if(ok)
    {
        SLIDEIO_LOG(INFO) << "DCMSlide::initFromDicomDirFile: initialization is successful.";
    }
    else
    {
        RAISE_RUNTIME_ERROR << "DCMSlide::initFromDicomDirFile: loading of DICOMDIR file failed.";
    }
}

void DCMSlide::init()
{
    SLIDEIO_LOG(INFO) << "DCMSlide::init-begin: initialize DCMSlide from path: " << m_srcPath;
    if(fs::is_regular_file(m_srcPath)) 
    {
        if (DCMFile::isDicomDirFile(m_srcPath))
        {
            initFromDicomDirFile();
        }
        else
        {
            initFromFile();
        }
    }
    else if(fs::is_directory(m_srcPath)) {
        initFromDir();
    }
    else {
        SLIDEIO_LOG(ERROR) << "DCMSlide::init: Only regular files or directories are supported: " << m_srcPath;
        throw std::runtime_error("DCMImageDriver: Only regular files are supported");
    }
    SLIDEIO_LOG(INFO) << "DCMSlide::init-end: initialize DCMSlide from path: " << m_srcPath;
}

