#include <gtest/gtest.h>

#include "slideio/core/tools/tempfile.hpp"
#include "slideio/processor/processor.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/processor/processortools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/processor/project.hpp"
#include "slideio/processor/tile.hpp"
#include "slideio/processor/multiresolutionsegmentation.hpp"

using namespace slideio;

TEST(Processor, simple) {
    std::string pathJpg = TestTools::getTestImagePath("jpeg", "p2YCpvg.jpeg");
    const slideio::TempFile storageFile("h5");
    const std::string storagePath = storageFile.getPath().string();

    std::shared_ptr<CVSlide> slide = ImageDriverManager::openSlide(pathJpg, "GDAL");
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    std::shared_ptr<Storage> storage = Storage::createStorage(storagePath, scene->getRect().size());
    std::shared_ptr<Project> project = std::make_shared<Project>(scene, storage);
    std::shared_ptr<MultiResolutionSegmentationParameters> params = 
        std::make_shared<MultiResolutionSegmentationParameters>(1.,cv::Size(512,512));
    mutliResolutionSegmentation(project,params);
    cv::Rect rect = scene->getRect();
    EXPECT_EQ(rect.width*rect.height, project->getObjects()->getObjectCount());
}

TEST(ProcessorTools, rotatePixelCW) {
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
        const cv::Point result = ProcessorTools::rotatePixelCW(t.current, t.center);
        EXPECT_EQ(result, t.expected);
    }
}

TEST(ProcessorTools, rotatePointCW) {
    const cv::Point p1(5, 7), center(5, 6);
    const cv::Point& p2 = ProcessorTools::rotatePointCW(p1, center);
    EXPECT_EQ(p2, cv::Point(4, 6));
    const cv::Point& p3 = ProcessorTools::rotatePointCW(p2, center);
    EXPECT_EQ(p3, cv::Point(5, 5));
    const cv::Point& p4 = ProcessorTools::rotatePointCW(p3, center);
    EXPECT_EQ(p4, cv::Point(6, 6));
    const cv::Point& p5 = ProcessorTools::rotatePointCW(p4, center);
    EXPECT_EQ(p5, p1);
}

TEST(Tile, contains) {
    cv::Point offset(10, 10);
    cv::Mat mask(20, 20, CV_32SC1, cv::Scalar(255));
    Tile tile(mask, offset);

    EXPECT_TRUE(tile.contains(cv::Point(15, 15)));
    EXPECT_TRUE(tile.contains(cv::Point(10, 10)));
    EXPECT_FALSE(tile.contains(cv::Point(5, 15)));
    EXPECT_FALSE(tile.contains(cv::Point(30, 15)));
}

TEST(Tile, doesObjectContain1) {
    cv::Mat mask(15, 15, CV_32S);
    mask.setTo(0);
    mask.at<int32_t>(cv::Point(5, 5)) = 1;
    mask.at<int32_t>(cv::Point(5, 6)) = 1;
    mask.at<int32_t>(cv::Point(5, 7)) = 1;

    cv::Point offset(4, 5);
    cv::Rect boundingRect(5, 5, 1, 3);
    boundingRect += offset;

    Tile tile(mask, offset);

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = boundingRect;

    EXPECT_TRUE(tile.doesObjectContain(cv::Point(5, 6) + offset, &object));
    EXPECT_FALSE(tile.doesObjectContain(cv::Point(6, 6) + offset, &object));
}

TEST(Tile, doesObjectContain2) {
    cv::Mat mask(15, 15, CV_32S);
    mask.setTo(0);
    mask.at<int32_t>(cv::Point(5, 5)) = 1;
    mask.at<int32_t>(cv::Point(5, 6)) = 1;
    mask.at<int32_t>(cv::Point(5, 7)) = 1;

    mask.at<int32_t>(cv::Point(6, 5)) = 1;
    mask.at<int32_t>(cv::Point(7, 5)) = 1;
    mask.at<int32_t>(cv::Point(8, 5)) = 1;

    cv::Point offset(4, 5);
    cv::Rect boundingRect(5, 5, 4, 3);
    boundingRect += offset;

    Tile tile(mask, offset);

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = boundingRect;

    EXPECT_TRUE(tile.doesObjectContain(cv::Point(5, 6) + offset, &object));
    EXPECT_FALSE(tile.doesObjectContain(cv::Point(6, 6) + offset, &object));
    EXPECT_FALSE(tile.doesObjectContain(cv::Point(9, 9) + offset, &object));
    EXPECT_FALSE(tile.doesObjectContain(cv::Point(-1, -1) + offset, &object));
    EXPECT_FALSE(tile.doesObjectContain(cv::Point(100, 100) + offset, &object));
}

TEST(Tile, isOnObjectBorder1) {
    auto comp = [](const cv::Point& left, const cv::Point& right) {
        return left.x < right.x || (left.x == right.x && left.y < right.y);
    };

    std::set<cv::Point, decltype(comp)> borderPoints(comp);
    std::list<cv::Point> borderList =
    {
        {5, 5}, {5, 6}, {5, 7},
        {6, 5}, {7, 5}, {8, 5}
    };
    borderPoints.insert(borderList.begin(), borderList.end());

    cv::Mat mask(15, 15, CV_32S);
    mask.setTo(0);
    int xMin = 0, xMax = mask.cols - 1;
    int yMin = 0, yMax = mask.rows - 1;

    for (const auto& point : borderPoints) {
        mask.at<int32_t>(point) = 1;
        xMin = std::min(xMin, point.x);
        xMax = std::max(xMax, point.x);
        yMin = std::min(yMin, point.y);
        yMax = std::max(yMax, point.y);
    }

    cv::Point offset(4, 5);
    cv::Rect boundingRect(xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);
    boundingRect += offset;

    Tile tile(mask, offset);

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = boundingRect;

    for (int y = 0; y < mask.rows; ++y) {
        for (int x = 0; x < mask.cols; ++x) {
            cv::Point point(x, y);
            point += offset;
            if (tile.isOnObjectBorder(point, &object)) {
                point -= offset;
                EXPECT_TRUE(borderPoints.find(point) != borderPoints.end());
                borderPoints.erase(point);
            }
        }
    }
    EXPECT_TRUE(borderPoints.empty());
}

TEST(Tile, isOnObjectBorder2) {
    auto comp = [](const cv::Point& left, const cv::Point& right) {
        return left.x < right.x || (left.x == right.x && left.y < right.y);
        };

    std::set<cv::Point, decltype(comp)> borderPoints(comp);

    cv::Mat mask(15, 15, CV_32S);
    mask.setTo(0);
    for(int y = 5; y < 10; ++y) {
        for(int x = 6; x < 11; ++x) {
            mask.at<int32_t>(cv::Point(x, y)) = 1;
        }
    }
    for (int y = 8; y < 10; ++y) {
        for (int x = 9; x < 11; ++x) {
            mask.at<int32_t>(cv::Point(x, y)) = 0;
        }
    }

    for (int y = 5; y < 10; ++y) {
        borderPoints.insert(cv::Point(6, y));
    }
    for (int y = 5; y < 8; ++y) {
        borderPoints.insert(cv::Point(10, y));
    }

    for (int y = 7; y < 10; ++y) {
        borderPoints.insert(cv::Point(8, y));
    }
    for (int x = 7; x < 10; ++x) {
        borderPoints.insert(cv::Point(x, 5));
    }

    borderPoints.insert(cv::Point(9, 7));
    borderPoints.insert(cv::Point(7, 9));

    int xMin = 0, xMax = mask.cols - 1;
    int yMin = 0, yMax = mask.rows - 1;

    for (const auto& point : borderPoints) {
        mask.at<int32_t>(point) = 1;
        xMin = std::min(xMin, point.x);
        xMax = std::max(xMax, point.x);
        yMin = std::min(yMin, point.y);
        yMax = std::max(yMax, point.y);
    }

    cv::Point offset(4, 5);
    cv::Rect boundingRect(xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);
    boundingRect += offset;

    Tile tile(mask, offset);

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = boundingRect;

    for (int y = 0; y < mask.rows; ++y) {
        for (int x = 0; x < mask.cols; ++x) {
            cv::Point point(x, y);
            point += offset;
            if (tile.isOnObjectBorder(point, &object)) {
                point -= offset;
                bool found = borderPoints.find(point) != borderPoints.end();
                EXPECT_TRUE(found);
                borderPoints.erase(point);
            }
        }
    }
    EXPECT_TRUE(borderPoints.empty());
}

TEST(Tile, findFirstObjectBorderPixel) {
    cv::Mat mask(15, 15, CV_32S);
    mask.setTo(1);
    const cv::Point offset(40, 50);
    const Tile tile(mask, offset);
    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = cv::Rect(offset, cv::Size(mask.cols, mask.rows));
    cv::Point pnt;
    EXPECT_TRUE(tile.findFirstObjectBorderPixel(&object, pnt));
    EXPECT_EQ(cv::Point(40,64), pnt);
    mask.at<int32_t>( 14, 0) = 0;
    EXPECT_TRUE(tile.findFirstObjectBorderPixel(&object, pnt));
    EXPECT_EQ(cv::Point(40, 63), pnt);
    mask.at<int32_t>(13, 0) = 0;
    EXPECT_TRUE(tile.findFirstObjectBorderPixel(&object, pnt));
    EXPECT_EQ(cv::Point(40, 62), pnt);
}

TEST(Tile, findNextObjectBorderPoint_shape1) {
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

    cv::Point offset(5, 8);
    cv::Rect boundingRect(xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);
    boundingRect += offset;

    for(auto& pnt : borderPoints) {
        pnt += offset;
    }

    Tile tile(mask, offset);

    ImageObject object;
    object.m_id = 1;
    object.m_boundingRect = boundingRect;

    cv::Point current;
    ASSERT_TRUE(tile.findFirstObjectBorderPixel(&object, current));
    ASSERT_EQ(borderPoints.front(), current);
    borderPoints.pop_front();
    cv::Point prev = current + cv::Point(0, 1);
    for(const auto& pnt : borderPoints) {
        cv::Point nextPrev, nextCurrent;
        EXPECT_TRUE(tile.findNextObjectBorderPixel(&object, prev, current, nextPrev, nextCurrent));
        //std::cout << current << " -> " << nextCurrent << std::endl;
        prev = nextPrev;
        current = nextCurrent;
        EXPECT_EQ(current, pnt);
    }
}


TEST(Tile, findNextObjectBorderPoint_shape2) {

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

    cv::Point current;
    ASSERT_TRUE(tile.findFirstObjectBorderPixel(&object, current));
    ASSERT_EQ(borderPoints.front(), current);
    borderPoints.pop_front();
    cv::Point prev = current + cv::Point(0, 1);
    for (const auto& pnt : borderPoints) {
        cv::Point nextPrev, nextCurrent;
        EXPECT_TRUE(tile.findNextObjectBorderPixel(&object, prev, current, nextPrev, nextCurrent));
        //std::cout << current << " -> " << nextCurrent << std::endl;
        prev = nextPrev;
        current = nextCurrent;
        EXPECT_EQ(current, pnt);
    }
}

TEST(Tile, isPerimeterLine) {
    std::shared_ptr<ImageObjectManager> imgObjMngr = std::make_shared<ImageObjectManager>();
    cv::Mat mask(10, 10, CV_32S);
    mask.setTo(0);
    int32_t id = 1;
    for (int y = 0; y < mask.rows; ++y) {
        for (int x = 0; x < mask.cols; ++x) {
            mask.at<int32_t>(cv::Point(x, y)) = id++;
        }
    }
    id = mask.at<int32_t>(cv::Point(5, 5));
    const Tile tile(mask, cv::Point(0,0));

    EXPECT_FALSE(tile.isPerimeterLine(cv::Point(5, 7), cv::Point(5, 6), id));
    EXPECT_TRUE(tile.isPerimeterLine(cv::Point(5, 6), cv::Point(5, 5), id));
    EXPECT_TRUE(tile.isPerimeterLine(cv::Point(5, 5), cv::Point(5, 6), id));
    EXPECT_FALSE(tile.isPerimeterLine(cv::Point(5, 6), cv::Point(4, 6), id));
    EXPECT_TRUE(tile.isPerimeterLine(cv::Point(5, 5), cv::Point(6, 5), id));
}

TEST(Tile, getLineNeighborId) {
    cv::Mat mask(10, 10, CV_32S);
    mask.setTo(0);
    int32_t id = 1;
    for (int y = 0; y < mask.rows; ++y) {
        for (int x = 0; x < mask.cols; ++x) {
            mask.at<int32_t>(cv::Point(x, y)) = id++;
        }
    }
    const Tile tile(mask, cv::Point(0, 0));

    id = mask.at<int32_t>(cv::Point(5, 5));
    ASSERT_EQ(56, id);

    EXPECT_EQ(65, tile.getLineNeighborId(cv::Point(5, 7), cv::Point(5, 6), 66));
    EXPECT_EQ(56, tile.getLineNeighborId(cv::Point(5, 6), cv::Point(6, 6), 66));
    EXPECT_EQ(-1, tile.getLineNeighborId(cv::Point(5, 6), cv::Point(6, 6), 166));
    EXPECT_EQ(-1, tile.getLineNeighborId(cv::Point(2, 2), cv::Point(6, 6), 166));
    EXPECT_EQ(-1, tile.getLineNeighborId(cv::Point(5, 6), cv::Point(6, 5), 56));
}
