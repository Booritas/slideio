#include <gtest/gtest.h>

#include "slideio/core/tools/tempfile.hpp"
#include "slideio/processor/imageobjectbordercontainer.hpp"
#include "slideio/processor/imageobjectmanager.hpp"
#include "slideio/processor/imageobjectpixelcontainer.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"

using namespace slideio;

TEST(ImageObjects, addRemoveSingleObjects) {
    std::shared_ptr<ImageObjectManager> imObjMngr = std::make_shared<ImageObjectManager>();
    imObjMngr->clear();
    {
        ImageObject& obj1 = imObjMngr->createObject();
        ASSERT_EQ(1, obj1.m_id);
        ASSERT_EQ(1, imObjMngr->getObjectCount());
        ImageObject& obj2 = imObjMngr->getObject(1);
        ASSERT_EQ(1, obj2.m_id);
    }
    {
        ImageObject& obj1 = imObjMngr->createObject();
        ASSERT_EQ(2, obj1.m_id);
        ASSERT_EQ(2, imObjMngr->getObjectCount());
        ImageObject& obj2 = imObjMngr->getObject(2);
        ASSERT_EQ(2, obj2.m_id);
    }
    {
        imObjMngr->removeObject(1);
        ASSERT_EQ(1, imObjMngr->getObjectCount());
        ImageObject& obj3 = imObjMngr->createObject();
        ASSERT_EQ(1, obj3.m_id);
        ASSERT_EQ(2, imObjMngr->getObjectCount());
    }
}

TEST(ImageObjects, bulkAddObjects) {
    std::shared_ptr<ImageObjectManager> imObjMngr = std::make_shared<ImageObjectManager>();
    imObjMngr->clear();
    {
        std::vector<int> ids;
        imObjMngr->bulkCreate(10, ids);
        ASSERT_EQ(10, ids.size());
        ASSERT_EQ(10, imObjMngr->getObjectCount());
        for (int i = 0; i < 10; ++i) {
            ASSERT_EQ(i+1, ids[i]);
            ImageObject& obj = imObjMngr->getObject(ids[i]);
            ASSERT_EQ(ids[i], obj.m_id);
        }
    }
    {
        std::vector<int> ids;
        imObjMngr->bulkCreate(10, ids);
        ASSERT_EQ(10, ids.size());
        ASSERT_EQ(20, imObjMngr->getObjectCount());
        for (int i = 0; i < 10; ++i) {
            ASSERT_EQ(i+11, ids[i]);
            ImageObject& obj = imObjMngr->getObject(ids[i]);
            ASSERT_EQ(ids[i], obj.m_id);
        }
    }
    {
        std::vector<int> ids = { 1, 5, 7, 15 };
        for (int id : ids) {
            imObjMngr->removeObject(id);
        }
        ASSERT_EQ(16, imObjMngr->getObjectCount());
        std::vector<int> ids2;
        imObjMngr->bulkCreate(10, ids2);
        ASSERT_EQ(10, ids2.size());
        ASSERT_EQ(26, imObjMngr->getObjectCount());
        std::vector<int> idNew = { 1, 5, 7, 15, 21, 22, 23, 24, 25, 26 };
        for (int i = 0; i < 10; ++i) {
            ASSERT_EQ(idNew[i], ids2[i]);
            ImageObject& obj = imObjMngr->getObject(ids2[i]);
            ASSERT_EQ(ids2[i], obj.m_id);
        }
    }
}

TEST(ImageObjects, capacity) {
    std::shared_ptr<ImageObjectManager> imObjMngr = std::make_shared<ImageObjectManager>();
    imObjMngr->clear();
    imObjMngr->setPageSize(10);
    ASSERT_EQ(10, imObjMngr->getCapacity());
    std::vector<int> ids;
    imObjMngr->bulkCreate(10, ids);
    ASSERT_EQ(10, imObjMngr->getCapacity());
    imObjMngr->bulkCreate(10, ids);
    ASSERT_EQ(20, imObjMngr->getCapacity());
    for(int i = 0; i < 20; ++i) {
        ASSERT_EQ(i+1, imObjMngr->getObject(i + 1).m_id);
    }
}

TEST(ImageObjects, clear) {
    std::shared_ptr<ImageObjectManager> imObjMngr = std::make_shared<ImageObjectManager>();
    imObjMngr->clear();
    ASSERT_EQ(0, imObjMngr->getObjectCount());
    std::vector<int> ids;
    imObjMngr->bulkCreate(100, ids);
    ASSERT_EQ(100, imObjMngr->getObjectCount());
    imObjMngr->clear();
    ASSERT_EQ(0, imObjMngr->getObjectCount());
}

TEST(ImageObjectPixelIterator, IterateOverPixels) {
    cv::Mat image(10, 10, CV_32S);
    image.setTo(0);
    image.at<int32_t>(cv::Point(5, 5)) = 1;
    image.at<int32_t>(cv::Point(5, 6)) = 1;
    image.at<int32_t>(cv::Point(5, 7)) = 1;

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = cv::Rect(5, 5, 1, 3);

    cv::Rect tileRect(0, 0, 10, 10);
    ImageObjectPixelContainer container(&object, image, tileRect);

    int count = 0;
    for (const cv::Point& pixel : container) {
        EXPECT_TRUE(image.at<int32_t>(pixel) == object.m_id);
        count++;
    }

    EXPECT_EQ(count, 3);
}

TEST(ImageObjectPixelIterator, EmptyObject) {
    cv::Mat image(10, 10, CV_32S);
    image.setTo(0);

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = cv::Rect(4, 4, 0, 0);

    cv::Rect tileRect(0, 0, 10, 10);
    ImageObjectPixelContainer container(&object, image, tileRect);

    EXPECT_EQ(container.begin(), container.end());
}

TEST(ImageObjectPixelIterator, NoPixelsInObject) {
    cv::Mat image(10, 10, CV_32S);
    image.setTo(0);

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = cv::Rect(4, 4, 4, 4);

    cv::Rect tileRect(0, 0, 10, 10);
    ImageObjectPixelContainer container(&object, image, tileRect);

    EXPECT_EQ(container.begin(), container.end());
}


TEST(ImageObjectPixelIterator, tileOffset) {
    cv::Mat image(10, 10, CV_32S);
    image.setTo(0);
    image.at<int32_t>(cv::Point(5, 5)) = 1;
    image.at<int32_t>(cv::Point(5, 6)) = 1;
    image.at<int32_t>(cv::Point(5, 7)) = 1;

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = cv::Rect(1005, 1505, 1, 3);

    cv::Rect tileRect(1000, 1500, 10, 10);
    ImageObjectPixelContainer container(&object, image, tileRect);

    int count = 0;
    for (const cv::Point& pixel : container) {
        EXPECT_TRUE(image.at<int32_t>(pixel) == object.m_id);
        count++;
    }

    EXPECT_EQ(count, 3);
}

TEST(ImageObjectPixelIterator, partialOverlap) {
    cv::Mat image(10, 10, CV_32S);
    image.setTo(0);
    image.at<int32_t>(cv::Point(5, 0)) = 1;
    image.at<int32_t>(cv::Point(5, 1)) = 1;
    image.at<int32_t>(cv::Point(5, 2)) = 1;

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = cv::Rect(1005, 1505, 1, 3);

    cv::Rect tileRect(1000, 1507, 10, 100);
    ImageObjectPixelContainer container(&object, image, tileRect);

    int count = 0;
    for (const cv::Point& pixel : container) {
        EXPECT_TRUE(image.at<int32_t>(pixel) == object.m_id);
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST(ImageObjectPixelIterator, partialOverlaHorizontal) {
    cv::Mat image(10, 10, CV_32S);
    image.setTo(0);
    image.at<int32_t>(cv::Point(5, 5)) = 1;
    image.at<int32_t>(cv::Point(6, 5)) = 1;
    image.at<int32_t>(cv::Point(7, 5)) = 1;

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = cv::Rect(1005, 1505, 30, 1);

    cv::Rect tileRect(1000, 1500, 10, 10);
    ImageObjectPixelContainer container(&object, image, tileRect);

    int count = 0;
    for (const cv::Point& pixel : container) {
        EXPECT_TRUE(image.at<int32_t>(pixel) == object.m_id);
        count++;
    }
    EXPECT_EQ(count, 3);
}

TEST(ImageObjectPixelIterator, partialOverlaHorizontalLeft) {
    cv::Mat image(10, 10, CV_32S);
    image.setTo(0);
    image.at<int32_t>(cv::Point(0, 5)) = 1;
    image.at<int32_t>(cv::Point(1, 5)) = 1;
    image.at<int32_t>(cv::Point(2, 5)) = 1;

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = cv::Rect(500, 1505, 503, 1);

    cv::Rect tileRect(1000, 1500, 10, 10);
    ImageObjectPixelContainer container(&object, image, tileRect);

    int count = 0;
    for (const cv::Point& pixel : container) {
        EXPECT_TRUE(image.at<int32_t>(pixel) == object.m_id);
        count++;
    }
    EXPECT_EQ(count, 3);
}

TEST(ImageObjectBorderIterator, shape1) {
    std::list<cv::Point> borderPoints;

    cv::Mat mask(15, 15, CV_32S);
    mask.setTo(0);
    for (int y = 5; y < 10; ++y) {
        for (int x = 6; x < 11; ++x) {
            mask.at<int32_t>(cv::Point(x, y)) = 1;
        }
    }
    for (int y = 8; y < 10; ++y) {
        for (int x = 9; x < 11; ++x) {
            mask.at<int32_t>(cv::Point(x, y)) = 0;
        }
    }

    for (int y = 9; y >= 5; --y) {
        borderPoints.push_back(cv::Point(6, y));
    }

    for (int x = 7; x < 11; ++x) {
        borderPoints.push_back(cv::Point(x, 5));
    }

    for (int y = 6; y < 8; ++y) {
        borderPoints.push_back(cv::Point(10, y));
    }

    borderPoints.push_back(cv::Point(9, 7));


    for (int y = 8; y < 10; ++y) {
        borderPoints.push_back(cv::Point(8, y));
    }

    borderPoints.push_back(cv::Point(7, 9));

    int xMin = 0, xMax = mask.cols - 1;
    int yMin = 0, yMax = mask.rows - 1;

    for (const auto& point : borderPoints) {
        mask.at<int32_t>(point) = 1;
        xMin = std::min(xMin, point.x);
        xMax = std::max(xMax, point.x);
        yMin = std::min(yMin, point.y);
        yMax = std::max(yMax, point.y);
    }

    cv::Point offset(0, 0);
    cv::Rect boundingRect(xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);
    boundingRect += offset;

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = boundingRect;

    ImageObjectBorderContainer container(&object, mask, boundingRect);

    auto border = borderPoints.begin();
    for(const cv::Point& point : container) {
        EXPECT_EQ(point, *border);
        ++border;
    }
}

TEST(ImageObjectBorderIterator, shape2) {

    cv::Mat mask(15, 15, CV_32S);
    mask.setTo(0);
    for (int y = 5; y < 9; ++y) {
        for (int x = 6; x < 11; ++x) {
            mask.at<int32_t>(cv::Point(x, y)) = 1;
        }
    }

    mask.at<int32_t>(cv::Point(6, 8)) = 0;
    mask.at<int32_t>(cv::Point(6, 5)) = 0;
    mask.at<int32_t>(cv::Point(10, 5)) = 0;
    mask.at<int32_t>(cv::Point(10, 8)) = 0;
    mask.at<int32_t>(cv::Point(8, 7)) = 0;
    mask.at<int32_t>(cv::Point(8, 8)) = 0;

    std::list<cv::Point> borderPoints = {
        {6, 7},
        {6, 6},
        {7, 5},
        {8, 5},
        {9, 5},
        {10, 6},
        {10, 7},
        {9, 8},
        {9, 7},
        {8, 6},
        {7, 7},
        {7, 8}
    };
    borderPoints.push_back(cv::Point(6, 7));


    int xMin = 0, xMax = mask.cols - 1;
    int yMin = 0, yMax = mask.rows - 1;

    for (const auto& point : borderPoints) {
        mask.at<int32_t>(point) = 1;
        xMin = std::min(xMin, point.x);
        xMax = std::max(xMax, point.x);
        yMin = std::min(yMin, point.y);
        yMax = std::max(yMax, point.y);
    }

    cv::Point offset(8, 9);
    cv::Rect boundingRect(xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);
    boundingRect += offset;
    for (auto& pnt : borderPoints) {
        pnt += offset;
    }

    Tile tile(mask, offset);

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = boundingRect;

    ImageObjectBorderContainer container(&object, mask, boundingRect);

    auto border = borderPoints.begin();
    for (const cv::Point& point : container) {
        EXPECT_EQ(point, *border);
        ++border;
    }
}
