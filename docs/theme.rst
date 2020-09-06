.. _theme:

Theme
=====

Application style can be configured in the Appearance configuration tab and
using Cascading Style Sheets (CSS).

Appearance Configuration
------------------------

The Appearance tab in configuration dialog allows to change font and colors of
the item list and other GUI elements in the main window.

By default, only the item list and internal item editor style is changed. To
change theme of whole the main window (menu bar, tool bar, tabs) and menus,
enable option "Set colors for tabs, tool bar and menus".

.. note::

    Some desktop environments handle the tray menu style by themselves and it
    cannot be changed in CopyQ.

You can change style in more detail by using "Edit Theme" button.

.. seealso::

    :ref:`faq-hide-menu-bar`

Style Sheets
------------

The appearance options are the used in application CSS files installed with
CopyQ (e.g.  placeholders in the files like ``${font}``). You can list the
theme installation path with ``copyq info themes`` command.

To override a CSS file, copy the file to your configuration directory under
``themes`` subdirectory. For example, override the style sheet for the item
list:

.. code-block:: bash

    $ copyq info themes
    /usr/share/copyq/themes

    $ copyq info config
    /home/me/.config/copyq/copyq.conf

    $ cp /usr/share/copyq/themes/items.css /home/me/.config/copyq/themes/

    $ $EDITOR /home/me/.config/copyq/themes/items.css

To reload the style sheets, you need to restart CopyQ or go to the
configuration dialog and click OK button.

You can set ``COPYQ_THEME_PREFIX`` environment variable for the preferred path
for CSS files.

Here are some special placeholders for CSS files:

- ``${css:scrollbar}`` - include ``scrollbar.css`` style sheet.
- ``${scale = 0.5}`` - set scaling for sizes and font (reset with value 1)
- ``${hsv_saturation_factor = 2}`` - set saturation for colors in the rest of
  the style sheet
- ``${hsv_value_factor = 0.9}`` - set value for colors in the rest of the style
  sheet
