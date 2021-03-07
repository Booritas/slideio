// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmslide.hpp"
#include "slideio/drivers/dcm/dcmscene.hpp"
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <algorithm>

using namespace slideio;
namespace fs = boost::filesystem;

DCMSlide::DCMSlide(const std::string& filePath) : m_srcPath(filePath)
{
    init();
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
    std::shared_ptr<DCMScene> scene(new DCMScene);
    std::shared_ptr<DCMFile> file(new DCMFile(m_srcPath));
    file->init();
    scene->addFile(file);
    scene->init();
    m_scenes.push_back(scene);
}

void DCMSlide::processSeries(std::vector<std::shared_ptr<DCMFile>>& files)
{
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

        }
    }
}

void DCMSlide::initFromDir()
{
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
                
            }
        }
    }
    for(auto&& itSeries : seriesMap) {
        auto series = itSeries.second;
        processSeries(series->files);
    }
    if(getNumScenes()==0) {
        throw std::runtime_error(
            (boost::format("DCMImageDriver: No valid DICOM files found in the directory %1%") % m_srcPath).str());
    }
}

void DCMSlide::init()
{
    if(fs::is_regular_file(m_srcPath)) {
        initFromFile();
    }
    else if(fs::is_directory(m_srcPath)) {
        initFromDir();
    }
    else {
        throw std::runtime_error("DCMImageDriver: Only regular files are supported");
    }
}

