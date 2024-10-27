#include <gtest/gtest.h>
#include "slideio-opencv/core.hpp"
#include <opencv2/imgproc.hpp>
#include "slideio/imagetools/tifftools.hpp"
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "tests/testlib/testtools.hpp"
#include <fstream>

#include "slideio/imagetools/similaritytools.hpp"

using namespace slideio;

TEST(ImageTools, cosineSimilarity) {
    // Arrange
    std::vector<double> a = { 1.0, 2.0, 3.0 };
    std::vector<double> b = { 1.0, 2.0, 3.0 };
    std::vector<double> c = { 4.0, 5.0, 6.0 };
    std::vector<double> d = { 0.0, 0.0, 0.0 };
    std::vector<double> e = { 1.0, 2.0, 4.0 };

    // Act
    double similarity_ab = SimilarityTools::cosineSimilarity(a, b);
    double similarity_ac = SimilarityTools::cosineSimilarity(a, c);
    double similarity_ad = SimilarityTools::cosineSimilarity(a, d);
    double similarity_ae = SimilarityTools::cosineSimilarity(a, e);

    // Assert
    ASSERT_NEAR(similarity_ab, 1.0, 0.0001); // vectors are identical, cosine similarity should be 1
    ASSERT_LT(similarity_ac, similarity_ab); // vectors are different, cosine similarity should be less than 1
    ASSERT_EQ(similarity_ad, 0.0); // one vector is zero, cosine similarity should be 0
}
