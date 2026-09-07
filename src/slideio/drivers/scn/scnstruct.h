#pragma once
#include <map>
#include "slideio/imagetools/tifftools.hpp"

namespace slideio
{
    class SCNReadContext; // defined in scnscene.hpp
}

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

// What Tiler's methods receive as userData for one call to
// readResampledLevelBlockChannelsEx: the per-channel directory map above (immutable, shared
// across threads) plus the context borrowed for the duration of that one call -- the only
// place a TIFF handle enters the read path. Acquired once by the caller and never re-acquired
// mid-read; see SCNScene::acquireContext. Declared here, alongside SCNTilingInfo, rather than
// file-local to scnscene.cpp, so a white-box test driving getTileCount/getTileRect/readTile
// directly can build one that matches the real read path.
struct SCNTileUserData
{
    SCNTilingInfo info;
    slideio::SCNReadContext* context = nullptr;
};

