# Public headers of slideio-imagetools. See
# src/slideio/core/public-headers.cmake for what this list is and what it drives.
#
# One header only, and that is the whole point of the module's surface:
# EncodeParameters is an argument of the converter's public API, so it has to
# ship. Everything else here -- the tiff, jpeg2000, FreeImage and colour-
# management wrappers -- is implementation and pulls third-party headers with it.
set(SLIDEIO_PUBLIC_HEADERS_imagetools
    encodeparameters.hpp
)
