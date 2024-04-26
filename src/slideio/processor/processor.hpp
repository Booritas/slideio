// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio/processor/slideio_processor_def.hpp"
#include "slideio/persistence/storage.hpp"
#include "slideio/core/cvscene.hpp"

namespace slideio
{
    class Project;

    class SLIDEIO_PROCESSOR_EXPORTS Processor
    {
    public:
        Processor(){}
        virtual ~Processor() {}
    };
};
