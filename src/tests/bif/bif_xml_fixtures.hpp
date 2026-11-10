// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

// Trimmed copies of the XML packets of real Ventana BIF files (OS-2.bif and Ventana-1.bif).
// Attribute quoting and whitespace are kept as in the files.

#include <string>

namespace slideio::bif::fixtures
{
    // IFD 0 of OS-2.bif, verbatim.
    inline const std::string os2Acquisition =
        R"XML(<iScan Magnification="40" ScanRes="0.232500" UnitNumber="BI10N0306" UserName="admin" BuildVersion="3.3.1.1" BuildDate="December, 13 2011" SlideAnnotation="" ShowLabel="1" LabelBoundary="0" Z-layers="1" Z-spacing="1" FocusMode="0" FocusQuality="1" ScanMode="1">
	<AOI0 Left="450" Top="1381" Right="633" Bottom="1465"/>
	<AOI1 Left="279" Top="1323" Right="382" Bottom="1415"/>
	<AOI2 Left="234" Top="421" Right="919" Bottom="1206"/>
</iScan>)XML";

    // IFD 0 of Ventana-1.bif, verbatim.
    inline const std::string ventanaAcquisition =
        R"XML(<?xml version='1.0' encoding='utf-8' ?>
<Metadata>
  <iScan Mode="brightfield" Magnification="40" ScanRes="0.25"
   UnitNumber="2000515" ScannerModel="VENTANA DP 200" Z-layers="1"
   Z-spacing="1" UserName="Operator" BuildVersion="1.1.0.15854"
   BuildDate="11/27/2019 11:6:28 AM" SlideAnnotation="" ShowLabel="1"
   LabelBoundary="0" Barcode1D="" Barcode2D="" FocusMode="0" FocusQuality="1"
   ScanMode="1" ScanWhitePoint="235" Anonymization="0">
    <AOI0 Left="297" Top="2323" Right="574" Bottom="2069"/>
  </iScan>
  <ProcessingParameters>
    <Registration Method="None" UseLinearEncoder="1" Radius="0"
     OverlapMinMicrons="" OverlapMaxMicrons="" ShiftMaxMicrons="" OverlapMin=""
     OverlapMax="" ShiftMax=""/>
    <Color TwistRGB="1,0,0,0,1,0,0,0,1" Applied="0"/>
  </ProcessingParameters>
</Metadata>)XML";

    // Level-0 EncodeInfo of OS-2.bif: the real ImageInfo start tags, a few joints each,
    // the real AoiOrigin block and the NUL padding the packet carries.
    inline std::string os2Stitch()
    {
        std::string xml = R"XML(<?xml version="1.0"?>
<EncodeInfo Ver='2'>
<SlideStitchInfo Left="-1" Top="-1" Right="-1" Bottom="-1">
<ImageInfo AOIScanned="1" Width="1024" Height="1360" NumRows="17" NumCols="11" Pos-X="0" Pos-Y="49268" MaxJTPFileSize="0">
<TileJointInfo FlagJoined='0' Confidence='0' Direction='RIGHT' Tile1='1' Tile2='2' OverlapX='0' OverlapY='0'/>
<TileJointInfo FlagJoined='1' Confidence='98' Direction='RIGHT' Tile1='3' Tile2='4' OverlapX='115' OverlapY='2'/>
<TileJointInfo FlagJoined='1' Confidence='99' Direction='UP' Tile1='3' Tile2='20' OverlapX='-2' OverlapY='93'/>
</ImageInfo>
<ImageInfo AOIScanned="0" Width="1024" Height="1360" NumRows="10" NumCols="12" Pos-X="5473" Pos-Y="30559" MaxJTPFileSize="0">
<TileJointInfo FlagJoined='0' Confidence='0' Direction='RIGHT' Tile1='0' Tile2='0' OverlapX='0' OverlapY='0'/>
</ImageInfo>
<ImageInfo AOIScanned="1" Width="1024" Height="1360" NumRows="61" NumCols="95" Pos-X="28274" Pos-Y="25569" MaxJTPFileSize="0">
<TileJointInfo FlagJoined='1' Confidence='98' Direction='UP' Tile1='200' Tile2='371' OverlapX='-2' OverlapY='95'/>
</ImageInfo>
</SlideStitchInfo>
<AoiOrigin>
<AOI0 OriginX="0" OriginY="38080"/>
<AOI1 OriginX="6144" OriginY="68000"/>
<AOI2 OriginX="30720" OriginY="0"/>
</AoiOrigin>
</EncodeInfo>
)XML";
        xml.append(std::string(64, '\0'));
        return xml;
    }

    // Level-0 EncodeInfo of Ventana-1.bif, trimmed to four joints. Tests mutate it by string
    // replacement on the pieces below or by appending an extra joint.
    inline const std::string ventanaStitchHead =
        R"XML(<?xml version='1.0' encoding='utf-8' ?>
<EncodeInfo Ver="2">
  <SlideStitchInfo Left="-1" Top="-1" Right="-1" Bottom="-1">
    <ImageInfo AOIScanned="1" AOIIndex="0" NumRows="21" NumCols="23"
     Width="1024" Height="1024" Pos-X="25165" Pos-Y="175179">
)XML";

    inline const std::string ventanaStitchJoints =
        R"XML(      <TileJointInfo FlagJoined="1" Confidence="100" Direction="LEFT" Tile1="1"
       Tile2="2" OverlapX="0" OverlapY="0"/>
      <TileJointInfo FlagJoined="1" Confidence="100" Direction="LEFT"
       Tile1="46" Tile2="45" OverlapX="0" OverlapY="0"/>
      <TileJointInfo FlagJoined="1" Confidence="100" Direction="LEFT" Tile1="4"
       Tile2="5" OverlapX="24" OverlapY="0"/>
      <TileJointInfo FlagJoined="1" Confidence="100" Direction="UP" Tile1="1"
       Tile2="46" OverlapX="0" OverlapY="0"/>
)XML";

    inline const std::string ventanaStitchTail =
        R"XML(    </ImageInfo>
  </SlideStitchInfo>
  <AoiOrigin>
    <AOI0 OriginX="0" OriginY="0"/>
  </AoiOrigin>
</EncodeInfo>
)XML";

    inline std::string ventanaStitch(const std::string& extraJoint = "")
    {
        return ventanaStitchHead + ventanaStitchJoints + extraJoint + ventanaStitchTail;
    }
}
