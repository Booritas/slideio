Image Drivers
=================

The slideio module accesses images through a system of image drivers. A driver implements an image format specifics. The driver's capabilities depend on the image format. The table below contains information about driver capabilities.

+--------+-----------------+----------+------------+
| Driver | Multiple Scenes | Z Stacks | Time Frames|
+========+=================+==========+============+
| CZI    |     yes         |   yes    |  yes       |
+--------+-----------------+----------+------------+
|  SVS   | yes             |  no      |    no      |
+--------+-----------------+----------+------------+
| GDAL   | no              |  no      |     no     |
+--------+-----------------+----------+------------+

