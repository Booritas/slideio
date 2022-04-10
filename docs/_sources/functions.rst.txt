Module functions
========================
.. automodule:: slideio
   :members:

Examples:

.. code-block:: python

 import slideio
 slideio.get_driver_ids()
 > ['CZI', 'GDAL', 'SVS']
 slide = slideio.open_slide(file_path="/data/a.czi",driver_id="CZI")
 slide.get_num_scenes()
 > 1