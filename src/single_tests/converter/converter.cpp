#include "slideio/converter/converter.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/base/exceptions.hpp"

#include "slideio/converter/converterparameters.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"


int main()
{
    // constexpr int tileWidth = 512;
    // constexpr int tileHeight = 128;
    // constexpr int numZoomLevels = 5;

    // std::string path = TestTools::getTestImagePath("gdal", "Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg");
    // SlidePtr slide = openSlide(path, "GDAL");
    // ScenePtr scene = slide->getScene(0);
    // auto sceneRect = scene->getRect();
    // const int sceneWidth = std::get<2>(sceneRect);
    // const int sceneHeight = std::get<3>(sceneRect);
    // const int numChannels = scene->getNumChannels();
    // const DataType dt = scene->getChannelDataType(0);

    // slideio::TempFile tmp("ome.tiff");
    // std::string outputPath = tmp.getPath().string();
    // if (std::filesystem::exists(outputPath)) {
    //     std::filesystem::remove(outputPath);
    // }
    // OMETIFFJpegConverterParameters parameters;
    // auto tiffParams =
    //     std::static_pointer_cast<TIFFContainerParameters>(parameters.getContainerParameters());
    // parameters.setQuality(99);
    // tiffParams->setNumZoomLevels(numZoomLevels);
    // tiffParams->setTileWidth(tileWidth);
    // tiffParams->setTileHeight(tileHeight);

    // TiffConverter converter;
    // ASSERT_NO_THROW(converter.createFileLayout(scene->getCVScene(), parameters));

	return 0;   
}
