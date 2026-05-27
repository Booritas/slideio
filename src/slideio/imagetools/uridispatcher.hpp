// src/slideio/imagetools/uridispatcher.hpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/randomaccessstream.hpp"

#include <memory>
#include <string>

namespace slideio
{
    enum class UriScheme { LocalFile, S3, Http };

    SLIDEIO_IMAGETOOLS_EXPORTS UriScheme detectUriScheme(const std::string& uri);

    // Translates "s3://bucket/key" -> "https://bucket.s3.amazonaws.com/key".
    // Returns the input unchanged for non-s3 URIs.
    SLIDEIO_IMAGETOOLS_EXPORTS std::string s3UriToHttps(const std::string& uri);

    // Factory: returns the appropriate RandomAccessStream for the given URI.
    SLIDEIO_IMAGETOOLS_EXPORTS std::shared_ptr<RandomAccessStream> createStream(
        const std::string& uri);

    // Returns the resource name (e.g. "slide.svs") suitable for pattern matching
    // — strips s3://, https://..., query strings, and file:// prefixes.
    SLIDEIO_IMAGETOOLS_EXPORTS std::string uriResourceName(const std::string& uri);

    // Given an originating URI and a relative or absolute name, produces a URI
    // that lives "next to" the original. Examples:
    //   siblingUri("http://h/dir/a.svs", "b.svs")        -> "http://h/dir/b.svs"
    //   siblingUri("s3://bucket/dir/a.afi", "b.svs")     -> "s3://bucket/dir/b.svs"
    //   siblingUri("/abs/dir/a.afi", "b.svs")            -> "/abs/dir/b.svs"
    // If `name` already contains a scheme, it is returned unchanged.
    SLIDEIO_IMAGETOOLS_EXPORTS std::string siblingUri(const std::string& base,
                                                      const std::string& name);
}
