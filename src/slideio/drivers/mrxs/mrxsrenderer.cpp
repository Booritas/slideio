// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsrenderer.hpp"
#include "slideio/drivers/mrxs/mrxserrors.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

using namespace slideio::mrxs;

namespace
{
    // An opaque RGB image as premultiplied float RGBA, with one transparent pixel of
    // border on every side so bilinear sampling reads zeros outside the image.
    cv::Mat toPaddedRgba(const cv::Mat& rgb)
    {
        cv::Mat rgba(rgb.rows, rgb.cols, CV_32FC4);
        for (int y = 0; y < rgb.rows; ++y) {
            const cv::Vec3b* s = rgb.ptr<cv::Vec3b>(y);
            cv::Vec4f* d = rgba.ptr<cv::Vec4f>(y);
            for (int x = 0; x < rgb.cols; ++x) {
                d[x] = cv::Vec4f(s[x][0] / 255.f, s[x][1] / 255.f, s[x][2] / 255.f, 1.f);
            }
        }
        cv::Mat padded;
        cv::copyMakeBorder(rgba, padded, 1, 1, 1, 1, cv::BORDER_CONSTANT, cv::Scalar::all(0));
        return padded;
    }

    // Translates a padded premultiplied image by (tx, ty) with bilinear sampling into the
    // window of a destination of size dstSize, and hands every touched destination pixel
    // to `sink`. Destination pixel x samples the image at x - tx, where image pixel
    // centres sit at integer coordinates; outside the image it reads transparency.
    template <typename Sink>
    void translate(const cv::Mat& padded, double tx, double ty, cv::Size dstSize, Sink&& sink)
    {
        const int w = padded.cols - 2, h = padded.rows - 2;
        const double kx = std::floor(-tx), ky = std::floor(-ty);
        const float ax = static_cast<float>(-tx - kx), ay = static_cast<float>(-ty - ky);
        const int ix = static_cast<int>(kx), iy = static_cast<int>(ky);
        // Destination pixels whose sample touches the image or its transparent border.
        const int x0 = std::max(0, -1 - ix), x1 = std::min(dstSize.width - 1, w - 1 - ix);
        const int y0 = std::max(0, -1 - iy), y1 = std::min(dstSize.height - 1, h - 1 - iy);
        for (int y = y0; y <= y1; ++y) {
            const cv::Vec4f* r0 = padded.ptr<cv::Vec4f>(y + iy + 1);
            const cv::Vec4f* r1 = padded.ptr<cv::Vec4f>(y + iy + 2);
            for (int x = x0; x <= x1; ++x) {
                const int c = x + ix + 1;
                const cv::Vec4f top = r0[c] * (1.f - ax) + r0[c + 1] * ax;
                const cv::Vec4f bottom = r1[c] * (1.f - ax) + r1[c + 1] * ax;
                sink(x, y, top * (1.f - ay) + bottom * ay);
            }
        }
    }

    // Cairo's SATURATE operator on premultiplied colour (format description 11.3).
    inline void saturate(cv::Vec4f& d, const cv::Vec4f& s)
    {
        const float as = s[3], ad = d[3];
        if (as <= 0.f || ad >= 1.f) {
            return;
        }
        const float fs = std::min(1.f, (1.f - ad) / as);
        d[0] += s[0] * fs;
        d[1] += s[1] * fs;
        d[2] += s[2] * fs;
        d[3] = std::min(as, 1.f - ad) + ad;
    }
}

void Renderer::render(int level, const cv::Rect& rect, cv::Mat& out, int64_t bandPixels) const
{
    const SlideStructure& slide = m_dataset.structure;
    const LevelGeometry& lg = m_dataset.geometry.levels.at(level);
    const LevelMetadata& meta = slide.metadata.levels.at(level);
    const std::vector<ImageRecord>& records = slide.levels.at(level);
    out.create(rect.size(), CV_8UC3);
    if (rect.empty()) {
        return;
    }
    const cv::Vec3f background(m_dataset.background[0] / 255.f, m_dataset.background[1] / 255.f,
                               m_dataset.background[2] / 255.f);
    const int tileW = static_cast<int>(std::ceil(lg.tileWidth));
    const int tileH = static_cast<int>(std::ceil(lg.tileHeight));
    const bool cut = meta.imageWidth > lg.tileWidth || meta.imageHeight > lg.tileHeight;
    const int bandRows = static_cast<int>(std::clamp<int64_t>(bandPixels / rect.width, 1, rect.height));

    for (int bandY = 0; bandY < rect.height; bandY += bandRows) {
        const int rows = std::min(bandRows, rect.height - bandY);
        cv::Mat acc(rows, rect.width, CV_32FC4, cv::Scalar::all(0));
        const double originX = rect.x, originY = rect.y + bandY;
        // Request-local: a decoded image serves every subtile cut from it in this band.
        std::unordered_map<uint32_t, cv::Mat> decoded;
        for (uint32_t i : lg.query(originX, originY, originX + rect.width, originY + rows)) {
            const Drawable& d = lg.drawables()[i];
            cv::Mat& image = decoded[d.record];
            if (image.empty()) {
                const ImageRecord& record = records[d.record];
                const std::string what = "zoom level " + std::to_string(level) + " image (" + std::to_string(record.x)
                    + ", " + std::to_string(record.y) + ")";
                image = toPaddedRgba(decodeImage(readPayload(slide, record.payload), meta.format,
                                                 meta.imageWidth, meta.imageHeight, what));
            }
            cv::Mat tile = image;
            if (cut) {
                // The subtile alone, through a transparent ceil-sized intermediate, so the
                // rest of the image cannot reach the destination (format description 11.2).
                cv::Mat intermediate(tileH, tileW, CV_32FC4, cv::Scalar::all(0));
                translate(image, -d.xi * lg.tileWidth, -d.yi * lg.tileHeight, intermediate.size(),
                          [&](int x, int y, const cv::Vec4f& v) { intermediate.at<cv::Vec4f>(y, x) = v; });
                cv::copyMakeBorder(intermediate, tile, 1, 1, 1, 1, cv::BORDER_CONSTANT, cv::Scalar::all(0));
            }
            translate(tile, d.x - originX, d.y - originY, acc.size(),
                      [&](int x, int y, const cv::Vec4f& v) { saturate(acc.at<cv::Vec4f>(y, x), v); });
        }
        // The background goes underneath only now: an opaque accumulator would have made
        // every saturating paint a no-op.
        for (int y = 0; y < rows; ++y) {
            const cv::Vec4f* a = acc.ptr<cv::Vec4f>(y);
            cv::Vec3b* o = out.ptr<cv::Vec3b>(bandY + y);
            for (int x = 0; x < rect.width; ++x) {
                const float clear = 1.f - a[x][3];
                for (int c = 0; c < 3; ++c) {
                    o[x][c] = cv::saturate_cast<uchar>((a[x][c] + clear * background[c]) * 255.f);
                }
            }
        }
    }
}
