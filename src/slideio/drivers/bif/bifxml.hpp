// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/bif/bif_api_def.hpp"
#include <opencv2/core/types.hpp>
#include <string>
#include <vector>

namespace slideio::bif
{
    struct SLIDEIO_BIF_EXPORTS AcquisitionInfo
    {
        double scanResMicrons = 0;
        double magnification = 0;
        int zLayers = 1;
        std::string scannerModel;
    };

    // The root is <iScan>, or <Metadata> with an <iScan> child; anything else throws
    // "BIF: not a Ventana BIF file (no iScan metadata in IFD 0)".
    SLIDEIO_BIF_EXPORTS AcquisitionInfo parseAcquisitionXml(const std::string& xml);

    enum class JointAxis { Horizontal, Vertical };   // RIGHT/LEFT, UP

    struct SLIDEIO_BIF_EXPORTS Joint
    {
        int tile1 = 0;
        int tile2 = 0;
        JointAxis axis = JointAxis::Horizontal;
        bool joined = false;
        int confidence = 0;
        double overlapX = 0;
        double overlapY = 0;
        bool usable() const { return joined && confidence > 0; }
    };

    struct SLIDEIO_BIF_EXPORTS Aoi
    {
        int index = 0;
        bool scanned = false;
        int originX = 0;
        int originY = 0;
        int tileWidth = 0;
        int tileHeight = 0;
        int cols = 0;
        int rows = 0;
        std::vector<Joint> joints;     // scanned AOIs only; usable joints only
        std::string imageInfoXml;      // <ImageInfo> without its children, serialised
    };

    struct SLIDEIO_BIF_EXPORTS StitchInfo
    {
        std::vector<Aoi> aois;         // document order, unscanned included
    };

    // Throws on malformed or inconsistent XML.
    SLIDEIO_BIF_EXPORTS StitchInfo parseStitchXml(const std::string& xml);

    // One-based snake tile number -> (col,row), top-left origin. Tile 1 is the bottom-left
    // tile; rows alternate direction. Throws if tileNumber is outside 1..cols*rows.
    SLIDEIO_BIF_EXPORTS cv::Point snakePosition(int tileNumber, int cols, int rows);
}
