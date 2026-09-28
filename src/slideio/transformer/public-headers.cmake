# Public headers of slideio-transformer. See
# src/slideio/core/public-headers.cmake for what this list is and what it drives.
#
# The *wrap.hpp headers are the public face of the filters: each wraps a
# Transformation implementation whose own header (gaussianblurfilter.hpp and
# friends) stays internal.
set(SLIDEIO_PUBLIC_HEADERS_transformer
    transformer_def.hpp
    transformer.hpp
    transformation.hpp
    transformations.hpp
    transformationtype.hpp
    transformationwrapper.hpp
    wrappers.hpp
    colorspace.hpp
    bilateralfilterwrap.hpp
    cannyfilterwrap.hpp
    colormanagementwrap.hpp
    colortransformationwrap.hpp
    gaussianblurfilterwrap.hpp
    laplacianfilterwrap.hpp
    medianblurfilterwrap.hpp
    scharrfilterwrap.hpp
    sobelfilterwrap.hpp
)
