// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/slideio/scenesource.hpp"
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace slideio
{
    /**@brief A source whose pixels are a pure function of their coordinates.
     *
     * Byte valued, so a test can predict every pixel: see #expected. Records
     * the arguments of every call so a test can assert what it was asked for.
     */
    class TestSceneSource : public ISceneSource
    {
    public:
        struct Call {
            int level;          // -1 for a readBlock call
            Rect rect;
            Size blockSize;
            std::vector<int> channels;
            int zSlice;
            int tFrame;
        };

        static uint8_t expected(int x, int y, int channel) {
            return static_cast<uint8_t>((x * 7 + y * 13 + channel * 31) & 0xFF);
        }

        void setHasPyramid(bool v)            { m_hasPyramid = v; }
        void setConcurrent(bool v)            { m_concurrent = v; }
        void setThrowOnRead(bool v)           { m_throwOnRead = v; }
        /// A snapshot: with setConcurrent(true) reads may still be appending.
        std::vector<Call> calls() const {
            std::lock_guard<std::mutex> lock(m_callsMutex);
            return m_calls;
        }

        void readBlock(const Rect& sceneRect, const Size& blockSize,
                       const std::vector<int>& channelIndices,
                       int zSlice, int tFrame,
                       void* buffer, size_t bufferSize) override {
            record({-1, sceneRect, blockSize, channelIndices, zSlice, tFrame});
            fill(sceneRect, blockSize, channelIndices, buffer, bufferSize);
        }

        bool readLevelBlock(int level, const Rect& levelRect, const Size& blockSize,
                            const std::vector<int>& channelIndices,
                            int zSlice, int tFrame,
                            void* buffer, size_t bufferSize) override {
            if (!m_hasPyramid) {
                return false;
            }
            record({level, levelRect, blockSize, channelIndices, zSlice, tFrame});
            fill(levelRect, blockSize, channelIndices, buffer, bufferSize);
            return true;
        }

        bool supportsConcurrentReads() const override { return m_concurrent; }

    private:
        void record(Call call) {
            std::lock_guard<std::mutex> lock(m_callsMutex);
            m_calls.push_back(std::move(call));
        }

        void fill(const Rect& rect, const Size& blockSize,
                  const std::vector<int>& channels, void* buffer, size_t bufferSize) {
            if (m_throwOnRead) {
                throw std::runtime_error("TestSceneSource: deliberate failure");
            }
            const size_t needed = static_cast<size_t>(blockSize.width) *
                                  blockSize.height * channels.size();
            if (bufferSize < needed) {
                throw std::runtime_error("TestSceneSource: buffer too small");
            }
            uint8_t* out = static_cast<uint8_t*>(buffer);
            for (int row = 0; row < blockSize.height; ++row) {
                for (int col = 0; col < blockSize.width; ++col) {
                    for (size_t c = 0; c < channels.size(); ++c) {
                        *out++ = expected(rect.x + col, rect.y + row, channels[c]);
                    }
                }
            }
        }

        bool m_hasPyramid = false;
        bool m_concurrent = false;
        bool m_throwOnRead = false;
        mutable std::mutex m_callsMutex;
        std::vector<Call> m_calls;
    };
}
