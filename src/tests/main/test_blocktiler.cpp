#include <gtest/gtest.h>
#include "slideio-opencv/core.hpp"

#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/blocktiler.hpp"
#include "slideio/core/tools/tilevisitor.hpp"

class MockTileVisitor : public slideio::ITileVisitor {
public:
    void visit(int x, int y, const cv::Mat& tile) override {
        m_tilePositions[x + y*10000] = tile;
    }
    std::map<int,cv::Mat> m_tilePositions;
};

TEST(BlockTiler, ApplyVisitor) {
    const cv::Mat block(100, 90, CV_8UC1); // Create a block of size 100x90 with 3 channels
    const cv::Size tileSize(9, 10); // Set the tile size to 9x10
    // Fill the block with a chessboard pattern
    int y = 0, x = 0;
    for (int i = 0; i < block.rows; i += tileSize.height, y++) {
        x = 0;
        for (int j = 0; j < block.cols; j += tileSize.width, x++) {
            cv::Rect rect(j, i, tileSize.width, tileSize.height);
            block(rect).setTo(cv::Scalar::all(y*10 + x));
        }
    }

    const int tileCountX = (block.cols-1) / tileSize.width + 1;
    const int tileCountY = (block.rows-1) / tileSize.height + 1;
    slideio::BlockTiler tiler(block, tileSize);
    MockTileVisitor visitor;
    EXPECT_NO_THROW(tiler.apply(&visitor));
    EXPECT_EQ(visitor.m_tilePositions.size(), tileCountX*tileCountY);
    const auto& positions = visitor.m_tilePositions;
    for(int tileY = 0; tileY < tileCountY; ++tileY) {
        for(int tileX = 0; tileX < tileCountX; ++tileX) {
            int id = tileX + tileY*10000;
            EXPECT_TRUE(positions.find(id) != positions.end());
            cv::Mat tile = positions.at(id);
            // Compute min and max values of the tile
            double minVal, maxVal;
            cv::Point minLoc, maxLoc;
            cv::minMaxLoc(tile, &minVal, &maxVal, &minLoc, &maxLoc);
            EXPECT_EQ(int(minVal), int(maxVal));
            EXPECT_EQ(minVal, tileY * 10 + tileX);
        }
    }
}

TEST(BlockTiler, InvalidTileSize) {
    cv::Mat block(100, 100, CV_8UC3); // Create a block of size 100x100 with 3 channels
    cv::Size tileSize(0, 0); // Set an invalid tile size
    EXPECT_THROW(slideio::BlockTiler tiler(block, tileSize), slideio::RuntimeError);
}

TEST(BlockTiler, EmptyBlock) {
    cv::Mat block; // Create an empty block
    cv::Size tileSize(10, 10); // Set the tile size to 10x10
    EXPECT_THROW(slideio::BlockTiler tiler(block, tileSize), slideio::RuntimeError);
}

TEST(BlockTiler, NullVisitor) {
    cv::Mat block(100, 100, CV_8UC3); // Create a block of size 100x100 with 3 channels
    cv::Size tileSize(10, 10); // Set the tile size to 10x10
    slideio::BlockTiler tiler(block, tileSize);
    EXPECT_THROW(tiler.apply(nullptr), slideio::RuntimeError);
}

TEST(BlockTiler, TileSize) {
    const cv::Mat block(105, 94, CV_8UC1); // Create a block of size 100x90 with 3 channels
    const cv::Size tileSize(9, 10); // Set the tile size to 9x10
    const int tileCountX = (block.cols - 1) / tileSize.width + 1;
    const int tileCountY = (block.rows - 1) / tileSize.height + 1;
    // Fill the block with a chessboard pattern
    int y = 0, x = 0, tileWidth=0, tileHeight=0;
    for (int i = 0; i < block.rows; i += tileSize.height, y++) {
        x = 0;
        tileHeight = tileSize.height;
        if(y == (tileCountY - 1)) {
            tileHeight = block.rows - y * tileSize.height;
            ASSERT_EQ(tileHeight, 5);
        }
        for (int j = 0; j < block.cols; j += tileSize.width, x++) {
            tileWidth = tileSize.width;
            if(x== (tileCountX - 1)) {
                tileWidth = block.cols - x * tileSize.width;
                ASSERT_EQ(tileWidth, 4);
            }
            cv::Rect rect(j, i, tileWidth, tileHeight);
            block(rect).setTo(cv::Scalar::all(y * 10 + x));
        }
    }

    slideio::BlockTiler tiler(block, tileSize);
    MockTileVisitor visitor;
    ASSERT_NO_THROW(tiler.apply(&visitor));
    EXPECT_EQ(visitor.m_tilePositions.size(), tileCountX * tileCountY);
    const auto& positions = visitor.m_tilePositions;
    for (int tileY = 0; tileY < tileCountY; ++tileY) {
        for (int tileX = 0; tileX < tileCountX; ++tileX) {
            int id = tileX + tileY * 10000;
            EXPECT_TRUE(positions.find(id) != positions.end());
            cv::Mat tile = positions.at(id);
            if(tileX == (tileCountX - 1)) {
                ASSERT_EQ(tile.cols, 4);
            }
            else {
                ASSERT_EQ(tile.cols, 9);
            }
            // Compute min and max values of the tile
            double minVal, maxVal;
            cv::Point minLoc, maxLoc;
            cv::minMaxLoc(tile, &minVal, &maxVal, &minLoc, &maxLoc);
            EXPECT_EQ(int(minVal), int(maxVal));
            EXPECT_EQ(minVal, tileY * 10 + tileX);
        }
    }
}
