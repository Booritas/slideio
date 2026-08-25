# Test Images

Every test image referenced from `src/tests/` and `src/single_tests/`, with where it
lives and what it costs on disk. Written to support deciding what to keep when the
corpus does not fit: the last column is what deleting an image would cost in tests.

Paths are relative to the directory **containing** the `slideio` repository, so that
all three data roots can be written the same way.

| Root | Environment variable | Path |
|---|---|---|
| `full` | `SLIDEIO_IMAGES_PATH` | `images` |
| `std` | `SLIDEIO_TEST_DATA_PATH` | `slideio_extra/testdata/cv/slideio` |
| `priv` | `SLIDEIO_TEST_DATA_PRIV_PATH` | `slideio_extra_priv/testdata/cv/slideio` |

| Suite | Test binary |
|---|---|
| `converter` | `slideio_converter_tests` |
| `main` | `slideio_tests` |
| `ndpi` | `slideio_ndpi_tests` |
| `ometiff` | `slideio_ometiff_tests` |
| `phtiff` | `slideio_phtiff_tests` |
| `pke` | `slideio_pke_tests` |
| `transformer` | `slideio_transformer_tests` |
| `vsi` | `slideio_vsi_tests` |
| *(others)* | standalone programs under `src/single_tests/` |

The **Tests** column counts the distinct `TEST`/`TEST_F` cases naming the image. The
programs under `src/single_tests/` have no test cases, so they show `--`.

## Summary

| | Count | Size |
|---|---:|---:|
| Images referenced by tests | 246 | |
| Present on this machine | 160 | **20.8 GB** |
| Referenced but absent | 86 | -- |
| Static references in test code | 578 | |
| References built at run time | 20 | |

### By directory

| Directory | Size present | Images present | Images absent |
|---|---:|---:|---:|
| `images/czi` | 8.8 GB | 19 | 1 |
| `images/dcm` | 7.5 GB | 12 | 0 |
| `images/zvi` | 2.7 GB | 3 | 0 |
| `images/svs` | 633.3 MB | 3 | 0 |
| `slideio_extra/testdata/cv/slideio/gdal` | 376.7 MB | 21 | 1 |
| `slideio_extra_priv/testdata/cv/slideio/czi` | 285.7 MB | 7 | 0 |
| `slideio_extra/testdata/cv/slideio/zvi` | 132.1 MB | 7 | 0 |
| `slideio_extra_priv/testdata/cv/slideio/svs` | 98.9 MB | 1 | 0 |
| `slideio_extra/testdata/cv/slideio/czi` | 84.7 MB | 8 | 0 |
| `slideio_extra/testdata/cv/slideio/svs` | 84.0 MB | 8 | 1 |
| `images/vsi` | 38.2 MB | 17 | 0 |
| `images/unicode` | 34.2 MB | 7 | 0 |
| `slideio_extra/testdata/cv/slideio/dcm` | 33.6 MB | 22 | 1 |
| `slideio_extra/testdata/cv/slideio/scn` | 23.4 MB | 8 | 0 |
| `slideio_extra_priv/testdata/cv/slideio/dcm` | 18.8 MB | 3 | 0 |
| `slideio_extra/testdata/cv/slideio/ndpi` | 6.3 MB | 2 | 0 |
| `slideio_extra/testdata/cv/slideio/jpeg` | 2.3 MB | 4 | 0 |
| `slideio_extra/testdata/cv/slideio/jxr` | 2.1 MB | 5 | 0 |
| `slideio_extra/testdata/cv/slideio/jp2K` | 367.5 KB | 2 | 0 |
| `slideio_extra_priv/testdata/cv/slideio/afi` | 276 B | 1 | 0 |
| `images/hamamatsu` | -- | 0 | 25 |
| `images/ometiff` | -- | 0 | 40 |
| `images/philips` | -- | 0 | 2 |
| `images/pke` | -- | 0 | 10 |
| `images/scn` | -- | 0 | 5 |

## Images present, largest first

Sizes are what is on disk now. "Tests" counts the distinct tests naming the image, which is what deleting it would cost.

| Location | Size | Suite | Tests |
|---|---:|---|---:|
| `images/czi/30-10-2020_NothingRecognized-15986.czi` | 6.1 GB | main | 3 |
| `images/dcm/private/H01EBB49P-24900` *(dir)* | 4.1 GB | performance | -- |
| `images/zvi/openslide/Zeiss-3-Mosaic.zvi` | 2.0 GB | main | 1 |
| `images/dcm/private/H01EBB50P-24777` *(dir)* | 1.3 GB | main | 9 |
| `images/czi/openslide/Zeiss-4-Mosaic.czi` | 1.1 GB | main | 1 |
| `images/dcm/private/wsi/M01FBC14P-589_level-0.dcm` | 1.1 GB | main | 5 |
| `images/dcm/private/H01EBB50P-24777/H01EBB50P-24777_level-0.dcm` | 968.7 MB | main | 4 |
| `images/svs/S1303802-11-HE-DX1.svs` | 631.0 MB | main, svs_memory | 1 |
| `images/czi/private/E2_A3_W12.czi` | 582.5 MB | main | 1 |
| `images/czi/private/20-024_K5_HE.czi` | 518.9 MB | main | 1 |
| `images/zvi/mouse/20140505_mouse_2cell_H2AUb_RING1B_DAPI_T_005.zvi` | 396.6 MB | main | 2 |
| `images/zvi/mouse/20140207_mouse_2cell_H2AUb_HA_DAPI_inj_002.zvi` | 323.1 MB | main | 1 |
| `images/czi/jxr-16bit-4chnls.czi` | 174.2 MB | converter | 1 |
| `slideio_extra_priv/testdata/cv/slideio/czi/jxr-16bit-4chnls.czi` | 174.2 MB | main | 2 |
| `slideio_extra/testdata/cv/slideio/zvi/Zeiss-1-Stacked.zvi` | 107.7 MB | main | 8 |
| `slideio_extra/testdata/cv/slideio/gdal/test.svs` | 99.3 MB | converter | 2 |
| `slideio_extra_priv/testdata/cv/slideio/svs/jp2k_3chnl_8bit.svs` | 98.9 MB | main | 1 |
| `images/czi/jxr-rgb-5scenes.czi` | 88.8 MB | converter | 2 |
| `slideio_extra_priv/testdata/cv/slideio/czi/jxr-rgb-5scenes.czi` | 88.8 MB | main | 3 |
| `images/czi/private/example_split.czi` | 87.7 MB | main | 1 |
| `images/czi/zeiss.czi` | 87.7 MB | main | 2 |
| `slideio_extra/testdata/cv/slideio/svs/JP2K-33003-1.svs` | 60.9 MB | main, transformer | 9 |
| `slideio_extra/testdata/cv/slideio/czi/03_14_2019_DSGN0545_A_wb_1353_fov_1_633.czi` | 45.9 MB | main | 2 |
| `slideio_extra/testdata/cv/slideio/gdal/multipage-ducks.tif` | 45.7 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.jpg` | 39.1 MB | converter, main, transformer | 20 |
| `images/czi/doughnut.czi` | 36.3 MB | converter | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_3x16bit_SRC_RGB_ducks.raw` | 34.3 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_3x16bit_SRC_RGB_ducks.tif` | 30.1 MB | main, phtiff | 3 |
| `slideio_extra/testdata/cv/slideio/gdal/Airbus_Pleiades_50cm_8bit_RGB_Yogyakarta.svs` | 28.4 MB | converter | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_3x16bit_SRC_RGB_ducks.png` | 24.2 MB | main | 1 |
| `images/unicode/тест/Leica-Fluorescence-1.scn` | 20.7 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/scn/Leica-Fluorescence-1.scn` | 20.7 MB | converter, main | 18 |
| `slideio_extra/testdata/cv/slideio/svs/CMU-1-Small-Region-page-0.tif` | 18.8 MB | main | 3 |
| `slideio_extra/testdata/cv/slideio/czi/PYP-467.czi` | 18.5 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_3x8bit_SRC_RGB_ducks.bmp` | 17.1 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_3x8bit_SRC_RGB_ducks.raw` | 17.1 MB | main | 1 |
| `slideio_extra_priv/testdata/cv/slideio/czi/jxr-16bit-4chnls.preview.tiff` | 14.7 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/benigns_01/patient0186/0186.LEFT_MLO.frames/frame0.tif` | 13.3 MB | main | 3 |
| `slideio_extra/testdata/cv/slideio/czi/08_18_2018_enc_1001_633.czi` | 11.7 MB | converter, main | 3 |
| `images/vsi/Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi` | 11.6 MB | vsi | 6 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_1x16bit_SRC_RGB_ducks.raw` | 11.4 MB | main | 2 |
| `slideio_extra_priv/testdata/cv/slideio/dcm/series` *(dir)* | 10.3 MB | main | 2 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_1x16bit_SRC_RGB_ducks.tif` | 9.8 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/zvi/Zeiss-1-Merged.zvi` | 9.7 MB | converter, main | 11 |
| `slideio_extra_priv/testdata/cv/slideio/dcm/series/series_1` *(dir)* | 8.0 MB | main | 2 |
| `slideio_extra/testdata/cv/slideio/dcm/benigns_01/patient0186/0186.LEFT_MLO.dcm` | 7.5 MB | main | 3 |
| `slideio_extra/testdata/cv/slideio/dcm/benigns_01/patient0186/0186.LEFT_CC.dcm` | 7.5 MB | main | 2 |
| `slideio_extra_priv/testdata/cv/slideio/czi/jxr-rgb-5scenes.preview.tiff` | 5.8 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_1x8bit_SRC_GRAY_ducks.bmp` | 5.7 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_1x8bit_SRC_GRAY_ducks.raw` | 5.7 MB | main | 1 |
| `images/czi/pJP31mCherry.czi` | 5.3 MB | converter | 1 |
| `images/unicode/тест/pJP31mCherry.czi` | 5.3 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/czi/pJP31mCherry.czi` | 5.3 MB | main | 6 |
| `images/vsi/test-output/vsi-ets-test-jpg2k_tile_5.tif` | 5.3 MB | vsi | 1 |
| `images/vsi/private/d/STS_G6889_11_1_pHH3.vsi` | 4.1 MB | vsi | 2 |
| `images/vsi/test-output/vsi-ets-test-jpg2k.vsi.ome.tif` | 3.9 MB | vsi | 3 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_3x8bit_SRC_RGB_ducks.png` | 3.6 MB | converter, main | 15 |
| `images/czi/T_3_CH_2.czi` | 3.6 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/ndpi/test3-DAPI-2-(387).ndpi` | 3.4 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/zvi/Zeiss-1-Merged-ch0.tif` | 3.2 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/zvi/Zeiss-1-Merged-ch1.tif` | 3.2 MB | main | 1 |
| `images/unicode/тест/test3-TRITC 2 (560).ndpi` | 2.9 MB | ndpi | 1 |
| `slideio_extra/testdata/cv/slideio/ndpi/test3-TRITC 2 (560).ndpi` | 2.9 MB | ndpi | -- |
| `images/unicode/тест/TOMMAlexaFluor647.zvi` | 2.8 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/zvi/TOMMAlexaFluor647.zvi` | 2.8 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/zvi/Zeiss-1-Stacked/zvi_slice_6_channel_1` | 2.8 MB | main | 3 |
| `slideio_extra/testdata/cv/slideio/zvi/Zeiss-1-Stacked/zvi_slice_7_channel_2` | 2.8 MB | main | 1 |
| `images/vsi/private/3d/01072022_35_2_z.vsi` | 2.7 MB | vsi | 1 |
| `images/czi/test/example_split (1).czi - ScanRegion0 (1, x=41169, y=4850, w=1000, h=1000).png` | 2.6 MB | main, phtiff | 2 |
| `images/czi/test/example_split (1).czi - ScanRegion0 (1, x=17583, y=3676, w=1000, h=1000).png` | 2.6 MB | main, phtiff | 2 |
| `images/czi/test/example_split (1).czi - ScanRegion0 (1, x=2668, y=1376, w=1000, h=1000).png` | 2.4 MB | main, phtiff | 2 |
| `images/vsi/private/3d/test-images/01072022_35_2_z.vsi - 60x_BF_Z_01 (1, x=45625, y=42302, w=984, h=1015).png` | 2.1 MB | vsi | 1 |
| `images/vsi/vs200-vsi-share/Image_B309.vsi` | 2.1 MB | vsi | 1 |
| `images/vsi/Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi` | 2.0 MB | vsi | 12 |
| `slideio_extra_priv/testdata/cv/slideio/czi/jxr-rgb-5scenes.label.tiff` | 2.0 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/jxr/seagull.bmp` | 1.9 MB | main | 4 |
| `images/svs/CMU-1-Small-Region.svs` | 1.8 MB | main | -- |
| `images/unicode/тест/CMU-1-Small-Region.svs` | 1.8 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/svs/CMU-1-Small-Region.svs` | 1.8 MB | main, phtiff | 20 |
| `slideio_extra/testdata/cv/slideio/gdal/img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.bmp` | 1.8 MB | main | 1 |
| `images/czi/test/zeiss-block.png` | 1.6 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/jpeg/p2YCpvg.png` | 1.6 MB | main | 1 |
| `images/vsi/OS-1/OS-1.vsi` | 1.3 MB | vsi | 1 |
| `slideio_extra/testdata/cv/slideio/svs/CMU-1-Small-Region-page-1.tif` | 1.3 MB | main, phtiff | 5 |
| `slideio_extra/testdata/cv/slideio/gdal/img_2448x2448_1x8bit_SRC_GRAY_ducks.png` | 1.1 MB | converter, main | 4 |
| `images/vsi/test-output/G1M16_ABD_HE_B6.vsi-40x_01(1,x=5836,y=11793,w=849,h=607).png` | 1.1 MB | vsi | 4 |
| `slideio_extra/testdata/cv/slideio/gdal/test.tif` | 1.1 MB | main | 1 |
| `images/vsi/test-output/STS_G6889_11_1_pHH3.vsi - 40x_BF_01 (1, x=82570, y=77046, w=1153, h=797).png` | 1.1 MB | vsi | 1 |
| `images/dcm/barre.dev/MultiFrame/MR-MONO2-8-16x-heart` | 1.0 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/MR-MONO2-8-16x-heart` | 1.0 MB | main | 1 |
| `slideio_extra/testdata/cv/slideio/czi/corrupted.czi` | 1014.0 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/US-RGB-8-epicard` | 901.0 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/XA-MONO2-8-12x-catheter` | 899.5 KB | main | 2 |
| `slideio_extra/testdata/cv/slideio/scn/Leica-Fluorescence-1/x2500-y2338-600x500.bmp` | 879.0 KB | main | 2 |
| `images/czi/16bit_CH_1_doughnut_crop.czi` | 875.9 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/czi/pJP31mCherry.grey/pJP31mCherry_b0t0z0c0x0-512y0-512.bmp` | 768.1 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/czi/pJP31mCherry.grey/pJP31mCherry_b0t0z0c1x0-512y0-512.bmp` | 768.1 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/czi/pJP31mCherry.grey/pJP31mCherry_b0t0z0c2x0-512y0-512.bmp` | 768.1 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/multipage.tif` | 704.7 KB | main, phtiff | 4 |
| `images/czi/test/bug_2D_rgb_compressed.png` | 641.8 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/svs/CMU-1-Small-Region-page-2.bmp` | 526.4 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/jpeg/p2YCpvg.jpeg` | 515.5 KB | main | 1 |
| `images/unicode/тест/CT-MONO2-12-lomb-an2` | 513.2 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/CT-MONO2-12-lomb-an2` | 513.2 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/openmicroscopy.org/CT1_J2KI.tiff` | 512.5 KB | main | 2 |
| `slideio_extra_priv/testdata/cv/slideio/dcm/series/series_1/tests/IMG-0001-00005.tiff` | 512.5 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/scn/Leica-Fluorescence-1/dir_0_tile_1-7.png` | 472.8 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/US-PAL-8-10x-echo` | 472.3 KB | main | 6 |
| `slideio_extra/testdata/cv/slideio/scn/Leica-Fluorescence-1/tile.png` | 448.6 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/scn/Leica-Fluorescence-1/thumbnail.png` | 424.2 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/svs/corrupted.svs` | 397.0 KB | main | 2 |
| `images/svs/test/S1303802-11-HE-DX1-block.png` | 395.5 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/тест/тест.tif` | 360.4 KB | main | 2 |
| `slideio_extra/testdata/cv/slideio/jp2K/relax.bmp` | 351.6 KB | main | 2 |
| `images/dcm/private/wsi/M01FBC14P-589_level-0.block.dcm` | 341.0 KB | main | 2 |
| `images/dcm/private/H01EBB50P-24777.block-3.png` | 330.2 KB | main | 1 |
| `images/vsi/test-output/Image_B309_Macro.png` | 292.3 KB | vsi | 1 |
| `images/czi/test/16bit_CH_1_doughnut_crop.tiff` | 259.1 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/scn/Leica-Fluorescence-1/dir_6_tile_6-8.bmp` | 257.1 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/scn/Leica-Fluorescence-1/dir_8_tile_6-8.bmp` | 257.1 KB | main | 2 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/OT-MONO2-8-hip.dcm` | 256.4 KB | main | 6 |
| `images/dcm/private/H01EBB50P-24777.block.png` | 250.1 KB | main | 1 |
| `images/vsi/test-output/G1M16_ABD_HE_B6.vsi-40x_01(1,x=0,y=0,w=512,h=512).png` | 227.6 KB | vsi | 1 |
| `images/vsi/test-output/1286FL9057GDF8RGDX257R2GLHZ.png` | 205.9 KB | vsi | 3 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/OT-MONO2-8-hip.frames/frame0.png` | 179.1 KB | main | 3 |
| `slideio_extra/testdata/cv/slideio/svs/CMU-1-Small-Region-page-0-tile_5-5.bmp` | 168.8 KB | main | 3 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/US-RGB-8-epicard.frames/frame0.png` | 148.5 KB | main | 1 |
| `slideio_extra_priv/testdata/cv/slideio/czi/jxr-rgb-5scenes.thumb.png` | 145.3 KB | main | 1 |
| `images/dcm/spine_mr/DICOMDIR` | 143.3 KB | main | 3 |
| `slideio_extra/testdata/cv/slideio/jpeg/lena_256.png` | 137.8 KB | main | 3 |
| `images/vsi/test-output/Image_B309_Overview.png` | 134.6 KB | vsi | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/MR-MONO2-12-angio-an1.frames/frame0.tif` | 128.2 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/svs/tests/JP2K-33003-1.png` | 127.7 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/jxr/tile16.raw` | 110.1 KB | main | 1 |
| `images/dcm/private/wsi/M01FBC14P-589_level-0.tile.png` | 102.8 KB | main | 1 |
| `images/vsi/vsi-multifile/vsi-ets-test-jpg2k.vsi` | 101.2 KB | vsi | 4 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/MR-MONO2-12-angio-an1` | 96.6 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/CT-MONO2-12-lomb-an2.frames/frame0.png` | 93.4 KB | main | 1 |
| `images/dcm/private/H01EBB50P-24777.block-2.png` | 71.8 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/jxr/seagull.wdp` | 71.0 KB | main | 3 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/XA-MONO2-8-12x-catheter.frames/frame5.png` | 70.1 KB | main | 2 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/XA-MONO2-8-12x-catheter.frames/frame6.png` | 70.1 KB | main | 2 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/US-PAL-8-10x-echo.frames/frame5.png` | 69.6 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/US-PAL-8-10x-echo.frames/frame6.png` | 69.5 KB | main | 1 |
| `slideio_extra_priv/testdata/cv/slideio/czi/jxr-16bit-4chnls.thumb.png` | 64.0 KB | main | 1 |
| `images/unicode/тест/lena_256.jpg` | 42.4 KB | main | 2 |
| `slideio_extra/testdata/cv/slideio/jpeg/lena_256.jpg` | 42.4 KB | main | 1 |
| `images/czi/bug_2D_rgb_compressed.czi` | 36.8 KB | main | 2 |
| `images/dcm/private/H01EBB50P-24777.tile.png` | 29.4 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/jxr/corrupted.wdp` | 19.8 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/MR-MONO2-8-16x-heart.frames/frame6.png` | 18.3 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/barre.dev/MR-MONO2-8-16x-heart.frames/frame5.png` | 17.2 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/jp2K/relax.jp2` | 15.9 KB | main | 2 |
| `slideio_extra/testdata/cv/slideio/dcm/openmicroscopy.org/CT1_J2KI` | 13.8 KB | main | 2 |
| `images/vsi/Zenodo/Abdominal/G1M16_ABD_HE_B6.aux.png` | 11.0 KB | vsi | 1 |
| `slideio_extra/testdata/cv/slideio/scn/z-stack.xml` | 8.0 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/colors.png` | 6.3 KB | phtiff, transformer | 10 |
| `slideio_extra/testdata/cv/slideio/jxr/tile16.jxr` | 5.5 KB | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/img_1024x600_3x8bit_RGB_color_bars_CMYKWRGB.png` | 2.7 KB | main | 3 |
| `slideio_extra_priv/testdata/cv/slideio/afi/fs.afi` | 276 B | main | 1 |

## Images referenced but not present

Referenced by the tests below but absent from this machine. These are the tests that fail, or that skip when `SLIDEIO_SKIP_MISSING_IMAGES` is set.

| Location | Size | Suite | Tests |
|---|---:|---|---:|
| `images/czi/2017-02-27 15.22.39.ndpi` | -- | memory_leaks | -- |
| `images/hamamatsu/2017-02-27 15.29.08-2.png` | -- | ndpi | 1 |
| `images/hamamatsu/2017-02-27 15.29.08.macro.png` | -- | ndpi | 1 |
| `images/hamamatsu/2017-02-27 15.29.08.map.png` | -- | ndpi | 1 |
| `images/hamamatsu/2017-02-27 15.29.08.ndpi` | -- | ndpi | 8 |
| `images/hamamatsu/DM0014 - 2020-04-02 10.25.21-roi-resampled-tiled.png` | -- | ndpi | 1 |
| `images/hamamatsu/DM0014 - 2020-04-02 10.25.21-roi-resampled.png` | -- | ndpi | 1 |
| `images/hamamatsu/DM0014 - 2020-04-02 10.25.21-tile.png` | -- | ndpi | 1 |
| `images/hamamatsu/DM0014 - 2020-04-02 10.25.21.ndpi` | -- | ndpi | 3 |
| `images/hamamatsu/DM0014 - 2020-04-02 11.10.47-resampled.png` | -- | ndpi | 1 |
| `images/hamamatsu/DM0014 - 2020-04-02 11.10.47.ndpi` | -- | ndpi | 3 |
| `images/hamamatsu/HE_Hamamatsu-roi-gray.png` | -- | ndpi | 1 |
| `images/hamamatsu/HE_Hamamatsu-roi-inversed.png` | -- | ndpi | 1 |
| `images/hamamatsu/HE_Hamamatsu-roi.png` | -- | ndpi | 1 |
| `images/hamamatsu/HE_Hamamatsu.ndpi` | -- | ndpi | 3 |
| `images/hamamatsu/openslide/CMU-1-1.png` | -- | ndpi | 2 |
| `images/hamamatsu/openslide/CMU-1-dir.png` | -- | main, ndpi | 3 |
| `images/hamamatsu/openslide/CMU-1-dir1.png` | -- | main | 1 |
| `images/hamamatsu/openslide/CMU-1-scanline.png` | -- | ndpi | 1 |
| `images/hamamatsu/openslide/CMU-1-tile.png` | -- | ndpi | 1 |
| `images/hamamatsu/openslide/CMU-1.ndpi` | -- | ndpi, ndpi_memory | 7 |
| `images/hamamatsu/openslide/CMU-1_002.tif` | -- | ndpi | 2 |
| `images/hamamatsu/openslide/CMU-2-roi-l0.png` | -- | ndpi | 2 |
| `images/hamamatsu/openslide/CMU-2.ndpi` | -- | ndpi | 2 |
| `images/hamamatsu/test3-TRITC 2 (560)-roi.png` | -- | ndpi | 1 |
| `images/hamamatsu/test3-TRITC 2 (560).ndpi` | -- | ndpi | 1 |
| `images/ometiff/00001_01.ome.tiff` | -- | phtiff | 2 |
| `images/ometiff/4D-Series/4D-series.ome.tiff` | -- | ometiff | 1 |
| `images/ometiff/Iron-Plate.ome.tiff` | -- | ometiff | 1 |
| `images/ometiff/LAMBDA-ModuloAlongZ-ModuloAlongT.ome.tiff` | -- | main | -- |
| `images/ometiff/Multifile/multifile-Z1.ome.tiff` | -- | ometiff | 3 |
| `images/ometiff/Multifile2/multifile-Z1.ome.tiff` | -- | ometiff | 1 |
| `images/ometiff/SPIM-ModuloAlongZ.ome.tiff` | -- | main | 1 |
| `images/ometiff/Subresolutions/Leica-1.ome.tiff` | -- | main, ometiff | 9 |
| `images/ometiff/Subresolutions/Leica-2.ome.tiff` | -- | main, ometiff | 8 |
| `images/ometiff/Subresolutions/retina_large.ome.tiff` | -- | main, ometiff | 8 |
| `images/ometiff/Tests/Iron-Plate (1, x=144, y=146, w=258, h=175).tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/Leica-1.ome-page_1.tif` | -- | ometiff | 2 |
| `images/ometiff/Tests/Leica-1.ome.tiff - Series 1 (1, x=21504, y=15360, w=512, h=512).png` | -- | ometiff | 2 |
| `images/ometiff/Tests/Leica-1.ome.tiff - Series 1 (1, x=24000, y=18000, w=2000, h=1000).png` | -- | ometiff | 2 |
| `images/ometiff/Tests/Leica-1.ome.tiff - Series 1 (4, x=24000, y=18000, w=2000, h=1000).png` | -- | ometiff | 2 |
| `images/ometiff/Tests/Leica-2.ome.tiff - Series 1 (1, x=23552, y=14336, w=512, h=512).png` | -- | ometiff | 1 |
| `images/ometiff/Tests/ULT-2020-111-014_1 (1, x=28333, y=36086, w=1099, h=760).tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/ULT-2020-111-014_1 (1, x=4375, y=39330, w=1153, h=743).tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/page_24.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/page_25.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/page_26.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/page_27.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/page_28.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/page_29.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/page_30.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/page_31.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/retina_large.ome-page32-channel-0.tif` | -- | ometiff | 3 |
| `images/ometiff/Tests/retina_large.ome-page32-channel-1.tif` | -- | ometiff | 2 |
| `images/ometiff/Tests/test.ome.tif - USL-2023-53777-20 (1, x=16245, y=23321, w=1028, h=640).tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/tubhiswt4D-C0-T20-Z3.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/tubhiswt4D-C0-T20-Z4.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/tubhiswt4D-C0-T20-Z5.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/tubhiswt4D-C0-T20-Z6.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/tubhiswt4D-C1-T20-Z5.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/tubhiswt4D-C1-T21-Z5.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/tubhiswt4D-C1-T22-Z5.tif` | -- | ometiff | 1 |
| `images/ometiff/Tests/tubhiswt4D-C1-T23-Z5.tif` | -- | ometiff | 1 |
| `images/ometiff/private/ULT-2020-111-014-1.ome.tif` | -- | ometiff | 1 |
| `images/ometiff/private/test.ome.tif` | -- | ometiff | 2 |
| `images/ometiff/tubhiswt-4D/tubhiswt_C0_TP0.ome.tif` | -- | ometiff | 3 |
| `images/philips/Philips-3.tiff` | -- | phtiff | 6 |
| `images/philips/Philips-4.tiff` | -- | phtiff | 2 |
| `images/pke/openmicroscopy/PKI_scans/HandEcompressed_Scan1.qptiff` | -- | pke | 5 |
| `images/pke/openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff` | -- | pke | 13 |
| `images/pke/test-images/HandEcompressed_Scan1 (1, x=11190, y=8580, w=1622, h=963).png` | -- | pke | 1 |
| `images/pke/test-images/HandEcompressed_Scan1-low.png` | -- | pke | 1 |
| `images/pke/test-images/LuCa-7color_Scan1-low.png` | -- | pke | 2 |
| `images/pke/test-images/LuCa-7color_Scan1.label.png` | -- | pke | 1 |
| `images/pke/test-images/LuCa-7color_Scan1.overv.png` | -- | pke | 1 |
| `images/pke/test-images/LuCa-7color_Scan1.qptiff - resolution #1 (1, x=11619, y=16875, w=1202, h=756).tif` | -- | pke | 2 |
| `images/pke/test-images/LuCa-7color_Scan1.qptiff - resolution #1 (1, x=4981, y=10654, w=2367, h=1578).tif` | -- | pke | 1 |
| `images/pke/test-images/LuCa-7color_Scan1.thumb.png` | -- | pke | 1 |
| `images/scn/private/HER2-63x_1.scn` | -- | main | 4 |
| `images/scn/private/page-67-StitchAB907A82-6319-422F-9B5B-EB0E0A9D0525-z=4-r=0-c=2.tiff` | -- | main | 2 |
| `images/scn/ultivue/Leica Aperio Versa 5 channel fluorescent image.scn` | -- | main | 4 |
| `images/scn/ultivue/test/Leica Aperio Versa 5 channel fluorescent image-label-temp.png` | -- | main | 1 |
| `images/scn/ultivue/test/Leica Aperio Versa 5 channel fluorescent image-label.png` | -- | main | 1 |
| `slideio_extra/testdata/cv/slideio/dcm/CT-MONO2-12-lomb-an2` | -- | main | 1 |
| `slideio_extra/testdata/cv/slideio/gdal/non_existent_file.png` | -- | main | 1 |
| `slideio_extra/testdata/cv/slideio/svs/TOMMAlexaFluor647.zvi` | -- | main | 1 |

## References built at run time

These call sites compose the path from a variable or a constant, so the file cannot be
identified by reading the source. They are not counted in the tables above.

| File | Line | Test | Arguments |
|---|---:|---|---|
| `src/tests/main/test_afi_driver.cpp` | 17 | `(outside a test)` | `dir, file, true` |
| `src/tests/main/test_czi_driver.cpp` | 212 | `CZIImageDriver.readBlock4D` | `"czi",bmpFileName` |
| `src/tests/main/test_czi_driver.cpp` | 337 | `CZIImageDriver.sceneIdsFromDims` | `"czi",imageName` |
| `src/tests/main/test_czi_driver.cpp` | 376 | `CZIImageDriver.slideRawMetadata` | `"czi",imageName` |
| `src/tests/main/test_czi_driver.cpp` | 407 | `CZIImageDriver.metadataCompression` | `"czi",imageName` |
| `src/tests/main/test_fiwrapper.cpp` | 89 | `FIWrapper.emptyFilePath` | `"gdal", testFileName` |
| `src/tests/main/test_gdal_driver.cpp` | 230 | `GDALDriver.metadataCompression` | `"gdal",std::get<0>(item)` |
| `src/tests/main/test_scn_driver.cpp` | 48 | `SCNImageDriver.slideRawMetadata` | `"scn", imageName` |
| `src/tests/main/test_svs_driver.cpp` | 358 | `SVSImageDriver.metadataCompression` | `"svs",imageName` |
| `src/tests/main/test_svs_driver.cpp` | 376 | `SVSImageDriver.slideRawMetadata` | `"svs",imageName` |
| `src/tests/main/test_zvi_driver.cpp` | 204 | `ZVIImageDriver.readBlock3Layers` | `"zvi", channelName` |
| `src/tests/phtiff/test_phtiff_driver.cpp` | 430 | `PhTiffImageDriverTests.canOpenFileByContent` | `"philips", fileName` |
| `src/tests/phtiff/test_phtiff_driver.cpp` | 1094 | `PhTiffImageDriverTests.metadataOfTheTestFiles` | `"philips", fileName` |
| `src/tests/phtiff/test_phtiff_driver.cpp` | 1154 | `PhTiffImageDriverTests.magnificationOfTheTestFiles` | `"philips", param.first` |
| `src/tests/phtiff/test_phtiff_driver.cpp` | 1206 | `PhTiffImageDriverTests.auxImagesOfTheTestFiles` | `"philips", param.first` |
| `src/tests/phtiff/test_phtiff_driver.cpp` | 2278 | `PHTDescriptionTests.isPhilipsDescriptionAcceptsBomPrefixedMetadata` | `"philips", fileName` |
| `src/tests/phtiff/test_phtiff_driver.cpp` | 2288 | `PHTDescriptionTests.isPhilipsDescriptionAcceptsBomPrefixedMetadata` | `"philips", ph2::REFERENCE_PNG` |
| `src/tests/phtiff/test_phtiff_driver.cpp` | 2689 | `PhTiffImageDriverTests.multiThreadedRead` | `"philips", ph2::FILE_NAME` |
| `src/tests/pke/test_pke_driver.cpp` | 366 | `PKEImageDriverTests.readStripedDir5Channels_SingleChannel` | `"pke", fileName` |
| `src/tests/pke/test_pke_driver.cpp` | 397 | `PKEImageDriverTests.readStripedDir5ChannelsAllChannels` | `"pke", fileName` |

## How this was produced

By `auxfiles/list-test-images.py`, which parses the two `TestTools` path helpers out
of every `.cpp` under `src/tests/` and `src/single_tests/` (excluding
`testlib/testtools.cpp`, which merely defines them) and records the enclosing test.
Arguments that are string literals -- including concatenated and `u8`-prefixed ones --
are resolved against the roots above and stat-ed; anything assembled from a variable is
listed separately rather than guessed at. A directory argument is marked *(dir)* and
its size is the sum of the files beneath it.

The document is a snapshot: it reflects the test sources at the commit it was generated
from and the files present on one machine. Regenerate it rather than editing by hand.
