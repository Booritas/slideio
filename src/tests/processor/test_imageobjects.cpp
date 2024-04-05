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

// TEST(ImageObjectBorderIterator, innerObject) {
//     cv::Mat image(10, 10, CV_32S);
//     image.setTo(0);
//     image.at<int32_t>(cv::Point(5, 5)) = 1;
//     image.at<int32_t>(cv::Point(5, 6)) = 1;
//     image.at<int32_t>(cv::Point(5, 7)) = 1;

//     ImageObject object;
//     object.m_id = 1;
//     object.m_boundingRect = cv::Rect(5, 5, 1, 3);
//     object.m_innerPoint = cv::Point(5, 5);

//     cv::Rect tileRect(0, 0, 10, 10);
//     ImageObjectBorderContainer container(&object, image, tileRect);

//     int count = 0;
//     for (const cv::Point& pixel : container) {
//         EXPECT_TRUE(image.at<int32_t>(pixel) == object.m_id);
//         count++;
//     }

//     EXPECT_EQ(count, 3);
// }
