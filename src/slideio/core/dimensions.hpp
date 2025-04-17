// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/base/exceptions.hpp"
#include <list>
#include <map>
#include <string>
#include <array>


namespace slideio
{
    template <std::size_t N = 3>
    class Dimensions
    {
    public:
		typedef std::array<int, N> Coordinates;
		typedef std::array<int, N> Sizes;
		typedef std::array<int, N> Increments;
		typedef std::array<std::string, N> Labels;
        Dimensions() = default;
        Dimensions(const Labels& labels, const Sizes& dimensionSizes, const Increments& increments) {
            init(labels, dimensionSizes, increments);
        }
        void init(const Labels& labels, const Sizes& dimensionSizes, const Increments& increments) {
            m_order = labels;
            m_sizes = dimensionSizes;
            m_increments = increments;
            m_dimensionMap.clear();
            for (std::size_t i = 0; i < N; ++i) {
                m_dimensionMap[m_order[i]] = static_cast<int>(i);
            }
			for (int i = 0; i < N; ++i) {
				if (m_sizes[i] <= 0) {
					RAISE_RUNTIME_ERROR << "Dimensions::init: dimension size must be greater than zero";
				}
			}
        }
        bool incrementCoordinates(Coordinates& coordinates) const {
            for (std::size_t i = 0; i < N; ++i) {
                int coord = coordinates[i] + m_increments[i];
                if (coord < m_sizes[i]) {
                    coordinates[i] = coord;
                    return true;
                }
                coordinates[i] = 0;
            }
            return false; // all dimensions are at their max size
        }
        Coordinates createCoordinates(const std::list<std::pair<std::string, int>>& coordList) const {
            Coordinates coordinates = {0};
            for (const auto& dim : coordList) {
                auto it = m_dimensionMap.find(dim.first);
                if (it == m_dimensionMap.end()) {
                    RAISE_RUNTIME_ERROR << "Dimensions::createCoordinates: dimension: " << dim.first
                        << " not found in dimension map";
                }
                coordinates[it->second] = dim.second;
            }
            return coordinates;
        }

        const Labels& getOrder() const { return m_order; }
        const Sizes& getSizes() const { return m_sizes; }
        const Increments& getIncrements() const { return m_increments; }

        int getDimensionSize(const std::string& label) const {
            auto it = m_dimensionMap.find(label);
            if (it == m_dimensionMap.end()) {
                RAISE_RUNTIME_ERROR << "Dimensions::getDimensionSize: dimension: " << label
                    << " not found in dimension map";
            }
            return m_sizes[it->second];
        }

        int getDimensionIndex(const std::string& label) const {
            auto it = m_dimensionMap.find(label);
            if (it == m_dimensionMap.end()) {
                RAISE_RUNTIME_ERROR << "Dimensions::getDimensionIndex: dimension: " << label
                    << " not found in dimension map";
            }
            return it->second;
        }

        int getNumDimensions() const { return N; }

        static bool areCoordsEqual(const Coordinates& lhs, const Coordinates& rhs) {
            for (std::size_t i = 0; i < N; ++i) {
                if (lhs[i] != rhs[i]) {
                    return false;
                }
            }
            return true;
        }

        static bool areCoordsLess(const Coordinates& lhs, const Coordinates& rhs) {
            for (std::size_t i = N; i-- > 0;) {
                if (lhs[i] < rhs[i]) {
                    return true;
                } else if (lhs[i] > rhs[i]) {
                    return false;
                }
            }
            return false;
        }

        static bool areCoordsGreater(const Coordinates& lhs, const Coordinates& rhs) {
            return areCoordsLess(rhs, lhs);
        }

    private:
        Labels m_order;
        Sizes m_sizes;
        Increments m_increments;
        std::map<std::string, int> m_dimensionMap;
    };

}
