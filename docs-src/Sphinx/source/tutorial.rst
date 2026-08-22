Quick Tutorial
===============

Read the whole image
--------------------

.. code-block:: python

   import slideio
   slide = slideio.open_slide("/data/test.png", "GDAL")
   scene = slide.get_scene(0)
   image = scene.read_block()

Read a rectangular block of an image
-------------------------------------

.. code-block:: python
 
   import slideio
   slide = slideio.open_slide("/data/test.svs", "SVS")
   scene = slide.get_scene(0)
   image = scene.read_block((0,0,1000,1000))

Read a rectangular block with rescaling
----------------------------------------

 .. code-block:: python
 
   import slideio
   slide = slideio.open_slide("/data/test.svs", "SVS")
   scene = slide.get_scene(0)
   image = scene.read_block((0,0,10000,10000), (500,500))

Read 10 z-slices of a rectangular block with rescaling
------------------------------------------------------

 .. code-block:: python
 
   import slideio
   slide = slideio.open_slide("/data/test.czi", "CZI")
   scene = slide.get_scene(0)
   image = scene.read_block((0,0,1000,1000), (500,500), slices=(0,10))

Iterate through scenes
-----------------------

 .. code-block:: python

   import slideio
   slide = slideio.open_slide("/data/test.czi", "CZI")
   num_scenes = slide.num_scenes
   for index in range(0, num_scenes):
      print(slide.get_scene(index).name)

Read a single channel of an image
-----------------------------------

.. code-block:: python

   import slideio
   slide = slideio.open_slide("/data/test.png", "GDAL")
   scene = slide.get_scene(0)
   image = scene.read_block(channel_indices=[0])

Inspect the zoom levels of a scene
-----------------------------------

.. code-block:: python

   import slideio
   slide = slideio.open_slide("/data/test.svs")
   scene = slide.get_scene(0)
   for index in range(scene.num_zoom_levels):
      info = scene.get_zoom_level_info(index)
      print(info)

Read a block from an explicitly selected zoom level
----------------------------------------------------

Unlike *read_block*, *read_block_from_level* reads from the level you name and no other, and the
rectangle is given in the coordinate system of that level. Use it when you already know which level
you want, so that no coordinate conversion and no implicit level selection happen on the way.

.. code-block:: python

   import slideio
   slide = slideio.open_slide("/data/test.svs")
   scene = slide.get_scene(0)
   # a 1000x1000 region at the origin of level 2, in the pixels of level 2
   image = scene.read_block_from_level(2, (0, 0, 1000, 1000))

Walk a zoom level tile by tile
-------------------------------

.. code-block:: python

   import slideio
   slide = slideio.open_slide("/data/test.svs")
   scene = slide.get_scene(0)
   level = 2
   info = scene.get_zoom_level_info(level)
   for tile in range(info.tile_count):
      rect = info.get_tile_rect(tile)
      image = scene.read_block_from_level(level, (rect.x, rect.y, rect.width, rect.height))

On a level with more than one tile every tile rectangle has the same size, so the tiles at the right
and bottom edge overhang the level. The part of such a rectangle that falls outside the level comes
back as background. On a level that consists of a single tile the rectangle is the level itself.

More examples
--------------

The `slideio tutorial <https://github.com/Booritas/slideio-tutorial>`_ repository contains jupyter
notebooks with complete examples, among them
`zoom-levels.ipynb <https://github.com/Booritas/slideio-tutorial/blob/master/zoom-levels.ipynb>`_ on
reading from an explicitly selected pyramid level.
