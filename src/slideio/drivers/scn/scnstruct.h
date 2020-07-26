#pragma once
#include <map>
#include "slideio/imagetools/tifftools.hpp"

struct SCNDimensionInfo
{
    int width;
    int height;
    int r;
    int c;
    int ifd;
};

struct SCNTilingInfo
{
    std::map<int, const slideio::TiffDirectory*> channel2ifd;
};

