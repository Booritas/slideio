#include <gtest/gtest.h>

#include "slideio/core/tools/tempfile.hpp"
#include "slideio/processor/processor.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/processor/processortools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/processor/project.hpp"

using namespace slideio;

TEST(Processor, simple) {
    std::string pathJpg = TestTools::getTestImagePath("jpeg", "p2YCpvg.jpeg");
    const slideio::TempFile storageFile("h5");
    const std::string storagePath = storageFile.getPath().string();

    std::shared_ptr<CVSlide> slide = ImageDriverManager::openSlide(pathJpg, "GDAL");
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    std::shared_ptr<Storage> storage = Storage::createStorage(storagePath, scene->getRect().size());
    std::shared_ptr<Project> project = std::make_shared<Project>(scene, storage);
    Processor::multiResolutionSegmentation(project, 0, 0, 0);
}

TEST(ProcessorTools, nextMoveCW) {
    const struct
    {
        cv::Point current;
        cv::Point center;
        cv::Point expected;
    } test[] = {
            {cv::Point(1, 1), cv::Point(0, 0), cv::Point(0, 1)},
            {cv::Point(106, 202), cv::Point(105, 201), cv::Point(105, 202)},
            {cv::Point(199, 100), cv::Point(200, 100), cv::Point(199, 99)},
            {cv::Point(199, 99), cv::Point(200, 100), cv::Point(200, 99)},
            {cv::Point(200, 99), cv::Point(200, 100), cv::Point(201, 99)},
            {cv::Point(201, 99), cv::Point(200, 100), cv::Point(201, 100)},
            {cv::Point(201, 100), cv::Point(200, 100), cv::Point(201, 101)},
            {cv::Point(201, 101), cv::Point(200, 100), cv::Point(200, 101)},
            {cv::Point(200, 101), cv::Point(200, 100), cv::Point(199, 101)},
            {cv::Point(199, 101), cv::Point(200, 100), cv::Point(199, 100)},

    };
    for (const auto& t : test) {
        const cv::Point result = ProcessorTools::nextMoveCW(t.current, t.center);
        ASSERT_EQ(result, t.expected);
    }
}
