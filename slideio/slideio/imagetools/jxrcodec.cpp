// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/slideio.hpp"
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#if !defined(WIN32)
#define INITGUID
#endif
#include <JXRGlue.h>
#include <JXRTest.h>
#include <opencv2/imgproc.hpp>

template <class T>
class JxrObjectKeeper
{
public:
    JxrObjectKeeper(T *object=nullptr) : m_object(object)
    {
        
    }
    ~JxrObjectKeeper()
    {
        if(m_object)
        {
            m_object->Release(&m_object);
        }
    }
    operator T*&()
    {
        return m_object;
    }
    T*& object()
    {
        return m_object;
    }
    T* operator->()
    {
        return m_object;
    }
    T* m_object;
};




void slideio::ImageTools::readJxrImage(const std::string& path, cv::OutputArray output)
{
    namespace fs = boost::filesystem;
    if(!fs::exists(path))
    {
        throw std::runtime_error(
            (boost::format("File %1% does not exist") % path).str()
        );
    }
    JxrObjectKeeper<PKCodecFactory> codecFactory(nullptr);
    PKCreateCodecFactory(&codecFactory.object(), WMP_SDK_VERSION);
    JxrObjectKeeper<PKImageDecode> decoder(nullptr);
    ERR err = codecFactory->CreateDecoderFromFile(path.c_str(), &decoder.object());
    if(err!=WMP_errSuccess)
    {
        throw std::runtime_error(
            (boost::format("Cannot create decoder from file. Error code: %1%") % err).str());
    }
    U32 numFrames = 0;
    decoder->GetFrameCount(decoder, &numFrames);
    if(numFrames>1)
    {
        throw std::runtime_error(
            (boost::format("JxrDecoder: unexpected number of sub-images: %1%") % numFrames).str());
    }
    PKPixelInfo pixelFormatInfo;
    pixelFormatInfo.pGUIDPixFmt = &decoder->guidPixFormat;
    err = PixelFormatLookup(&pixelFormatInfo, LOOKUP_FORWARD);
    if(err!=WMP_errSuccess)
    {
        throw std::runtime_error("Unsupported pixel format");
    }
    const int numChannels = static_cast<int>(pixelFormatInfo.cChannel);

    JxrObjectKeeper<PKFactory> factory(nullptr);
    PKCreateFactory(&factory.object(), PK_SDK_VERSION);

    decoder->WMP.wmiI.cROILeftX = 0;
    decoder->WMP.wmiI.cROITopY = 0;
    decoder->WMP.wmiI.cROIWidth = decoder->WMP.wmiI.cWidth;
    decoder->WMP.wmiI.cROIHeight = decoder->WMP.wmiI.cHeight;


    output.create(decoder->uHeight, decoder->uWidth, CV_MAKETYPE(CV_8U, 3));
    cv::Mat raster = output.getMat();
    char* outputFileExtension = const_cast<char*>(".bmp");

    Float rX = 0.0, rY = 0.0;
    PKRect rect = {0, 0, 0, 0};
    rect.Width = (I32)(decoder->WMP.wmiI.cROIWidth);
    rect.Height = (I32)(decoder->WMP.wmiI.cROIHeight);
    decoder->GetResolution(decoder.object(), &rX, &rY);
    const int rasterSize = rect.Height*rect.Width*numChannels;
    const int size = rasterSize + 100;
    std::vector<uint8_t> buff(size);
    const auto outFormat = GUID_PKPixelFormat24bppRGB;
    struct WMPStream* encodeStream = nullptr;
    JxrObjectKeeper<PKFormatConverter> converter(nullptr);
    codecFactory->CreateFormatConverter(&converter.object());
    err = converter->Initialize(converter, decoder.object(), outputFileExtension, outFormat);
    if(err!=WMP_errSuccess)
    {
        throw std::runtime_error(
            (boost::format("Error by initialization of format converter: %1%") % err).str());
    }

    factory->CreateStreamFromMemory(&encodeStream, buff.data(), size);
    const PKIID* encoderIID(nullptr);
    GetTestEncodeIID(outputFileExtension, &encoderIID);

    JxrObjectKeeper<PKImageEncode> encoder(nullptr);
    PKTestFactory_CreateCodec(encoderIID,  (void**)&encoder.object());
    encoder->Initialize(encoder, encodeStream, nullptr, 0);
    encoder->SetPixelFormat(encoder, outFormat);
    encoder->SetResolution(encoder, rX, rY);
    encoder->WMP.wmiSCP.bBlackWhite = decoder->WMP.wmiSCP.bBlackWhite;
    encoder->SetSize(encoder, rect.Width, rect.Height);
    encoder->WriteSource = PKImageEncode_Transcode;
    encoder->WriteSource(encoder, converter, &rect);

    const size_t offset = encoder->offPixel;
    std::copy(buff.data() + offset, buff.data()+offset+rasterSize, raster.data);

    cv::flip(raster, raster, 0);
    cv::cvtColor(raster, raster, cv::COLOR_BGR2RGB);
}

void PKCodecFactory_CreateDecoderFromMemory(const uint8_t* buff, size_t size, PKFactory* factory, PKImageDecode** ppDecoder)
{
    char szFilename[] = "test.jxr";
    ERR err = WMP_errSuccess;

    char *pExt = NULL;
    const PKIID* pIID = NULL;

    struct WMPStream* pStream = NULL;
    PKImageDecode* pDecoder = NULL;

    // get file extension
    pExt = strrchr(szFilename, '.');

    // get decode PKIID
    err = GetImageDecodeIID(pExt, &pIID);
    if(err!=WMP_errSuccess)
        throw std::runtime_error("JxrCodec: Cannot find pixel format.");

    // create stream
    err = factory->CreateStreamFromMemory(&pStream, const_cast<uint8_t*>(buff), size);
    if(err!=WMP_errSuccess)
        throw std::runtime_error("JxrCodec: Cannot create memory stream.");

    // Create decoder
    err = PKCodecFactory_CreateCodec(pIID, reinterpret_cast<void**>(ppDecoder));
    if(err!=WMP_errSuccess)
        throw std::runtime_error("JxrCodec: Cannot create codec for the pixel format.");
    pDecoder = *ppDecoder;

    // attach stream to decoder
    err = pDecoder->Initialize(pDecoder, pStream);
    if(err!=WMP_errSuccess)
        throw std::runtime_error("JxrCodec: Cannot create codec for the pixel format.");

    pDecoder->fStreamOwner = !0;
    if(err!=WMP_errSuccess)
        throw std::runtime_error("JxrCodec: Error codec initialization.");
}

void slideio::ImageTools::decodeJxrBlock(const uint8_t* data, size_t dataBlockSize, cv::OutputArray output)
{
    if(data==nullptr || dataBlockSize==0)
    {
        throw std::runtime_error("JxrCodec: invalid input block");
    }
    JxrObjectKeeper<PKCodecFactory> codecFactory(nullptr);
    PKCreateCodecFactory(&codecFactory.object(), WMP_SDK_VERSION);
    JxrObjectKeeper<PKFactory> factory(nullptr);
    PKCreateFactory(&factory.object(), PK_SDK_VERSION);
    ERR err = WMP_errSuccess;

    JxrObjectKeeper<PKImageDecode> decoder(nullptr);
    PKCodecFactory_CreateDecoderFromMemory(data, dataBlockSize, factory, &decoder.object());

    U32 numFrames = 0;
    decoder->GetFrameCount(decoder, &numFrames);
    if(numFrames>1)
    {
        throw std::runtime_error(
            (boost::format("JxrDecoder: unexpected number of sub-images: %1%") % numFrames).str());
    }
    PKPixelInfo pixelFormatInfo;
    pixelFormatInfo.pGUIDPixFmt = &decoder->guidPixFormat;
    err = PixelFormatLookup(&pixelFormatInfo, LOOKUP_FORWARD);
    if(err!=WMP_errSuccess)
    {
        throw std::runtime_error("Unsupported pixel format");
    }
    const int numChannels = static_cast<int>(pixelFormatInfo.cChannel);

    decoder->WMP.wmiI.cROILeftX = 0;
    decoder->WMP.wmiI.cROITopY = 0;
    decoder->WMP.wmiI.cROIWidth = decoder->WMP.wmiI.cWidth;
    decoder->WMP.wmiI.cROIHeight = decoder->WMP.wmiI.cHeight;


    output.create(decoder->uHeight, decoder->uWidth, CV_MAKETYPE(CV_8U, 3));
    cv::Mat raster = output.getMat();
    char* outputFileExtension = const_cast<char*>(".bmp");

    Float rX = 0.0, rY = 0.0;
    PKRect rect = {0, 0, 0, 0};
    rect.Width = (I32)(decoder->WMP.wmiI.cROIWidth);
    rect.Height = (I32)(decoder->WMP.wmiI.cROIHeight);
    decoder->GetResolution(decoder.object(), &rX, &rY);
    const int rasterSize = rect.Height*rect.Width*numChannels;
    const int size = rasterSize + 100;
    std::vector<uint8_t> buff(size);
    const auto outFormat = GUID_PKPixelFormat24bppRGB;
    struct WMPStream* encodeStream = nullptr;
    JxrObjectKeeper<PKFormatConverter> converter(nullptr);
    codecFactory->CreateFormatConverter(&converter.object());
    err = converter->Initialize(converter, decoder.object(), outputFileExtension, outFormat);
    if(err!=WMP_errSuccess)
    {
        throw std::runtime_error(
            (boost::format("Error by initialization of format converter: %1%") % err).str());
    }

    factory->CreateStreamFromMemory(&encodeStream, buff.data(), size);
    const PKIID* encoderIID(nullptr);
    GetTestEncodeIID(outputFileExtension, &encoderIID);

    JxrObjectKeeper<PKImageEncode> encoder(nullptr);
    PKTestFactory_CreateCodec(encoderIID,  (void**)&encoder.object());
    encoder->Initialize(encoder, encodeStream, nullptr, 0);
    encoder->SetPixelFormat(encoder, outFormat);
    encoder->SetResolution(encoder, rX, rY);
    encoder->WMP.wmiSCP.bBlackWhite = decoder->WMP.wmiSCP.bBlackWhite;
    encoder->SetSize(encoder, rect.Width, rect.Height);
    encoder->WriteSource = PKImageEncode_Transcode;
    encoder->WriteSource(encoder, converter, &rect);

    const size_t offset = encoder->offPixel;
    std::copy(buff.data() + offset, buff.data()+offset+rasterSize, raster.data);

    cv::flip(raster, raster, 0);
    cv::cvtColor(raster, raster, cv::COLOR_BGR2RGB);
}
