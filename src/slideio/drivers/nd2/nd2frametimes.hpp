// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <vector>

namespace slideio
{
    namespace nd2
    {
        /// Frame times in milliseconds by frame index (design 6.4); NaN where unknown.
        /// A frame's time comes from `table` (the `CustomData|AcqTimesCache!` values) when
        /// the table covers it, else from `read(frame)`, which runs the first time that
        /// frame is asked for, and the answer is kept. Nothing is sized by the frame count,
        /// so a file declaring 2^31 frames costs nothing until its times are read, and
        /// opening a file reads only the times the T fit needs (PR review).
        /// Copies share one store; safe to call from several threads at once.
        class SLIDEIO_ND2_EXPORTS FrameTimes
        {
        public:
            /// NaN when the frame has no time.
            using Reader = std::function<double(int64_t frame)>;

            FrameTimes() = default;
            /// `count` frames: the table's entries first, `read` for the rest (NaN without one).
            FrameTimes(std::vector<double> table, int64_t count, Reader read = Reader());
            /// Every time known up front.
            FrameTimes(std::vector<double> times);
            FrameTimes(std::initializer_list<double> times);

            int64_t size() const { return m_count; }
            /// Frames past the new count read as unknown.
            void resize(int64_t count) { m_count = count; }
            /// NaN when `frame` is outside [0, size()) or its time is unknown.
            double operator[](int64_t frame) const;

        private:
            struct Store;
            std::shared_ptr<Store> m_store;
            int64_t m_count = 0;
        };
    }
}
