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

CSS files can contain placeholders like ``${bg}`` which are defined in theme
configuration file. You can edit this file in Appearance configuration tab with
"Edit Theme" button.

Placeholder can be assigned to colors in following formats:

- ``#RGB`` (each of R, G, and B is a single hex digit)
- ``#RRGGBB``
- ``#AARRGGBB`` (with alpha channel)
- `a color name <https://www.w3.org/TR/SVG11/types.html#ColorKeywords>`__
- ``transparent``
- ``rgba(R,G,B,A)`` (each of R, G, and B is 0-255, A is alpha channel 0.0-1.0)

There are extra color names for current system theme:

- ``default_bg`` - background for list and line/text edit widgets
- ``default_text`` - foreground color for the above
- ``default_placeholder_text`` - placeholder text color
- ``default_alt_bg`` - alternative item background
- ``default_highlight_bg`` - highlight background
- ``default_highlight_text`` - highlighted text color
- ``default_tooltip_bg`` - tooltip background
- ``default_tooltip_text`` - tooltip text color
- ``default_window`` - window background
- ``default_window_text`` - window text color
- ``default_button`` - button background
- ``default_button_text`` - button text color
- ``default_bright_text`` - bright window text color
- ``default_light`` - lighter than button
- ``default_midlight`` - between button and light
- ``default_dark`` - darker than button
- ``default_mid`` - between button and dark
- ``default_shadow`` - very dark
- ``default_link`` - hyperlink color
- ``default_link_visited`` - visited hyperlink color

Placeholder can be also assigned color expressions, for example:

- ``sel_bg=bg + #000409 - #100``
- ``menu_bar_css="background: ${bg}; color: ${fg + #444}"``
- ``${bg + #333}`` (directly in CSS)

Here are some special placeholders for CSS files:

- ``${css:scrollbar}`` - include ``scrollbar.css`` style sheet.
- ``${scale = 0.5}`` - set scaling for sizes and font (reset with value 1)
- ``${hsv_saturation_factor = 2}`` - set saturation for colors in the rest of
  the style sheet
- ``${hsv_value_factor = 0.9}`` - set value for colors in the rest of the style
  sheet

Plugin Indicators
~~~~~~~~~~~~~~~~~

Some plugins draw visual indicators on items. The **pinned** plugin draws a
vertical border on the right edge, and the **notes** plugin draws a small bar
next to the note text.

Both indicators derive their color from the CSS ``color`` property of the item
widget and are rendered semi-transparent by default.

To set the indicator color or width explicitly, use the ``qproperty-`` CSS
properties on the plugin widget class. When a color property is set, it is used
exactly as specified with no alpha modification.

.. note::

    Qt ``qproperty-`` values are applied once when the rule matches and are not
    reverted when the rule stops matching. To vary a property by state (e.g.
    selected vs. unselected), you must set explicit values for **both** states.

**Pinned indicator properties:**

- ``qproperty-pinnedIndicatorColor`` --- indicator color (default: text color at 20% opacity)
- ``qproperty-pinnedIndicatorWidth`` --- border width in points (default: ``6``)

.. code-block:: css

    /* Solid red pinned indicator, wider border */
    ItemPinned {
        qproperty-pinnedIndicatorColor: rgba(255, 0, 0, 255);
        qproperty-pinnedIndicatorWidth: 10;
    }

    /* Different indicator color when selected (both rules required) */
    ItemPinned { qproperty-pinnedIndicatorColor: rgba(0, 0, 255, 128); }
    ItemPinned[CopyQ_selected="true"] {
        qproperty-pinnedIndicatorColor: rgba(255, 204, 0, 255);
    }

**Notes indicator properties:**

- ``qproperty-notesIndicatorColor`` --- indicator color (default: text color at 31% opacity)
- ``qproperty-notesIndicatorWidth`` --- bar indent/width in pixels (default: ``16``)

.. code-block:: css

    /* Custom notes indicator color */
    ItemNotes { qproperty-notesIndicatorColor: rgba(100, 200, 100, 180); }

    /* Wider notes bar */
    ItemNotes { qproperty-notesIndicatorWidth: 24; }
