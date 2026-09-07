// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif
#include <memory>
#include <string>

#include "ndpitifftools.hpp"
#include "ndpitiffkeeper.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/readcontext.hpp"

namespace libtiff
{
    struct tiff;
    typedef tiff TIFF;
}

namespace slideio
{
    // The NDPI counterpart of e.g. SCNReadContext: one owning handle. NDPITIFFKeeper's
    // path-taking constructor opens the file and this throws if that fails, so no
    // null-handle guard is needed at any acquireContext() call site.
    class NDPIReadContext : public ReadContext
    {
    public:
        explicit NDPIReadContext(const std::string& filePath) : keeper(filePath) {
            if (!keeper.isValid()) {
                RAISE_RUNTIME_ERROR << "NDPIImageDriver: cannot open file " << filePath;
            }
        }
        NDPITIFFKeeper keeper;
    };

    class SLIDEIO_NDPI_EXPORTS NDPIFile
    {
    public:
        NDPIFile(){
        }
        ~NDPIFile();
        void init(const std::string& filePath);
        const std::vector<NDPITiffDirectory>& directories() const {
            return m_directories;
        }
        const std::string getFilePath() const  {
            return m_filePath;
        }
        /// Borrows a handle for one block read. The pool lives here rather than
        /// on the scene because the handle does: NDPIScene::m_pfile is a raw
        /// pointer to a shared NDPIFile, so a per-scene pool would multiply
        /// descriptors by scene count.
        ContextPool::Borrow acquireContext() { return pool().acquire(); }
        /// Contexts the pool has constructed so far. For tests -- it is how
        /// scenesOfOneFileShareTheHandlePool (test_ndpi_driver.cpp) observes that
        /// every scene of this file draws from the one pool here, rather than each
        /// growing a pool of its own. Mirrors ContextPool::contextCount(), which
        /// exists for the same reason.
        int contextCount() const { return pool().contextCount(); }
        const NDPITiffDirectory& findZoomDirectory(double zoom, int sceneWidth, int dirBegin, int dirEnd);
    private:
        void scanFile();
        // The pool exists only after init() has supplied a path. No live path reaches
        // either accessor before that -- scenes are constructed from an NDPIFile whose
        // init() has already returned -- but the shape invites one, so the null is
        // reported as an error rather than dereferenced.
        ContextPool& pool() const {
            if (!m_contextPool) {
                RAISE_RUNTIME_ERROR << "NDPIFile: the file is not initialized";
            }
            return *m_contextPool;
        }
    private:
        std::string m_filePath;
        std::vector<NDPITiffDirectory> m_directories;
        // A plain ContextPool member can't be initialized until filePath is known, and
        // NDPIFile is default-constructed well before that (init() supplies the path
        // later) -- so the pool is built in init(), not in the constructor init list.
        //
        // Declared last on purpose, and it must stay after m_directories.
        // ~ContextPool blocks until every outstanding borrow is returned, and
        // members are destroyed in reverse declaration order, so this ordering
        // is what keeps m_directories alive for an in-flight NDPIScene::readTile
        // -- which reads m_pfile->directories() -- until that read has finished.
        std::unique_ptr<ContextPool> m_contextPool;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif
