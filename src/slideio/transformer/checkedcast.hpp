// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/exceptions.hpp"
#include "slideio/transformer/transformationtype.hpp"

namespace slideio
{
    // Transformation and TransformationWrapper are public and can be subclassed
    // outside SlideIO, so getType() names a built-in class without proving the
    // object is one. Every downcast from either goes through this rather than a
    // C-style or static cast, which would read an outside subclass as a class it
    // is not (#89).
    template <typename Concrete, typename Base>
    const Concrete& checkedTransformationCast(const Base& source)
    {
        const Concrete* concrete = dynamic_cast<const Concrete*>(&source);
        if (!concrete) {
            RAISE_RUNTIME_ERROR << "Transformation reporting type " << source.getType()
                << " is not the built-in class of that type. "
                   "Transformations defined outside SlideIO are not supported.";
        }
        return *concrete;
    }
}
