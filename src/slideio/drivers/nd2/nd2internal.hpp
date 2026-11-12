// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

// Internal to the ND2 driver: not listed among the public headers, not installed.
#include "slideio/drivers/nd2/nd2variant.hpp"
#include <vector>

namespace slideio
{
    namespace nd2
    {
        namespace detail
        {
            /// The elements of a list, or the members of a map whose names are not indices.
            inline std::vector<const Variant*> elementsOf(const Variant* v)
            {
                std::vector<const Variant*> out;
                if (!v) {
                    return out;
                }
                if (!v->items().empty()) {
                    for (const Variant& item : v->items()) {
                        out.push_back(&item);
                    }
                }
                else {
                    for (const auto& member : v->members()) {
                        out.push_back(&member.second);
                    }
                }
                return out;
            }
        }
    }
}
