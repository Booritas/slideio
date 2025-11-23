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
    int z;
};

struct SCNTilingInfo
{
    const slideio::TiffDirectory* getValidDir() const{
        const slideio::TiffDirectory *dir = nullptr;
        for (auto it = channel2ifd.begin(); it != channel2ifd.end(); ++it) {
            if (it->second) {
                dir = it->second;
                break;
            }
        }
        return dir;
    }
	const slideio::TiffDirectory* getChannelDir(int channel) const {
		auto it = channel2ifd.find(channel);
		if (it != channel2ifd.end()) {
			return it->second;
		}
		return nullptr;
	}
    std::map<int, const slideio::TiffDirectory*> channel2ifd;
};

