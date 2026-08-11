# Philips TIFF Format Detection — Design

**Status:** Approved 2026-08-11
**Driver:** `src/slideio/drivers/svs` (PHTIFF id), `src/slideio/slideio/imagedrivermanager.cpp`
**Addresses:** findings 3 and 4 of the PHTIFF driver review

## 1. Goal

Make the PHTIFF driver reachable through automatic format detection, without it
claiming every TIFF file:

- `openSlide(path)` with no driver id opens a Philips TIFF with the PHTIFF
  driver.
- A TIFF that is not a Philips file continues to be opened by GDAL, and an
  OME-TIFF by OMETIFF.
- `driverOrder` in `ImageDriverManager::findDriver` remains the single
  description of detection precedence.

## 2. Current state

`ImageDriverManager::findDriver` walks a fixed `driverOrder` list and returns
the first driver whose `canOpenFile` accepts the path. Two defects make Philips
support unreachable:

- `"PHTIFF"` is absent from `driverOrder` (`imagedrivermanager.cpp:58`), so the
  driver is never offered a file. A Philips TIFF falls through to GDAL, which
  reads the base directory as a flat image with no pyramid and no auxiliary
  images. Philips files can only be opened by passing the driver id explicitly.
- `SVSImageDriver` does not override `canOpenFile`, so with the PHTIFF id it
  accepts any path matching `*.tif;*.tiff` (`svsimagedriver.cpp:30`). Adding
  the driver to `driverOrder` as it stands would hand it every plain TIFF that
  GDAL is meant to read.

For `.tif`/`.tiff` the extension carries no information: GDAL and PHTIFF both
claim it, and OMETIFF claims the `*.ome.tif*` subset of it. Only the content
distinguishes them.

## 3. Scope

In scope:

- A content test that identifies a Philips TIFF from its metadata.
- A `canOpenFile` override on `SVSImageDriver` applying that test for the
  PHTIFF id.
- `"PHTIFF"` inserted into `driverOrder` ahead of `"GDAL"`.
- A named constant for the PHTIFF driver id.
- Installing the libtiff message handler in the `TIFFKeeper` path constructor.

Out of scope:

- The auxiliary image mapping (fix 2) and the zoom level padding (fix 1), both
  already committed.
- Slide and scene metadata, magnification, and the debug dump of the image
  description (findings 5, 6, 7 of the review).
- `OTImageDriver::getFileSpecs` contains `*ome.tiff` without a separating dot,
  so `genome.tiff` matches the OME-TIFF pattern. Real, unrelated to detection
  precedence, and left for its own change.

## 4. Approach

Detection stays a property of the ordered loop. The manager learns nothing
about Philips; the driver answers for itself, as `DCMImageDriver` already does
for extensionless files.

```
OMETIFF → SVS → CZI → AFI → SCN → DCM → ZVI → NDPI → VSI → QPTIFF → PHTIFF → GDAL
```

- `foo.ome.tiff` matches OMETIFF, which precedes PHTIFF, and is unaffected.
- A Philips `.tif` is rejected by every earlier driver and accepted by PHTIFF.
- Any other `.tif` is rejected by PHTIFF's content test and falls through to
  GDAL, the last entry.

The alternative considered was a dedicated TIFF branch in `findDriver` that
sniffed the description and returned PHTIFF or GDAL directly. It was rejected:
it sends every OME-TIFF to GDAL unless it repeats the OME pattern check, it
hard-codes GDAL as the fallback so `driverOrder` no longer describes what
happens to a TIFF, and it puts format knowledge in the manager, which currently
has none.

## 5. Components

### 5.1 `PHTDescription::isPhilipsDescription(const std::string&)`

New static predicate. A Philips description is an XML document whose root
element is `DataObject` with `ObjectType="DPUfsImport"`.

Order of checks:

1. `find("DPUfsImport")` — a cheap reject for the common case of a description
   that is not Philips at all.
2. One `tinyxml2` parse.
3. Root element present, named `DataObject`, with `ObjectType` equal to
   `DPUfsImport`.

Steps 2 and 3 subsume `Tools::isXml`, which is itself "parses and has a root
element". Calling `isXml` first and then parsing again to reach the root would
walk Philips-2.tiff's 844 KB description twice for the same answer.

A pure function of a string, so its edge cases are testable without a file.

### 5.2 `SVSImageDriver::canOpenFile`

```
ImageDriver::canOpenFile fails   → false      (extension does not match)
driver id is SVS                 → true       (*.svs needs no content test)
driver id is PHTIFF              → content test
```

The content test opens the file with `TIFFKeeper`, reads
`TIFFTAG_IMAGEDESCRIPTION` off the first directory with
`TIFFKeeper::readStringTag`, and passes it to `isPhilipsDescription`. No
directory scan: one open, one tag, close by destructor.

Wrapped in try/catch returning false. `TiffTools::openTiffFile` raises when the
file does not exist, and a detection call must answer rather than throw.

### 5.3 PHTIFF driver id constant

The id is currently a literal in `imagedrivermanager.cpp:87`,
`svsslide.cpp:228` and `svstiledscene.cpp:38`; this change adds a fourth
comparison. Declare it once and use it at all four sites.

### 5.4 `TIFFKeeper` message handler

`TIFFKeeper(libtiff::TIFF*)` installs a `TIFFMessageHandler`;
`TIFFKeeper(const std::string&)` does not. Detection probes files that may not
be TIFFs at all, and libtiff writes its diagnostics to stderr unless the
handler is installed. The path constructor gets the same handler.

## 6. Error handling

Every failure resolves to `false`, letting the loop continue to GDAL:

| Condition | Result |
|---|---|
| File does not exist | `openTiffFile` raises, caught, false |
| Not a TIFF despite the extension | `TIFFOpenW` fails, keeper invalid, false |
| TIFF without an image description | `readStringTag` returns empty, false |
| Description is not Philips XML | false |
| Description is Philips but malformed XML | false |

The last row is a deliberate trade. A strict test routes a corrupt Philips file
to GDAL instead of to PHTIFF's more specific parse error. Loosening the test to
a substring match would improve that diagnostic at the cost of accepting files
the driver cannot then read.

## 7. Testing

`isPhilipsDescription`, no file needed:

- Philips metadata from `MockSVSSlide::createFakeXml` → true.
- OME-XML, an Aperio text description, empty, whitespace only → false.
- Non-XML text containing `DPUfsImport` → false.
- XML whose root `ObjectType` is something else → false.
- A description prefixed with a UTF-8 BOM → true.

`SVSImageDriver::canOpenFile` against real files:

- PHTIFF accepts `philips/Philips-3.tiff`.
- PHTIFF rejects `gdal/img_2448x2448_3x8bit_SRC_RGB_ducks.tif`, an
  `ometiff/*.ome.tiff`, and a `.svs`.
- The SVS id keeps accepting `*.svs` by extension alone.

`findDriver`, end to end:

- `philips/Philips-3.tiff` → PHTIFF.
- `gdal/img_2448x2448_3x8bit_SRC_RGB_ducks.tif` → GDAL.
- `ometiff/*.ome.tiff` → OMETIFF. This is the regression guard for the
  precedence: it fails if PHTIFF is placed ahead of OMETIFF or if detection is
  special-cased on the extension.

Two consequences for the existing tests:

- `PhTiffImageDriverTests.canOpenFile` asserts that the PHTIFF driver accepts
  paths such as `/projects/ometiff.tif` that do not exist. A content test makes
  those false, so the test is rewritten against real files. The negative
  extension cases (`.ometif`, `.ometf2`, …) stay as they are.
- The `findDriver` assertions go in the phtiff suite rather than
  `test_imagedrivermanager.cpp`. That test resolves paths through
  `getTestImagePath` (`SLIDEIO_TEST_DATA_PATH`), while the Philips files live
  under `SLIDEIO_IMAGES_PATH`; putting them there would give the main suite a
  new dataset dependency that the phtiff suite already carries.

## 8. Regulatory note

Detection decides which driver reads a slide, so a file that GDAL previously
opened as a flat image will now be opened as a pyramid with auxiliary images.
This changes what a reader sees and is likely a design change under IEC 62304 /
ISO 13485 design controls rather than a defect fix. Classification to be
confirmed with GRC, Legal or Regulatory Affairs; no clauses are cited here.
