#include <gtest/gtest.h>

#include "slideio/core/tools/tempfile.hpp"
#include "slideio/processor/processor.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;

TEST(Processor, simple) {
    std::string pathJpg = TestTools::getTestImagePath("jpeg", "p2YCpvg.jpeg");
    const slideio::TempFile storageFile("h5");
    const std::string storagePath = storageFile.getPath().string();

    std::shared_ptr<CVSlide> slide = ImageDriverManager::openSlide(pathJpg, "GDAL");
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    std::shared_ptr<Storage> storage = Storage::createStorage(storagePath, scene->getRect().size());

    Processor::multiResolutionSegmentation(scene, 0, 0, 0, storage);
}

