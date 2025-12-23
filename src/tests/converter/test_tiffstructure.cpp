#include <gtest/gtest.h>

#include "slideio/base/exceptions.hpp"
#include "slideio/converter/tiffstructure.hpp"

using namespace slideio::converter;

TEST(TiffStructureTests, TiffDirectoryStructure_SettersAndGetters) {
    TiffDirectoryStructure dir;
    dir.setChannelRange(cv::Range(1, 3));
    dir.setZSliceRange(cv::Range(2, 4));
    dir.setTFrameRange(cv::Range(0, 1));
    dir.setZoomLevelRange(cv::Range(0, 2));
    dir.setDescription("desc");
    dir.setPlaneCount(4);

    EXPECT_EQ(cv::Range(1, 3), dir.getChannelRange());
    EXPECT_EQ(cv::Range(2, 4), dir.getZSliceRange());
    EXPECT_EQ(cv::Range(0, 1), dir.getTFrameRange());
    EXPECT_EQ(cv::Range(0, 2), dir.getZoomLevelRange());
    EXPECT_EQ("desc", dir.getDescription());
    EXPECT_EQ(4, dir.getPlaneCount());
}

TEST(TiffStructureTests, TiffPageStructure_Subdirectories) {
    TiffPageStructure page;
    // Initially zero
    EXPECT_EQ(0, page.getNumSubDirectories());
    // Append and verify
    TiffDirectoryStructure& sub1 = page.appendSubDirectory();
    sub1.setZoomLevelRange(cv::Range(1, 2));
    EXPECT_EQ(1, page.getNumSubDirectories());
    const TiffDirectoryStructure& got1 = page.getSubDirectory(0);
    EXPECT_EQ(cv::Range(1, 2), got1.getZoomLevelRange());

    TiffDirectoryStructure& sub2 = page.appendSubDirectory();
    sub2.setChannelRange(cv::Range(0, 1));
    EXPECT_EQ(2, page.getNumSubDirectories());
    auto& got2 = page.getSubDirectory(1);
    EXPECT_EQ(cv::Range(0, 1), got2.getChannelRange());
}

TEST(TiffStructureTests, TiffPageStructure_SubdirectoryOutOfRange) {
    TiffPageStructure page;
    // Expect runtime error when index is out of range
    EXPECT_THROW({ (void)page.getSubDirectory(0); }, slideio::RuntimeError);
}

TEST(TiffStructureTests, TiffDirectoryStructure_CopyConstructor) {
    // Create and populate source directory
    TiffDirectoryStructure source;
    source.setChannelRange(cv::Range(11, 15));
    source.setZSliceRange(cv::Range(22, 27));
    source.setTFrameRange(cv::Range(33, 39));
    source.setZoomLevelRange(cv::Range(0, 3));
    source.setDescription("Test Description");
    source.setPlaneCount(42);

    // Create copy using copy constructor
    TiffDirectoryStructure copy(source);

    // Verify all fields are copied correctly
    EXPECT_EQ(source.getChannelRange(), copy.getChannelRange());
    EXPECT_EQ(source.getZSliceRange(), copy.getZSliceRange());
    EXPECT_EQ(source.getTFrameRange(), copy.getTFrameRange());
    EXPECT_EQ(source.getZoomLevelRange(), copy.getZoomLevelRange());
    EXPECT_EQ(source.getDescription(), copy.getDescription());
    EXPECT_EQ(source.getPlaneCount(), copy.getPlaneCount());

    // Verify deep copy - modifying source shouldn't affect copy
    source.setPlaneCount(100);
    source.setDescription("Modified");
    EXPECT_EQ(42, copy.getPlaneCount());
    EXPECT_EQ("Test Description", copy.getDescription());
}

TEST(TiffStructureTests, TiffDirectoryStructure_AssignmentOperator) {
    // Create and populate source directory
    TiffDirectoryStructure source;
    source.setChannelRange(cv::Range(20, 23));
    source.setZSliceRange(cv::Range(26, 28));
    source.setTFrameRange(cv::Range(35, 37));
    source.setZoomLevelRange(cv::Range(1, 4));
    source.setDescription("Source Description");
    source.setPlaneCount(99);

    // Create target with different values
    TiffDirectoryStructure target;
    target.setChannelRange(cv::Range(0, 1));
    target.setPlaneCount(1);
    target.setDescription("Target Description");

    // Assign source to target
    target = source;

    // Verify all fields are copied correctly
    EXPECT_EQ(source.getChannelRange(), target.getChannelRange());
    EXPECT_EQ(source.getZSliceRange(), target.getZSliceRange());
    EXPECT_EQ(source.getTFrameRange(), target.getTFrameRange());
    EXPECT_EQ(source.getZoomLevelRange(), target.getZoomLevelRange());
    EXPECT_EQ(source.getDescription(), target.getDescription());
    EXPECT_EQ(source.getPlaneCount(), target.getPlaneCount());

    // Verify deep copy - modifying source shouldn't affect target
    source.setPlaneCount(200);
    source.setDescription("Modified Source");
    EXPECT_EQ(99, target.getPlaneCount());
    EXPECT_EQ("Source Description", target.getDescription());
}

TEST(TiffStructureTests, TiffDirectoryStructure_SelfAssignment) {
    TiffDirectoryStructure dir;
    dir.setChannelRange(cv::Range(14, 17));
    dir.setZSliceRange(cv::Range(25, 28));
    dir.setDescription("Self Assignment Test");
    dir.setPlaneCount(77);

    // Self-assignment should be safe
    dir = dir;

    // Verify values are unchanged
    EXPECT_EQ(cv::Range(14, 17), dir.getChannelRange());
    EXPECT_EQ(cv::Range(25, 28), dir.getZSliceRange());
    EXPECT_EQ("Self Assignment Test", dir.getDescription());
    EXPECT_EQ(77, dir.getPlaneCount());
}

TEST(TiffStructureTests, TiffDirectoryStructure_CopyConstructorWithDefaults) {
    // Create directory with default values
    TiffDirectoryStructure source;

    // Copy with defaults
    TiffDirectoryStructure copy(source);

    // Verify default values are copied
    EXPECT_EQ(cv::Range(0, 0), copy.getChannelRange());
    EXPECT_EQ(cv::Range(0, 0), copy.getZSliceRange());
    EXPECT_EQ(cv::Range(0, 0), copy.getTFrameRange());
    EXPECT_EQ(cv::Range(0, 0), copy.getZoomLevelRange());
    EXPECT_EQ("", copy.getDescription());
    EXPECT_EQ(1, copy.getPlaneCount()); // Default is 1
}

TEST(TiffStructureTests, TiffDirectoryStructure_ChainedAssignment) {
    TiffDirectoryStructure dir1, dir2, dir3;
    
    dir1.setChannelRange(cv::Range(1, 2));
    dir1.setDescription("Chain Test");
    dir1.setPlaneCount(55);

    // Chained assignment
    dir3 = dir2 = dir1;

    // Verify all copies have the same values
    EXPECT_EQ(dir1.getChannelRange(), dir2.getChannelRange());
    EXPECT_EQ(dir1.getChannelRange(), dir3.getChannelRange());
    EXPECT_EQ(dir1.getDescription(), dir2.getDescription());
    EXPECT_EQ(dir1.getDescription(), dir3.getDescription());
    EXPECT_EQ(dir1.getPlaneCount(), dir2.getPlaneCount());
    EXPECT_EQ(dir1.getPlaneCount(), dir3.getPlaneCount());
    EXPECT_EQ("Chain Test", dir2.getDescription());
    EXPECT_EQ("Chain Test", dir3.getDescription());
}