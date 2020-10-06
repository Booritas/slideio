// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.#pragma once
#ifndef OPENCV_slideio_zvitags_HPP
#define OPENCV_slideio_zvitags_HPP

namespace slideio
{
    enum class ZVITAG
    {
        ZVITAG_COMPRESSION = 222,
        ZVITAG_DATE_MAPPING_TABLE = 257,
        ZVITAG_BLACK_VALUE = 258,
        ZVITAG_WHITE_VALUE = 259,
        ZVITAG_IMAGE_WIDTH = 515,
        ZVITAG_IMAGE_HEIGHT = 516,
        ZVITAG_IMAGE_COUNT = 517,
        ZVITAG_IMAGE_PIXEL_TYPE = 518,
        ZVITAG_NUMBER_RAW_IMAGES = 519,
        ZVITAG_SCALE_X = 769,
        ZVITAG_SCALE_UNIT_X = 770,
        ZVITAG_SCALE_WIDTH = 771,
        ZVITAG_SCALE_Y = 772,
        ZVITAG_SCALE_UNIT_Y = 773,
        ZVITAG_SCALE_HEIGHT = 774,
        ZVITAG_SCALE_Z = 775,
        ZVITAG_SCALE_UNIT_Z = 776,
        ZVITAG_SCALE_DEPTH = 777,
        ZVITAG_MULTICHANNEL_COLOUR = 1282,
        ZVITAG_CHANNEL_NAME = 1284,
        ZVITAG_STAGE_POSITION_X = 2073,
        ZVITAG_STAGE_POSITION_Y = 2074,
        ZVITAG_MAGNIFICATION = 2076,
        ZVITAG_IMAGE_INDEX_U = 2817,
        ZVITAG_IMAGE_INDEX_V = 2818,
        ZVITAG_IMAGE_INDEX_Z = 2819,
        ZVITAG_IMAGE_INDEX_C = 2820,
        ZVITAG_IMAGE_INDEX_T = 2821,
        ZVITAG_IMAGE_TILE_INDEX = 2822,
        ZVITAG_IMAGE_ACQUSITION_INDEX = 2823,
        ZVITAG_IMAGE_COUNT_TILES = 2824,
        ZVITAG_IMAGE_COUNT_A = 2815,
        ZVITAG_IMAGE_INDEX_S = 2827,
        ZVITAG_IMAGE_INDEX_RAW = 2828,
        ZVITAG_IMAGE_COUNT_Z = 2832,
        ZVITAG_IMAGE_COUNT_C = 2833,
        ZVITAG_IMAGE_COUNT_T = 2834,
        ZVITAG_IMAGE_COUNT_U = 2838,
        ZVITAG_IMAGE_COUNT_V = 2839,
        ZVITAG_IMAGE_COUNT_S = 2840,
    };

}


#endif