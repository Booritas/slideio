// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <cstdint>
#include <ostream>
#include "size.hpp"

namespace slideio
{
    class Rect {
    public:
        Rect() : x(0), y(0), width(0), height(0){}
        Rect(int32_t _x, int32_t _y, int32_t _width, int32_t _height) {
            x = _x;
            y = _y;
            width = _width;
            height = _height;
        }
        Rect(const Rect& r) = default;
        Rect(Rect&& r) noexcept {
            x = r.x;
            y = r.y;
            width = r.width;
            height = r.height;
            r.x = r.y = r.width = r.height = 0;
        }
        ~Rect() = default;
        Rect& operator = (const Rect& r) = default;
        Rect& operator = (Rect&& r) noexcept {
            x = r.x;
            y = r.y;
            width = r.width;
            height = r.height;
            r.x = r.y = r.width = r.height = 0;
            return *this;
        }
        Size size() const {
            return { width, height };
        }
        int32_t area() const {
            return width * height;
        }
        bool empty() const {
            return width <= 0 || height <= 0;
        }
        friend std::ostream& operator<<(std::ostream& os, const Rect& rect) {
            os << "Rect (x: " << rect.x << ", y: " << rect.y << ", width: " << rect.width << ", height: " << rect.height << ")";
            return os;
        }
        bool valid() const {
            return x >= 0 && y >= 0 && width > 0 && height > 0;
        }
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
    };

}
