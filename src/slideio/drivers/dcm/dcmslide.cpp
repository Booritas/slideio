// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmslide.hpp"
#include "slideio/drivers/dcm/dcmscene.hpp"
#include "slideio/base.hpp"
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <algorithm>

using namespace slideio;
namespace fs = boost::filesystem;

DCMSlide::DCMSlide(const std::string& filePath) : m_srcPath(filePath)
{
    SLIDEIO_LOG(trace) << "DCMSlide::constructor-begin: " << m_srcPath;
    init();
    SLIDEIO_LOG(trace) << "DCMSlide::constructor-end: " << m_srcPath;
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
    SLIDEIO_LOG(trace) << "DCMSlide::initFromFile-begin: initialize DCMSlide from file: " << m_srcPath;

    std::shared_ptr<DCMScene> scene(new DCMScene);
    std::shared_ptr<DCMFile> file(new DCMFile(m_srcPath));
    file->init();
    scene->addFile(file);
    scene->init();
    m_scenes.push_back(scene);

    SLIDEIO_LOG(trace) << "DCMSlide::initFromFile-end: initialize DCMSlide from file: " << m_srcPath;
}

void DCMSlide::processSeries(std::vector<std::shared_ptr<DCMFile>>& files)
{
    SLIDEIO_LOG(trace) << "DCMSlide::processSeries-begin: initialize DCMSlide from file: " << m_srcPath;

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
        std::sort(files.begin(), files.end(), compare);
        std::vector<DCMScene> scenes;
        std::shared_ptr<DCMFile> firstFile = files[0];
        std::shared_ptr<DCMScene> scene(new DCMScene);
        scene->addFile(firstFile);
        for(auto itFile = ++files.begin(); itFile<files.end(); ++itFile) {
            std::shared_ptr<DCMFile> file = *itFile;
            if(equal(firstFile, file)) {
                scene->addFile(file);
            }
            else {
                try {
                    scene->init();
                    m_scenes.push_back(scene);
                }
                catch(std::exception& ex) {
                    SLIDEIO_LOG(warning) << "DCMSlide::processSeries: initialization of scene:" << file->getFilePath() << ". Error: " << ex.what();
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
            SLIDEIO_LOG(warning) << "DCMSlide::processSeries: error initialization of scene: " << scene->getFilePath() << ". Error: " << ex.what();
        }
    }
    SLIDEIO_LOG(trace) << "DCMSlide::processSeries-end: initialize DCMSlide from file: " << m_srcPath;
}

void DCMSlide::initFromDir()
{
    SLIDEIO_LOG(trace) << "DCMSlide::initFromDir-begin: initialize DCMSlide from directory: " << m_srcPath;
    struct Series
    {
        std::vector<std::shared_ptr<DCMFile>> files;
    };
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
                SLIDEIO_LOG(error) << "DCMSlide::initFromDir: No valid DICOM files found in the directory: " << m_srcPath << ". Error: " << ex.what();
            }
        }
    }
    for(auto&& itSeries : seriesMap) {
        auto series = itSeries.second;
        processSeries(series->files);
    }
    if(getNumScenes()==0) {
        SLIDEIO_LOG(error) << "DCMSlide::initFromDir: No valid DICOM files found in the directory: " << m_srcPath;
        throw std::runtime_error(
            (boost::format("DCMImageDriver: No valid DICOM files found in the directory %1%") % m_srcPath).str());
    }
    SLIDEIO_LOG(trace) << "DCMSlide::initFromDir-end: initialize DCMSlide from directory: " << m_srcPath;
}

void DCMSlide::init()
{
    SLIDEIO_LOG(trace) << "DCMSlide::init-begin: initialize DCMSlide from directory: " << m_srcPath;
    if(fs::is_regular_file(m_srcPath)) {
        initFromFile();
    }
    else if(fs::is_directory(m_srcPath)) {
        initFromDir();
    }
    else {
        SLIDEIO_LOG(error) << "DCMSlide::init: Only regular files are supported: " << m_srcPath;
        throw std::runtime_error("DCMImageDriver: Only regular files are supported");
    }
    SLIDEIO_LOG(trace) << "DCMSlide::init-end: initialize DCMSlide from directory: " << m_srcPath;
}

