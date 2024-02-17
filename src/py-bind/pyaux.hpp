// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/levelinfo.hpp"

class PyRect : public std::tuple<int, int, int, int>
{
public:
    PyRect(const std::tuple<int, int, int, int>& tulpe) : std::tuple<int, int, int, int>(tulpe)
    {
    }
    int& x() { return std::get<0>(*this);}
    int& y() { return std::get<1>(*this);}
    int& width() { return std::get<2>(*this);}
    int& height() { return std::get<3>(*this);}
    int x() const { return std::get<0>(*this);}
    int y() const { return std::get<1>(*this);}
    int width() const { return std::get<2>(*this);}
    int height() const { return std::get<3>(*this);}
};

class PySize : public std::tuple<int, int>
{
public:
    PySize(const std::tuple<int, int>& tulpe) : std::tuple<int, int>(tulpe)
    {
    }
    int& width() { return std::get<0>(*this);}
    int& height() { return std::get<1>(*this);}
    int width() const { return std::get<0>(*this);}
    int height() const { return std::get<1>(*this);}
};

class PyZoomLevelInfo : public slideio::LevelInfo
{
public:
    PyZoomLevelInfo(const slideio::LevelInfo& LevelInfo) : slideio::LevelInfo(LevelInfo) {
    }
    int level() { return getLevel();}
    double scale() { return getScale();}
    double magnification() { return getMagnification();}
    PySize tileSize() const {
        const auto& tileSize = getTileSize();
        PySize pySize(std::tuple<int,int>(tileSize.width, tileSize.height));
        return pySize;
    }
    PySize size() const {
        const auto& size = getSize();
        PySize pySize(std::tuple<int, int>(size.width, size.height));
        return pySize;
    }
};