// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/cvscene.hpp"
#include "transformer_def.hpp"

namespace slideio
{
    class Transformation;

    class SLIDEIO_TRANSFORMER_EXPORTS TransformerScene : public CVScene
    {
    public:
        TransformerScene(std::shared_ptr<CVScene> originScene);
    public:
        std::string getFilePath() const override;
        std::string getName() const override;
        cv::Rect getRect() const override;
        int getNumChannels() const override;
        DataType getChannelDataType(int channel) const override;
        Resolution getResolution() const override;
        double getMagnification() const override;
        Compression getCompression() const override;
        int getNumZSlices() const override;
        int getNumTFrames() const override;
        std::string getChannelName(int channel) const override;
        double getZSliceResolution() const override;
        double getTFrameResolution() const override;
        std::string getRawMetadata() const override;
        std::shared_ptr<CVScene> getOriginScene() const {
            return m_originScene;
        }
    private:
        std::shared_ptr<CVScene> m_originScene;
    };
}