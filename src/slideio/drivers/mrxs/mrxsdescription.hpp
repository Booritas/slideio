// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/core/metadata.hpp"

namespace slideio::mrxs
{
    /**@brief the metadata tree of a bundle (design section 6): "ini" holds Slidedat.ini's
     * groups and keys verbatim, "mrxs" what the driver made of them -- rendering profile,
     * position source, nominal and content bounds, per-level size and codec, objective,
     * and the layers it does not render.*/
    MetadataBuilder describe(const MRXSDataset& dataset);
}
