// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <cstdint>
#include <ostream>

namespace slideio
{
    class Size
    {
    public:
        Size() {
            width = 0;
            height = 0;
        }
        Size(int32_t _width, int32_t _height) {
            width = _width;
            height = _height;
        }
        Size(const Size& sz) {
            width = sz.width;
            height = sz.height;
        }
        Size(Size&& sz) noexcept {
            width = sz.width;
            height = sz.height;
            sz.width = 0;
            sz.height = 0;
        }
        ~Size() = default;
        Size& operator = (const Size& sz) = default;
        Size& operator = (Size&& sz) noexcept {
            if (this != &sz) {
                width = sz.width;
                height = sz.height;
                sz.width = 0;
                sz.height = 0;
            }
            return *this;
        }
        bool operator == (const Size& other) const {
            return width == other.width && height == other.height;
        }

        int32_t area() const {
            return width * height;
        }
        bool empty() const {
            return width <= 0 || height <= 0;
        }

        friend std::ostream& operator<<(std::ostream& os, const Size& size) {
            os << "Size (width: " << size.width << ", height: " << size.height << ")";
            return os;
        }
        int32_t width;
        int32_t height;
    };
}
