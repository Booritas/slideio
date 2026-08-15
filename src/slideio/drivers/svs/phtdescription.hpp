// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/svs/svs_api_def.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <tinyxml2.h>

namespace slideio
{
    class SLIDEIO_SVS_EXPORTS PHTDescription
    {
    public:
        class Attribute
        {
        public:
            std::string_view Name;
            std::string_view Group;
            std::string_view Element;
		};
    public:
        explicit PHTDescription(const std::string& description);
        ~PHTDescription();
        // The class owns the parsed xml document and cannot be copied.
        PHTDescription(const PHTDescription&) = delete;
        PHTDescription& operator=(const PHTDescription&) = delete;
        PHTDescription(PHTDescription&& other) noexcept;
        PHTDescription& operator=(PHTDescription&& other) noexcept;

        // True if the text is the xml metadata of a philips tiff file. Used to tell a
        // philips file from any other tiff, which the *.tif extension cannot do.
        static bool isPhilipsDescription(const std::string& description);

        // The data objects of the given type held by the array of `arrayAttribute`, e.g.
        // the DPScannedImage objects of PIM_DP_SCANNED_IMAGES. If the parent does not
        // carry that attribute, every attribute of the parent is searched for an array
        // instead, so a file that declares its objects elsewhere is still read.
        std::vector<tinyxml2::XMLElement*> getObjectList(const tinyxml2::XMLElement* parent,
            const Attribute& arrayAttribute, std::string_view name);
        int getAttributeInt(const tinyxml2::XMLElement* element, const Attribute& attribute);
        std::string getAttributeText(const tinyxml2::XMLElement* element, const Attribute& attribute);
        // The values of an IStringArray attribute, e.g. "1.6.6186" "20150402_R48" "4.0.3".
        // getAttributeText cannot read those: it strips the outer quotes only and hands back
        // the interior ones as part of the value.
        std::vector<std::string> getAttributeTextList(const tinyxml2::XMLElement* element, const Attribute& attribute);
        std::vector<double> getAttributeDoubleList(const tinyxml2::XMLElement* element, const Attribute& attribute);
        tinyxml2::XMLElement* getRoot();
		bool hasAttribute(const tinyxml2::XMLElement* element, const Attribute& attribute);
		tinyxml2::XMLDocument* getDocument() const{
			return m_doc.get();
		}
    private:
        std::unique_ptr<tinyxml2::XMLDocument> m_doc;
    };

    // The philips metadata vocabulary. Nested so that names like MANUFACTURER, LABEL,
    // WSI and BITS_STORED do not sit at slideio scope, where they are collision prone.
    // inline constexpr rather than namespace-scope const: one object for the whole
    // program instead of a copy per translation unit.
    namespace phtiff
    {
        // Slide (root DataObject) level attributes
        inline constexpr PHTDescription::Attribute MANUFACTURER = { "DICOM_MANUFACTURER", "0x0008", "0x0070" };
        inline constexpr PHTDescription::Attribute SOFTWARE_VERSIONS = { "DICOM_SOFTWARE_VERSIONS", "0x0018", "0x1020" };
        inline constexpr PHTDescription::Attribute UFS_INTERFACE_VERSION = { "PIM_DP_UFS_INTERFACE_VERSION", "0x301D", "0x1001" };
        inline constexpr PHTDescription::Attribute UFS_BARCODE = { "PIM_DP_UFS_BARCODE", "0x301D", "0x1002" };
        inline constexpr PHTDescription::Attribute SCANNED_IMAGES = { "PIM_DP_SCANNED_IMAGES", "0x301D", "0x1003" };

        // Scanned image (DPScannedImage) attributes
        inline constexpr PHTDescription::Attribute IMAGE_TYPE = { "PIM_DP_IMAGE_TYPE", "0x301D", "0x1004" };
        inline constexpr PHTDescription::Attribute IMAGE_DATA = { "PIM_DP_IMAGE_DATA", "0x301D", "0x1005" };
        inline constexpr PHTDescription::Attribute IMAGE_ROWS = { "PIM_DP_IMAGE_ROWS", "0x301D", "0x1006" };
        inline constexpr PHTDescription::Attribute IMAGE_COLUMNS = { "PIM_DP_IMAGE_COLUMNS", "0x301D", "0x1007" };
        inline constexpr PHTDescription::Attribute SOURCE_FILE = { "PIM_DP_SOURCE_FILE", "0x301D", "0x1000" };
        inline constexpr PHTDescription::Attribute PIXEL_TRANSFORMATION_METHOD = { "UFS_IMAGE_PIXEL_TRANSFORMATION_METHOD", "0x301D", "0x2013" };
        inline constexpr PHTDescription::Attribute DERIVATION_DESCRIPTION = { "DICOM_DERIVATION_DESCRIPTION", "0x0008", "0x2111" };
        inline constexpr PHTDescription::Attribute IMAGE_RESOLUTION = { "DICOM_PIXEL_SPACING", "0x0028", "0x0030" };

        // Pixel format attributes
        inline constexpr PHTDescription::Attribute SAMPLES_PER_PIXEL = { "DICOM_SAMPLES_PER_PIXEL", "0x0028", "0x0002" };
        inline constexpr PHTDescription::Attribute PHOTOMETRIC_INTERPRETATION = { "DICOM_PHOTOMETRIC_INTERPRETATION", "0x0028", "0x0004" };
        inline constexpr PHTDescription::Attribute PLANAR_CONFIGURATION = { "DICOM_PLANAR_CONFIGURATION", "0x0028", "0x0006" };
        inline constexpr PHTDescription::Attribute BITS_ALLOCATED = { "DICOM_BITS_ALLOCATED", "0x0028", "0x0100" };
        inline constexpr PHTDescription::Attribute BITS_STORED = { "DICOM_BITS_STORED", "0x0028", "0x0101" };
        inline constexpr PHTDescription::Attribute HIGH_BIT = { "DICOM_HIGH_BIT", "0x0028", "0x0102" };
        inline constexpr PHTDescription::Attribute PIXEL_REPRESENTATION = { "DICOM_PIXEL_REPRESENTATION", "0x0028", "0x0103" };

        // Compression attributes
        inline constexpr PHTDescription::Attribute LOSSY_IMAGE_COMPRESSION = { "DICOM_LOSSY_IMAGE_COMPRESSION", "0x0028", "0x2110" };
        inline constexpr PHTDescription::Attribute LOSSY_IMAGE_COMPRESSION_RATIO = { "DICOM_LOSSY_IMAGE_COMPRESSION_RATIO", "0x0028", "0x2112" };
        inline constexpr PHTDescription::Attribute LOSSY_IMAGE_COMPRESSION_METHOD = { "DICOM_LOSSY_IMAGE_COMPRESSION_METHOD", "0x0028", "0x2114" };

        // Zoom level (PixelDataRepresentation) attributes
        inline constexpr PHTDescription::Attribute LEVEL_SEQUENCE = { "PIIM_PIXEL_DATA_REPRESENTATION_SEQUENCE", "0x1001", "0x8B01" };
        inline constexpr PHTDescription::Attribute LEVEL_NUMBER = { "PIIM_PIXEL_DATA_REPRESENTATION_NUMBER", "0x1001", "0x8B02" };
        inline constexpr PHTDescription::Attribute LEVEL_ROWS = { "PIIM_PIXEL_DATA_REPRESENTATION_ROWS", "0x2001", "0x115D" };
        inline constexpr PHTDescription::Attribute LEVEL_COLUMNS = { "PIIM_PIXEL_DATA_REPRESENTATION_COLUMNS", "0x2001", "0x115E" };
        inline constexpr PHTDescription::Attribute LEVEL_POSITION = { "PIIM_DP_PIXEL_DATA_REPRESENTATION_POSITION", "0x101D", "0x100B" };

        inline constexpr std::string_view SCANNED_IMAGE = "DPScannedImage";
        inline constexpr std::string_view WSI = "WSI";
        inline constexpr std::string_view PIXEL_DATA_REPRESENTATION = "PixelDataRepresentation";
        inline constexpr std::string_view DP_UFS_IMPORT = "DPUfsImport";
    }
}

