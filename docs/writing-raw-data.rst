Writing Raw Data
================

Application allows you to save any kind of data using *drag and drop* or
scripting interface.

To add an image to ``Images`` tab you can run:

::

    cat image1.png | copyq tab Images write image/png -

This works for any other MIME data type (though unknown formats won't be
displayed properly).
