// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <cstdint>
#include <ostream>
#include <algorithm>
#include <climits>

namespace cv { class Range; }

namespace slideio
{
    class Range
    {
    public:
        Range() : start(0), end(0) {}
        Range(int32_t _start, int32_t _end) : start(_start), end(_end) {}
        Range(const cv::Range& r);
        Range(const Range& r) = default;
        Range(Range&& r) noexcept {
            start = r.start;
            end = r.end;
            r.start = 0;
            r.end = 0;
        }
        ~Range() = default;
        Range& operator = (const Range& r) = default;
        Range& operator = (const cv::Range& r);
        Range& operator = (Range&& r) noexcept {
            if (this != &r) {
                start = r.start;
                end = r.end;
                r.start = 0;
                r.end = 0;
            }
            return *this;
        }
        
        operator cv::Range() const;
        
        int32_t size() const {
            return end - start;
        }
        
        bool empty() const {
            return start == end;
        }
        
        static Range all() {
            return Range(INT_MIN, INT_MAX);
        }
        
        bool operator == (const Range& other) const {
            return start == other.start && end == other.end;
        }
        
        bool operator != (const Range& other) const {
            return !(*this == other);
        }
        
        friend Range operator & (const Range& r1, const Range& r2) {
            Range r(std::max(r1.start, r2.start), std::min(r1.end, r2.end));
            r.end = std::max(r.end, r.start);
            return r;
        }
        
        Range& operator &= (const Range& r) {
            *this = *this & r;
            return *this;
        }
        
        friend Range operator + (const Range& r, int32_t delta) {
            return Range(r.start + delta, r.end + delta);
        }
        
        friend Range operator + (int32_t delta, const Range& r) {
            return Range(r.start + delta, r.end + delta);
        }
        
        friend Range operator - (const Range& r, int32_t delta) {
            return r + (-delta);
        }
        
        friend std::ostream& operator<<(std::ostream& os, const Range& range) {
            os << "Range (start: " << range.start << ", end: " << range.end << ")";
            return os;
        }
        
        int32_t start;
        int32_t end;
    };
}

#if defined(SLIDEIO_INTERNAL_HEADER)
#include "range.inl"
#endif
