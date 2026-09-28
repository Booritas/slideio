# Public headers of slideio-core: the ones shipped in the -dev package.
#
# This list is the definition of the module's public surface. It drives both
# install() and the compile check in src/public_headers_check, so a header added
# here is immediately compiled the way a consumer would compile it -- alone, and
# without SLIDEIO_INTERNAL_HEADER.
#
# Not listed, deliberately:
#   rect.inl, size.inl, range.inl  the cv::Rect/cv::Size/cv::Range conversions,
#                                  included by their .hpp only under
#                                  SLIDEIO_INTERNAL_HEADER. They include
#                                  <opencv2/core/types.hpp>, and shipping them
#                                  would make OpenCV a public dependency.
#   metadata_internal.hpp          implementation of metadata.hpp.
#   cvscene.hpp, cvslide.hpp,      the OpenCV-based interface; cvscene.hpp
#   imagedriver.hpp                includes <opencv2/core.hpp>, same reason.
#   log.hpp, logcontract.hpp,      no exported entry point a consumer can reach;
#   dimensions.hpp, refcounter.hpp setLogLevel() in slideio.hpp is the public
#                                  way to the logger.
#
# resolution.hpp is listed but is reached by no other public header -- Scene
# returns resolutions as std::tuple<double,double>. It stays public because it
# has shipped in every release; removing it would break out-of-tree callers for
# no gain. Do not "clean it up" without a BREAKING_CHANGES.md entry.
set(SLIDEIO_PUBLIC_HEADERS_core
    slideio_core_def.hpp
    slideio_enums.hpp
    slideio_structs.hpp
    colorprofile.hpp
    exceptions.hpp
    levelinfo.hpp
    metadata.hpp
    range.hpp
    rect.hpp
    resolution.hpp
    size.hpp
)
