// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>

#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/tifffiles.hpp"
#include "tests/testlib/testtools.hpp"

class TIFFFilesTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
    std::list<std::string> testFiles = {
        TestTools::getFullTestImagePath("svs","CMU-1-Small-Region.svs"),
        TestTools::getFullTestImagePath("ometiff","Subresolutions/retina_large.ome.tiff"),
        TestTools::getFullTestImagePath("ometiff","Subresolutions/Leica-2.ome.tiff"),
        TestTools::getFullTestImagePath("ometiff","Subresolutions/Leica-1.ome.tiff"),
        TestTools::getFullTestImagePath("ometiff","SPIM-ModuloAlongZ.ome.tiff"),
        TestTools::getFullTestImagePath("ometiff","LAMBDA-ModuloAlongZ-ModuloAlongT.ome.tiff")
    };
    slideio::TIFFFiles tiffFiles;
};

TEST_F(TIFFFilesTest, GetOrOpen_FileNotFound) {
    EXPECT_THROW(tiffFiles.getOrOpen("nonexistent_file.tiff"), slideio::RuntimeError);
}

TEST_F(TIFFFilesTest, GetOrOpen_FileOpenedSuccessfully) {
	std::string testFilePath = testFiles.front();
    libtiff::TIFF* tiff1 = tiffFiles.getOrOpen(testFilePath);
    ASSERT_NE(tiff1, nullptr);
    libtiff::TIFF* tiff2 = tiffFiles.getOrOpen(testFilePath);
	ASSERT_EQ(tiff1, tiff2);
	EXPECT_EQ(tiffFiles.getNumberOfOpenFiles(), 1);
    tiffFiles.close(testFilePath);
	EXPECT_EQ(tiffFiles.getNumberOfOpenFiles(), 0);
}

TEST_F(TIFFFilesTest, Close_FileClosedSuccessfully) {
    std::string testFilePath = testFiles.front();
    tiffFiles.getOrOpen(testFilePath);
    tiffFiles.close(testFilePath);
    EXPECT_EQ(0, tiffFiles.getNumberOfOpenFiles());
}

TEST_F(TIFFFilesTest, CloseAll_AllFilesClosedSuccessfully) {
	for (const auto& filePath : testFiles) {
		tiffFiles.getOrOpen(filePath);
	}
	EXPECT_EQ(testFiles.size(), tiffFiles.getNumberOfOpenFiles());
    EXPECT_EQ(testFiles.size(), tiffFiles.getOpenFileCounter());

    for (const auto& filePath : testFiles) {
        tiffFiles.getOrOpen(filePath);
    }
    EXPECT_EQ(testFiles.size(), tiffFiles.getNumberOfOpenFiles());
    EXPECT_EQ(testFiles.size(), tiffFiles.getOpenFileCounter());


    tiffFiles.close(testFiles.front());
    EXPECT_EQ(testFiles.size()-1, tiffFiles.getNumberOfOpenFiles());
    EXPECT_EQ(testFiles.size()-1, tiffFiles.getOpenFileCounter());
    tiffFiles.closeAll();
    EXPECT_EQ(0, tiffFiles.getNumberOfOpenFiles());
    EXPECT_EQ(0, tiffFiles.getOpenFileCounter());
}