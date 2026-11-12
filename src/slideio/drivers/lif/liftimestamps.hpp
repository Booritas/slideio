// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/lif/lif_api_def.hpp"
#include "slideio/drivers/lif/liflayout.hpp"
#include <cstdint>
#include <vector>

namespace tinyxml2
{
    class XMLElement;
}

namespace slideio
{
    namespace lif
    {
        /// Timestamps of an image, in FILETIME ticks (100 ns), in storage order.
        ///
        /// The argument is the image's `Data/Image` element, NOT its owning
        /// `Element`: both encodings of the list (`TimeStampList` with a hex
        /// text body, and `TimeStampList` with `TimeStamp HighInteger/LowInteger`
        /// children) are direct children of `Image`, beside `ImageDescription`
        /// (see auxfiles/lif-fixtures/generate.py, fixture `timestamps`).
        /// Returns an empty vector when the image carries none.
        SLIDEIO_LIF_EXPORTS std::vector<uint64_t> parseTimestampTicks(const tinyxml2::XMLElement* image);

        /// The timestamps of one field, addressed by (t, c, z) instead of by a
        /// position in the file's list (spec D 6.3).
        ///
        /// Offsets are measured from acquisitionTime(), which is the field's earliest
        /// plane floored to a whole second. Flooring while measuring from the exact
        /// earliest tick would make acquisitionTime() + offset() miss the real time
        /// by the fractional second, breaking the CVScene contract; measuring from the
        /// floored origin keeps the sum exact and offsets >= 0. The cost: the earliest
        /// plane reads its sub-second fraction rather than 0.
        class SLIDEIO_LIF_EXPORTS PlaneTimes
        {
        public:
            /// ticks: FILETIME ticks in storage order for the whole image.
            static PlaneTimes build(const std::vector<uint64_t>& ticks, const Layout& layout, int field);
            /// Exactly one tick per stored plane: (interleaved ? 1 : C) * Z * T * M.
            bool complete() const { return m_complete; }
            /// Unix seconds (floor) of the field's earliest plane; 0 when incomplete.
            int64_t acquisitionTime() const { return m_acquisition; }
            /// Seconds from acquisitionTime(), >= 0; 0 when incomplete or out of range.
            double offset(int t, int c, int z) const;

        private:
            bool m_complete = false;
            int64_t m_acquisition = 0;
            int m_numChannels = 0, m_numZ = 0, m_numT = 0;
            std::vector<double> m_offsets;   // (t * Z + z) * C + c
        };
    }
}
