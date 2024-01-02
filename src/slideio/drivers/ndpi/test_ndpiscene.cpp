#include <gtest/gtest.h>
#include <slideio/drivers/ndpi/ndpiscene.hpp>

TEST(NDPIScene, findZoomDirectory)
{
    // Create a mock NDPITiffFile object
    NDPITiffFile mockFile;
    // Set up the directories in the mock file
    std::vector<NDPITiffDirectory> directories;
    directories.push_back(NDPITiffDirectory(0, 0, 0, 0)); // Add some directories
    directories.push_back(NDPITiffDirectory(1, 1, 1, 1));
    directories.push_back(NDPITiffDirectory(2, 2, 2, 2));
    mockFile.setDirectories(directories);

    // Create an NDPIScene object with the mock file
    NDPIScene scene(&mockFile);

    // Define the input parameters for the function
    cv::Rect imageBlockRect(0, 0, 100, 100);
    cv::Size requiredBlockSize(200, 200);

    // Call the function
    const NDPITiffDirectory& result = scene.findZoomDirectory(imageBlockRect, requiredBlockSize);

    // Check the expected result
    EXPECT_EQ(result, directories[2]); // The third directory should be returned
}