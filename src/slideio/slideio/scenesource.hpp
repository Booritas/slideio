// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/slideio/slideio_def.hpp"
#include "slideio/slideio/scenemetadata.hpp"
#include "slideio/core/slideio_enums.hpp"
#include "slideio/core/rect.hpp"
#include "slideio/core/size.hpp"
#include <cstddef>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace slideio
{
    class Scene;

    /**@brief Supplies the pixels of a scene that is computed rather than decoded.
     *
     * Implement this, describe the result in a SceneMetadata, and #makeScene
     * returns a genuine slideio::Scene that reads through it. The scene is a
     * real Scene -- it passes to convertScene, to a viewer and to the Python
     * bindings unchanged -- it is simply not backed by a file.
     */
    class ISceneSource
    {
    public:
        virtual ~ISceneSource() = default;

        /**@brief Reads a rectangle in SCENE coordinates, resampled to blockSize.
         *
         * The one method every source must implement.
         *
         * @param sceneRect : rectangle in the scene's own coordinate system.
         * Not clamped: SlideIO forwards it exactly as the caller gave it, so
         * the part lying outside the scene must be filled with the background
         * value, not omitted.
         * @param blockSize : size of the result. Equal to sceneRect.size()
         * means no resampling.
         * @param channelIndices : channels to return, in the given order.
         * Never empty: makeScene's adapter expands "every channel" before
         * calling, so an implementation always receives explicit indices.
         * Duplicated indices are permitted: a channel listed more than once is
         * delivered once per occurrence, in request order (so {0, 0} yields
         * two copies of channel 0).
         * @param zSlice : z slice index, 0 for a plane scene.
         * @param tFrame : time frame index, 0 for a scene without time frames.
         * @param buffer : destination. Row-major, channel-interleaved in the
         * order of @p channelIndices, rows contiguous with no padding, so its
         * length is blockSize.width * blockSize.height * channel count *
         * element size. Every requested channel shares one data type -- a
         * cv::Mat carries a single depth for all of its channels, so a
         * mixed-type request cannot be expressed and the adapter rejects one
         * before calling here. A scene whose channels differ in type is read
         * one channel per call.
         * @param bufferSize : its size in bytes.
         *
         * Report failure by throwing. Never by filling part of the buffer.
         */
        virtual void readBlock(const Rect& sceneRect,
                               const Size& blockSize,
                               const std::vector<int>& channelIndices,
                               int zSlice, int tFrame,
                               void* buffer, size_t bufferSize) = 0;

        /**@brief Reads from an explicitly named pyramid level, in LEVEL coordinates.
         *
         * Return false -- the default -- to let SlideIO derive the read from
         * #readBlock by scaling through LevelInfo::getScale(), which is what
         * CVScene does for a single level driver. Override and return true
         * when the source has a real pyramid and reading the named level
         * directly avoids rounding twice.
         *
         * Parameters are those of #readBlock, with levelRect in the coordinate
         * system of @p level. The channel contract is the same, duplicated
         * indices included.
         *
         * The two paths differ in who clamps. Declining hands the read to
         * CVScene::readClampedBlock, which clips levelRect to the level
         * bounds, background-fills the remainder and calls #readBlock with
         * the surviving part in scene coordinates. Overriding receives
         * levelRect unclamped and owns its own bounds handling.
         */
        virtual bool readLevelBlock(int /*level*/,
                                    const Rect& /*levelRect*/,
                                    const Size& /*blockSize*/,
                                    const std::vector<int>& /*channelIndices*/,
                                    int /*zSlice*/, int /*tFrame*/,
                                    void* /*buffer*/, size_t /*bufferSize*/) {
            return false;
        }

        /**@brief Whether reads of one source may overlap.
         *
         * False -- the default -- makes SlideIO serialise them, matching
         * CVScene. Return true only if #readBlock is safe to call from several
         * threads at once.
         */
        virtual bool supportsConcurrentReads() const { return false; }

        /**@brief Scene metadata as the source's own format. Empty by default.
         *
         * Read once, when #makeScene builds the scene: Scene::getRawMetadata
         * and Scene::getMetadata both report that snapshot.*/
        virtual std::string getRawMetadata() const { return std::string(); }

        /**@brief The format of #getRawMetadata, which decides how
         * Scene::getMetadata parses it.
         *
         * MetadataFormat::Unknown by default, meaning "not declared": the
         * scene then reports MetadataFormat::Text when there is raw metadata
         * -- kept as a single "text" entry -- and MetadataFormat::None when
         * there is not, deciding from the one #getRawMetadata call #makeScene
         * makes. Override to report JSON or XML and get a structured tree.*/
        virtual MetadataFormat getMetadataFormat() const {
            return MetadataFormat::Unknown;
        }

        /**@brief Names of auxiliary images. Empty by default.
         *
         * Every name reported here must be served by #getAuxImage.*/
        virtual std::list<std::string> getAuxImageNames() const {
            return std::list<std::string>();
        }

        /**@brief An auxiliary image by name. nullptr by default.
         *
         * Return a Scene -- usually one built by #makeScene from a source of
         * its own -- for every name #getAuxImageNames reports. nullptr makes
         * Scene::getAuxImage throw RuntimeError for that name.*/
        virtual std::shared_ptr<Scene> getAuxImage(const std::string& /*name*/) const {
            return nullptr;
        }
    };

    /**@brief Wraps an ISceneSource in a genuine slideio::Scene.
     *
     * Performs no image work: no file is opened, nothing is cropped, no
     * channel is selected. Every field of @p metadata is reported verbatim by
     * the corresponding Scene getter, and pixels are fetched from @p source
     * on demand.
     *
     * @param source : supplies the pixels. Must not be null.
     * @param metadata : what the resulting scene reports about itself.
     * @return a Scene usable anywhere a file backed one is, convertScene and
     * the Python bindings included.
     *
     * Throws RuntimeError if the metadata is not internally consistent --
     * deliberately at construction, so a malformed scene fails here rather
     * than at its first tile read.
     */
    std::shared_ptr<Scene> SLIDEIO_EXPORTS makeScene(std::shared_ptr<ISceneSource> source,
                                                     const SceneMetadata& metadata);
}
