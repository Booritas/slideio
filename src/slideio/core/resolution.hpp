// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <cstdint>
#include <ostream>

namespace slideio
{
    class Resolution
    {
    public:
        Resolution() : x(0), y(0) {}
        Resolution(double _x, double _y) : x(_x), y(_y) {}
        Resolution(const Resolution& pt) = default;
        Resolution(Resolution&& pt) noexcept : x(pt.x), y(pt.y) {
            pt.x = 0;
            pt.y = 0;
        }
        ~Resolution() = default;
        Resolution& operator = (const Resolution& pt) = default;
        Resolution& operator = (Resolution&& pt) noexcept {
            x = pt.x;
            y = pt.y;
            pt.x = 0;
            pt.y = 0;
            return *this;
        }
        bool operator == (const Resolution& pt) const {
            return x == pt.x && y == pt.y;
        }
        double x;
        double y;
    };
    
}
