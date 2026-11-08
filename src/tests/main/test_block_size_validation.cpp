// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// A negative block size is the caller's error. Every read path rejects it with a
// RuntimeError naming the size, rather than letting OpenCV throw cv::Exception from
// a Mat allocation or a buffer-size comparison report a misleading "buffer too small"
// (issue #88). A zero-size block stays a no-op.
#include <gtest/gtest.h>
#include <functional>
#include <string>
#include <vector>
#include "tests/testlib/testscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/slideio/sceneinternal.hpp"

using namespace slideio;

namespace
{
    std::shared_ptr<TestScene> pyramid()
    {
        auto scene = std::make_shared<TestScene>();
        scene->setRect(cv::Rect(0, 0, 400, 300));
        scene->setNumChannels(3);
        scene->setChannelDataType(DataType::DT_Byte);
        scene->addLevel(LevelInfo(0, {400, 300}, 1.0, 20.0, {256, 256}));
        scene->addLevel(LevelInfo(1, {200, 150}, 0.5, 10.0, {256, 256}));
        return scene;
    }

    // The read raises slideio::RuntimeError, and its message names the block size.
    ::testing::AssertionResult rejectsBlockSize(const std::function<void()>& read)
    {
        try {
            read();
        }
        catch (const RuntimeError& error) {
            const std::string message = error.what();
            if (message.find("block size") != std::string::npos) {
                return ::testing::AssertionSuccess();
            }
            return ::testing::AssertionFailure() << "RuntimeError does not name the block size: " << message;
        }
        catch (const std::exception& error) {
            return ::testing::AssertionFailure() << "threw something other than RuntimeError: " << error.what();
        }
        return ::testing::AssertionFailure() << "threw nothing";
    }

    const cv::Rect rect(0, 0, 100, 100);
    const std::vector<cv::Size> negativeSizes{{-1, 100}, {100, -1}, {-1, -100}};
}

TEST(BlockSizeValidation, aResampledReadRejectsANegativeBlockSize)
{
    auto scene = pyramid();
    for (const cv::Size& size : negativeSizes) {
        cv::Mat block;
        EXPECT_TRUE(rejectsBlockSize([&] { scene->readResampledBlockChannels(rect, size, {}, block); }))
            << size;
    }
}

TEST(BlockSizeValidation, a4DReadRejectsANegativeBlockSize)
{
    auto scene = pyramid();
    for (const cv::Size& size : negativeSizes) {
        cv::Mat block;
        EXPECT_TRUE(rejectsBlockSize([&] {
            scene->readResampled4DBlockChannels(rect, size, {}, cv::Range(0, 1), cv::Range(0, 1), block);
        })) << size;
    }
}

TEST(BlockSizeValidation, aLevelReadRejectsANegativeBlockSize)
{
    auto scene = pyramid();
    for (const cv::Size& size : negativeSizes) {
        cv::Mat block;
        EXPECT_TRUE(rejectsBlockSize([&] { scene->readResampledLevelBlockChannels(1, rect, size, {}, block); }))
            << size;
        EXPECT_TRUE(rejectsBlockSize([&] {
            scene->readResampledLevel4DBlockChannels(1, rect, size, {}, cv::Range(0, 1), cv::Range(0, 1), block);
        })) << size;
    }
}

TEST(BlockSizeValidation, aBufferReadThroughTheSceneRejectsANegativeBlockSize)
{
    auto scene = SceneInternal::createScene(pyramid());
    std::vector<uint8_t> buffer(100 * 100 * 3);
    const auto blockRect = std::make_tuple(0, 0, 100, 100);
    for (const cv::Size& size : negativeSizes) {
        const auto blockSize = std::make_tuple(size.width, size.height);
        EXPECT_TRUE(rejectsBlockSize([&] {
            scene->readResampledBlockChannels(blockRect, blockSize, {}, buffer.data(), buffer.size());
        })) << size;
        EXPECT_TRUE(rejectsBlockSize([&] {
            scene->readResampled4DBlockChannels(blockRect, blockSize, {}, std::make_tuple(0, 1),
                                                std::make_tuple(0, 1), buffer.data(), buffer.size());
        })) << size;
        EXPECT_TRUE(rejectsBlockSize([&] {
            scene->readResampledLevelBlockChannels(1, blockRect, blockSize, {}, buffer.data(), buffer.size());
        })) << size;
        EXPECT_TRUE(rejectsBlockSize([&] {
            scene->readResampledLevel4DBlockChannels(1, blockRect, blockSize, {}, std::make_tuple(0, 1),
                                                     std::make_tuple(0, 1), buffer.data(), buffer.size());
        })) << size;
    }
}

TEST(BlockSizeValidation, aZeroSizeLevelReadIsANoOp)
{
    auto scene = pyramid();
    cv::Mat block;
    EXPECT_NO_THROW(scene->readResampledLevelBlockChannels(1, rect, cv::Size(0, 100), {}, block));
    EXPECT_NO_THROW(scene->readResampledLevelBlockChannels(1, rect, cv::Size(100, 0), {}, block));
}
