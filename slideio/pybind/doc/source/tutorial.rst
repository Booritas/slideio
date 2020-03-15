Quick Tutorial
===============

Read the whole image
--------------------

.. code-block:: python

   import slideio
   slide = slideio.open_slidei("/data/test.png", "GDAL")
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
   slide = slideio.open_slidei("/data/test.png", "GDAL")
   scene = slide.get_scene(0)
   image = scene.read_block(channel_indices=[0])
