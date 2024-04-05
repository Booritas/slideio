#include <gtest/gtest.h>

#include "slideio/core/tools/tempfile.hpp"
#include "slideio/processor/processor.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/processor/processortools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/processor/project.hpp"
#include "slideio/processor/tile.hpp"

using namespace slideio;

TEST(Processor, simple)
{
   std::string pathJpg = TestTools::getTestImagePath("jpeg", "p2YCpvg.jpeg");
   const slideio::TempFile storageFile("h5");
   const std::string storagePath = storageFile.getPath().string();

   std::shared_ptr<CVSlide> slide = ImageDriverManager::openSlide(pathJpg, "GDAL");
   std::shared_ptr<CVScene> scene = slide->getScene(0);
   std::shared_ptr<Storage> storage = Storage::createStorage(storagePath, scene->getRect().size());
   std::shared_ptr<Project> project = std::make_shared<Project>(scene, storage);
   Processor::multiResolutionSegmentation(project, 0, 0, 0);
}

TEST(ProcessorTools, nextMoveCW)
{
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
   for (const auto &t : test)
   {
      const cv::Point result = ProcessorTools::nextMoveCW(t.current, t.center);
      EXPECT_EQ(result, t.expected);
   }
}

TEST(Tile, contains)
{
   cv::Point offset(10, 10);
   cv::Mat mask(20, 20, CV_32SC1, cv::Scalar(255));
   Tile tile(mask, offset);

   EXPECT_TRUE(tile.contains(cv::Point(15, 15)));
   EXPECT_TRUE(tile.contains(cv::Point(10, 10)));
   EXPECT_FALSE(tile.contains(cv::Point(5, 15)));
   EXPECT_FALSE(tile.contains(cv::Point(30, 15)));
}

TEST(Tile, doesObjectContain1)
{
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

TEST(Tile, doesObjectContain2)
{
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

TEST(Tile, isOnObjectBorder1)
{
   auto comp = [](const cv::Point& left, const cv::Point& right){
      return left.x < right.x || (left.x == right.x && left.y < right.y);};

   std::set<cv::Point, decltype(comp)> borderPoints(comp);
   std::list<cv::Point> borderList =
   {
      {5,5}, {5,6}, {5,7},
      {6,5}, {7,5}, {8,5}
   };
   borderPoints.insert(borderList.begin(), borderList.end());

   cv::Mat mask(15, 15, CV_32S);
   mask.setTo(0);
   int xMin = 0, xMax = mask.cols - 1;
   int yMin = 0, yMax = mask.rows - 1;

   for(const auto& point : borderPoints) {
      mask.at<int32_t>(point) = 1;
      xMin = std::min(xMin, point.x);
      xMax = std::max(xMax, point.x);
      yMin = std::min(yMin, point.y);
      yMax = std::max(yMax, point.y);
   }

   cv::Point offset(4, 5);
   cv::Rect boundingRect(xMin, yMin, xMax-xMin+1, yMax-yMin+1);
   boundingRect += offset;

   Tile tile(mask, offset);

   ImageObject object;
   object.m_id = 1;
   object.m_boundingRect = boundingRect;

   for(int y=0; y<mask.rows; ++y)
   {
      for(int x=0; x<mask.cols; ++x)
      {
         cv::Point point(x, y);
         point += offset;
         if(tile.isOnObjectBorder(point, &object))
         {
            EXPECT_TRUE(borderPoints.find(point) != borderPoints.end());
            borderPoints.erase(point);
         }
      }
   }
   EXPECT_TRUE(borderPoints.empty());
}
