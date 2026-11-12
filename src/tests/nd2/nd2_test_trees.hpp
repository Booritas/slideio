// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include <string>
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include "slideio/drivers/nd2/nd2variant.hpp"
#include "slideio/core/tools/filereader.hpp"

namespace nd2test
{
    /// The three metadata trees of a fixture, decoded: Ver3 (LV names) or Ver2 XML.
    struct Trees
    {
        slideio::nd2::Variant attributes;
        slideio::nd2::Variant experiment;
        slideio::nd2::Variant picture;
        bool hasPicture = false;
    };

    inline slideio::nd2::Variant loadTree(const slideio::FileReader& file,
        const slideio::nd2::ChunkFile& chunks, const std::string& name, bool* found = nullptr)
    {
        const slideio::nd2::ChunkHeader* h = chunks.find(name);
        if (found) *found = h != nullptr;
        if (!h) return slideio::nd2::Variant();
        return slideio::nd2::decodeMetadata(chunks.readData(file, *h));
    }

    inline Trees loadTrees(const std::string& fixtureName)
    {
        const std::string path = std::string(ND2_TEST_DATA_DIR) + "/" + fixtureName;
        slideio::FileReader file(path);
        const slideio::nd2::ChunkFile chunks = slideio::nd2::ChunkFile::read(file);
        const bool v3 = chunks.versionMajor() >= 3;
        Trees t;
        t.attributes = loadTree(file, chunks, v3 ? "ImageAttributesLV!" : "ImageAttributes!");
        t.experiment = loadTree(file, chunks, v3 ? "ImageMetadataLV!" : "ImageMetadata!");
        t.picture = loadTree(file, chunks, v3 ? "ImageMetadataSeqLV|0!" : "ImageMetadataSeq|0!", &t.hasPicture);
        return t;
    }
}
