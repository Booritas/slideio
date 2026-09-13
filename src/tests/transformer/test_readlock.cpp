// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include "tests/testlib/testscene.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/transformer/transformer.hpp"
#include "slideio/transformer/gaussianblurfilter.hpp"

using namespace slideio;

namespace
{
    // A non-concurrent origin that reports whether two reads of it ever overlapped.
    //
    // TestScene leaves supportsConcurrentReads() at the CVScene default of false,
    // which is what dcm and gdal do -- so this stands in for exactly the scenes
    // the base class is supposed to be serialising.
    //
    // The first thread to arrive parks inside the read for a bounded interval and
    // watches for a second thread entering behind it. It never waits on the second
    // thread, so the test terminates whether or not the exclusion holds: if reads
    // are properly serialised the second thread cannot enter, the first one times
    // out, and both complete in sequence.
    class OverlapDetectingScene : public TestScene
    {
    public:
        static constexpr auto kParkTime = std::chrono::milliseconds(300);

        OverlapDetectingScene() {
            setChannelDataType(DataType::DT_Byte);
        }

        void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                          const std::vector<int>& componentIndices, int zSliceIndex,
                                          int tFrameIndex, cv::OutputArray output) override {
            // Decrement on every exit, including an exception: a counter left
            // armed would make the *next* read report an overlap that never
            // happened, turning a read failure into a bogus concurrency failure.
            struct Departure {
                std::atomic<int>& counter;
                ~Departure() { --counter; }
            } departure{m_inside};

            if (++m_inside > 1) {
                m_overlapped = true;
            }
            if (!m_parked.exchange(true)) {
                // Only the first arrival parks, so the test cannot stall for
                // more than one kParkTime however many threads run. It never
                // waits *for* the second thread, so the test terminates whether
                // or not the exclusion holds.
                const auto deadline = std::chrono::steady_clock::now() + kParkTime;
                while (std::chrono::steady_clock::now() < deadline && m_inside.load() < 2) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if (m_inside.load() > 1) {
                    m_overlapped = true;
                }
            }
            allocateBlock(blockSize, componentIndices, output);
        }

        bool overlapped() const { return m_overlapped.load(); }

    private:
        // Deliberately does NOT call TestScene::readResampledBlockChannelsEx.
        // That records every call in a plain std::vector, and this scene exists
        // to have two threads inside it at once -- the concurrent case asserts
        // exactly that -- so delegating would be an unsynchronised concurrent
        // push_back: undefined behaviour, and noise in any future sanitizer run.
        // The transform chain needs nothing from the block but its shape.
        void allocateBlock(const cv::Size& blockSize, const std::vector<int>& componentIndices,
                           cv::OutputArray output) const {
            if (!output.needed()) {
                return;
            }
            const int channels = componentIndices.empty() ? getNumChannels()
                                                          : static_cast<int>(componentIndices.size());
            output.create(blockSize, CV_8UC(channels));
            output.getMat().setTo(cv::Scalar::all(0));
        }

        std::atomic<int> m_inside{0};
        std::atomic<bool> m_parked{false};
        std::atomic<bool> m_overlapped{false};
    };

    // Reads through the CVScene rather than the Scene wrapper: the public
    // readBlock family of Scene writes into a caller-supplied buffer, and it is
    // CVScene's public layer that carries the lock under test.
    void readWholeScene(const std::shared_ptr<Scene>& scene) {
        std::shared_ptr<CVScene> cvScene = scene->getCVScene();
        cv::Mat raster;
        cvScene->readBlock(cvScene->getRect(), raster);
    }
}

TEST(TransformedSceneReadLock, twoTransformsOverOneOriginDoNotReadItConcurrently)
{
    // Each TransformerScene used to take its own mutex, so neither excluded the
    // other and both entered the non-concurrent origin at once.
    auto origin = std::make_shared<OverlapDetectingScene>();
    auto originScene = std::make_shared<Scene>(origin);
    GaussianBlurFilter filter;
    std::shared_ptr<Scene> first = transformScene(originScene, filter);
    std::shared_ptr<Scene> second = transformScene(originScene, filter);

    std::thread a([&] { readWholeScene(first); });
    std::thread b([&] { readWholeScene(second); });
    a.join();
    b.join();

    ASSERT_FALSE(origin->overlapped());
}

TEST(TransformedSceneReadLock, aDirectReadOfTheOriginExcludesATransformedRead)
{
    // The direct read took the origin's mutex and the transformed read took the
    // transformer's -- different mutexes, so they did not exclude each other.
    auto origin = std::make_shared<OverlapDetectingScene>();
    auto originScene = std::make_shared<Scene>(origin);
    GaussianBlurFilter filter;
    std::shared_ptr<Scene> transformed = transformScene(originScene, filter);

    std::thread a([&] { readWholeScene(transformed); });
    std::thread b([&] { readWholeScene(originScene); });
    a.join();
    b.join();

    ASSERT_FALSE(origin->overlapped());
}

TEST(TransformedSceneReadLock, aConcurrentOriginIsStillReadConcurrentlyThroughATransform)
{
    // The fix must not re-serialise what §22's forwarding made concurrent: a
    // scene that reports concurrent reads is still read by two threads at once
    // through a transform. Without the overlap this test cannot distinguish a
    // working forward from a lock taken on every read, so it asserts the
    // overlap happened rather than that it did not.
    class ConcurrentScene : public OverlapDetectingScene
    {
    public:
        bool supportsConcurrentReads() const override { return true; }
    };
    auto origin = std::make_shared<ConcurrentScene>();
    auto originScene = std::make_shared<Scene>(origin);
    GaussianBlurFilter filter;
    std::shared_ptr<Scene> first = transformScene(originScene, filter);
    std::shared_ptr<Scene> second = transformScene(originScene, filter);

    std::thread a([&] { readWholeScene(first); });
    std::thread b([&] { readWholeScene(second); });
    a.join();
    b.join();

    ASSERT_TRUE(origin->overlapped());
}
