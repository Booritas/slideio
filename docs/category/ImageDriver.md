---
layout: category
title: Image Drivers
sidebar_sort_order: 500
---
**SlideIO** library uses *driver* pattern to read information of specific image formats. Each driver implements all necessary functionality to accesss one or more image formats. Each driver has a unique ID. Currently the following drivers are available.

| Driver ID | File format                                                                                                                                  | File extensions                  |
|--------|----------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------|
| SVS    | [Aperio SVS](https://www.leicabiosystems.com/en-de/digital-pathology/manage/aperio-imagescope/)                                              | *.svs                            |
| AFI    | [Aperio AFI - Fluorescent images](https://www.pathologynews.com/fileformats/leica-afi/)                                                      | *.afi                            |
| SCN    | [Leica](https://www.leica-microsystems.com/) SCN images                                                                                      | *.scn                            |
| CZI    | [Zeiss CZI](https://www.zeiss.com/microscopy/en/products/software/zeiss-zen/czi-image-file-format.html) images                               | *.czi                            |
| ZVI    | Zeiss ZVI image format                                                                                                                       | *.zvi                            |
| DCM    | DICOM images                                                                                                                                 | *.dcm, no extension              |
| NDPI   | [Hamamatsu NDPI image format](https://www.hamamatsu.com/eu/en/product/life-science-and-medical-systems/digital-slide-scanner/U12388-01.html) | *.ndpi                           |
| GDAL   | General image fomtes                                                                                                                         | *.jpeg,*.jpg,*.tiff,*.tiff,*.png |

## Descriptions of the drivers