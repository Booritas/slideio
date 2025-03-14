﻿// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmslide.hpp"
#include "slideio/drivers/dcm/dcmscene.hpp"
#include "slideio/base/base.hpp"
#include <filesystem>
#include <algorithm>
#include <dcmdata/dcdeftag.h>
#include <dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcdicdir.h>
#include "wsiscene.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/base/log.hpp"

using namespace slideio;
namespace fs = std::filesystem;
const std::string SLIDE_MICROSCOPY_MODALITY = "SM";


struct Series
{
    std::vector<std::shared_ptr<DCMFile>> files;
};

DCMSlide::DCMSlide(const std::string& filePath) : m_srcPath(filePath) {
    SLIDEIO_LOG(INFO) << "DCMSlide::constructor-begin: " << m_srcPath;
    init();
    SLIDEIO_LOG(INFO) << "DCMSlide::constructor-end: " << m_srcPath;
}

int DCMSlide::getNumScenes() const {
    return static_cast<int>(m_scenes.size());
}

std::string DCMSlide::getFilePath() const {
    return m_srcPath;
}

std::shared_ptr<CVScene> DCMSlide::getScene(int index) const {
    return m_scenes[index];
}

void DCMSlide::initFromFile() {
    SLIDEIO_LOG(INFO) << "DCMSlide::initFromFile-begin: initialize DCMSlide from file: " << m_srcPath;
    if(DCMFile::isWSIFile(m_srcPath)) {
        initFromWSIFile();
    }
    else {
        initFromRegularDicomFile();
    }
    SLIDEIO_LOG(INFO) << "DCMSlide::initFromFile-end: initialize DCMSlide from file: " << m_srcPath;
}

void DCMSlide::processRegularSeries(std::vector<std::shared_ptr<DCMFile>>& files, bool keepOrder) {
    auto compare = [](std::shared_ptr<DCMFile> left, std::shared_ptr<DCMFile> right)-> bool {
        if (left->getWidth() < right->getWidth()) {
            return true;
        }
        if (left->getWidth() == right->getWidth()) {
            if (left->getHeight() < right->getHeight()) {
                return true;
            }
            if (left->getHeight() == right->getHeight()) {
                if (left->getInstanceNumber() < right->getInstanceNumber()) {
                    return true;
                }
            }
        }
        return false;
    };
    auto equal = [](const std::shared_ptr<DCMFile>& left, const std::shared_ptr<DCMFile>& right) {
        if (SLIDE_MICROSCOPY_MODALITY == left->getModality() || SLIDE_MICROSCOPY_MODALITY == right->getModality())
            return false;
        if (left->getWidth() != right->getWidth()) {
            return false;
        }
        if (left->getHeight() != right->getHeight()) {
            return false;
        }
        if (left->getInstanceNumber() < 0 || right->getInstanceNumber() < 0) {
            return false;
        }
        return true;
    };

    if (!files.empty()) {
        if (!keepOrder) {
            std::sort(files.begin(), files.end(), compare);
        }
        std::vector<DCMScene> scenes;
        std::shared_ptr<DCMFile> firstFile = files[0];
        std::shared_ptr<DCMScene> scene(new DCMScene);
        scene->addFile(firstFile);
        for (auto itFile = ++files.begin(); itFile < files.end(); ++itFile) {
            std::shared_ptr<DCMFile> file = *itFile;
            if (keepOrder || equal(firstFile, file)) {
                scene->addFile(file);
            }
            else {
                try {
                    scene->init();
                    m_scenes.push_back(scene);
                }
                catch (std::exception& ex) {
                    SLIDEIO_LOG(WARNING) << "DCMSlide::processSeries: initialization of scene:" << file->getFilePath()
                        << ". Error: " << ex.what();
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
            SLIDEIO_LOG(WARNING) << "DCMSlide::processSeries: error initialization of scene: " << scene->getFilePath()
                << ". Error: " << ex.what();
        }
    }
}

void DCMSlide::processWSISeries(std::vector<std::shared_ptr<DCMFile>>& files) {
    std::vector<std::shared_ptr<DCMFile>> auxFiles;
    auto scene = std::make_shared<WSIScene>();
    for(auto&& file : files) {
        scene->addFile(file);
    }
    scene->init();
    m_scenes.push_back(scene);
}

void DCMSlide::processSeries(std::vector<std::shared_ptr<DCMFile>>& files, bool keepOrder) {
    SLIDEIO_LOG(INFO) << "DCMSlide::processSeries-begin: initialize DCMSlide from file: " << m_srcPath;

    std::vector<std::shared_ptr<DCMFile>> wsiFiles;
    auto moveIterator = std::remove_if(files.begin(), files.end(), [&wsiFiles](std::shared_ptr<DCMFile> file) {
        if (file->isWSIFile()) {
            wsiFiles.push_back(file);
            return true;
        }
        return false;
    });
    files.erase(moveIterator, files.end());

    if(!wsiFiles.empty()) {
        processWSISeries(wsiFiles);
    }
    if(!files.empty()) {
        processRegularSeries(files, keepOrder);
    }
    SLIDEIO_LOG(INFO) << "DCMSlide::processSeries-end: initialize DCMSlide from file: " << m_srcPath;
}

void DCMSlide::initFromDir() {
    SLIDEIO_LOG(INFO) << "DCMSlide::initFromDir-begin: initialize DCMSlide from directory: " << m_srcPath;
    fs::recursive_directory_iterator dir(m_srcPath), end;
    std::map<std::string, std::shared_ptr<Series>> seriesMap;
    for (; dir != end; ++dir) {
        if (fs::is_regular_file(dir->path())) {
            try {
                std::shared_ptr<DCMFile> file(new DCMFile(dir->path().string()));
                file->init();
                const std::string& seriesUID = file->getSeriesUID();
                auto itScene = seriesMap.find(seriesUID);
                if (itScene == seriesMap.end()) {
                    std::shared_ptr<Series> series(new Series);
                    series->files.push_back(file);
                    seriesMap[seriesUID] = series;
                }
                else {
                    itScene->second->files.push_back(file);
                }
            }
            catch (std::exception& ex) {
                SLIDEIO_LOG(ERROR) << "DCMSlide::initFromDir: No valid DICOM files found in the directory: " <<
                    m_srcPath << ". Error: " << ex.what();
            }
        }
    }
    for (auto&& itSeries : seriesMap) {
        auto series = itSeries.second;
        processSeries(series->files);
    }
    if (getNumScenes() == 0) {
        RAISE_RUNTIME_ERROR << "DCMImageDriver: No valid DICOM files found in the directory " << m_srcPath;
    }
    SLIDEIO_LOG(INFO) << "DCMSlide::initFromDir-end: initialize DCMSlide from directory: " << m_srcPath;
}

void DCMSlide::initFromDicomDirFile() {
    SLIDEIO_LOG(INFO) << "DCMSlide::initFromDicomDirFile: attempt to initialize slide from as from DicomDir file : " <<
        m_srcPath;
    bool ok(false);
    DcmDicomDir dicomdir(m_srcPath.c_str());
    DcmDirectoryRecord& rec = dicomdir.getRootRecord();
    DcmDirectoryRecord* patientRecord = nullptr;
    int patientIndex = 0;
    fs::path filePath(m_srcPath);
    fs::path directoryPath = filePath.parent_path();
    for (int patientIndex = 0; (patientRecord = rec.getSub(patientIndex)) != nullptr; ++patientIndex) {
        DcmDirectoryRecord* studyRecord = nullptr;
        for (int studyIndex = 0; (studyRecord = patientRecord->getSub(studyIndex)) != nullptr; ++studyIndex) {
            DcmDirectoryRecord* seriesRecord = nullptr;
            for (int seriesIndex = 0; (seriesRecord = studyRecord->getSub(seriesIndex)) != nullptr; ++seriesIndex) {
                std::vector<std::shared_ptr<DCMFile>> series;
                DcmDirectoryRecord* imageRecord = nullptr;
                for (int imageIndex = 0; (imageRecord = seriesRecord->getSub(imageIndex)) != nullptr; ++imageIndex) {
                    OFString fileId;
                    if (imageRecord->findAndGetOFStringArray(DCM_ReferencedFileID, fileId, true).good()) {
                        try {
                            std::string fileName = fileId.c_str();
                            Tools::replaceAll(fileName, "\\", "/");
                            filePath = directoryPath / fileName;
                            std::string str = filePath.string();
                            std::shared_ptr<DCMFile> dcm(new DCMFile(str));
                            dcm->init();
                            series.push_back(dcm);
                        }
                        catch (std::exception& ex) {
                            SLIDEIO_LOG(WARNING) << "DCMImageDriver: Error by processing DICOMDIR for file:" << ex.
                                what();
                        }
                    }
                }
                if (!series.empty()) {
                    processSeries(series, true);
                    ok = true;
                }
            }
        }
    }
    if (ok) {
        SLIDEIO_LOG(INFO) << "DCMSlide::initFromDicomDirFile: initialization is successful.";
    }
    else {
        RAISE_RUNTIME_ERROR << "DCMSlide::initFromDicomDirFile: loading of DICOMDIR file failed.";
    }
}

void DCMSlide::init() {
    SLIDEIO_LOG(INFO) << "DCMSlide::init-begin: initialize DCMSlide from path: " << m_srcPath;
#if defined(WIN32)
    std::wstring srcPathW = Tools::toWstring(m_srcPath);
    if (fs::is_regular_file(srcPathW))
#else
    if (fs::is_regular_file(m_srcPath))
#endif
    {
        if (DCMFile::isDicomDirFile(m_srcPath)) {
            initFromDicomDirFile();
        }
        else {
            initFromFile();
        }
    }
#if defined(WIN32)
    else if (fs::is_directory(srcPathW))
#else
    else if (fs::is_directory(m_srcPath))
#endif
    {
        initFromDir();
    }
    else {
        SLIDEIO_LOG(ERROR) << "DCMSlide::init: Only regular files or directories are supported: " << m_srcPath;
        throw std::runtime_error("DCMImageDriver: Only regular files are supported");
    }
    SLIDEIO_LOG(INFO) << "DCMSlide::init-end: initialize DCMSlide from path: " << m_srcPath;
}

void DCMSlide::initFromWSIFile() {
    std::shared_ptr<WSIScene> scene(new WSIScene);
    std::shared_ptr<DCMFile> file(new DCMFile(m_srcPath));
    file->init();
    file->setScale(1.0);
    scene->addFile(file);
    scene->init();
    m_scenes.push_back(scene);
}

void DCMSlide::initFromRegularDicomFile() {
    std::shared_ptr<DCMScene> scene(new DCMScene);
    std::shared_ptr<DCMFile> file(new DCMFile(m_srcPath));
    file->init();
    scene->addFile(file);
    scene->init();
    m_scenes.push_back(scene);
}
