.. _command-examples:

Command Examples
================

Here are some useful commands for creating custom menu items, global
shortcuts and automatically process new clipboard content in CopyQ.

If you want to use any of the commands below, copy it to clipboard and
paste it to the command list in Command dialog (opened with F6
shortcut). For detailed info see :ref:`faq-share-commands`.

All these and more commands are available at
`CopyQ command repository <https://github.com/hluk/copyq-commands>`__.

Join Selected Items
~~~~~~~~~~~~~~~~~~~

Creates new item containing concatenated text of selected items.

.. code-block:: ini

    [Command]
    Name=Join Selected Items
    Command=copyq add -- %1
    InMenu=true
    Icon=\xf066
    Shortcut=Space

Paste Current Date and Time
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copies current date/time text to clipboard and pastes to current window
on global shortcut Win+Alt+T.

.. code-block:: ini

    [Command]
    Command="
        copyq:
        var time = dateString('yyyy-MM-dd hh:mm:ss')
        copy('Current date/time is ' + time)
        paste()"
    GlobalShortcut=meta+alt+t
    Icon=\xf017
    Name=Paste Current Time

Play Sound when Copying to Clipboard
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Following command will play an audio file whenever something is copied
clipboard.

On Windows:

.. code-block:: ini

    [Command]
    Name=Play Sound on Copy
    Command="
         powershell:
         (New-Object Media.SoundPlayer \"C:\\Users\\copy.wav\").PlaySync()"
    Automatic=true
    Icon=\xf028

On Linux (requires VLC multimedia player):

.. code-block:: ini

    [Command]
    Name=Play Sound on Copy
    Command="
         bash:
         cvlc --play-and-exit ~/audio/example.mp3"
    Automatic=true
    Icon=\xf028

Edit and Paste
~~~~~~~~~~~~~~

Following command allows to edit current clipboard text before pasting
it. If the editing is canceled the text won't be pasted.

.. code-block:: ini

    [Command]
    Command="
        copyq:
        var text = dialog('paste', str(clipboard()))
        if (text) {
          copy(text)
          copySelection(text)
          paste()
        }"
    GlobalShortcut=ctrl+shift+v
    Icon=\xf0ea
    Name=Edit and Paste

Remove Background and Text Colors
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Removes background and text colors from rich text (e.g. text copied from
web pages).

Command can be both automatically applied on text copied to clipboard
and invoked from menu (or using custom shortcut).

.. code-block:: ini

    [Command]
    Automatic=true
    Command="
        copyq:
        var html = str(input())
        html = html.replace(/color\\s*:/g, 'xxx:')
        setData('text/html', html)"
    Icon=\xf042
    InMenu=true
    Input=text/html
    Name=Remove Background and Text Colors

Linkify
~~~~~~~

Stores an item with interactive link from plain text URL copied to clipboard.

.. code-block:: ini

    [Command]
    Automatic=true
    Command="
        copyq:
        const link = str(input());
        const href = `<a href=\"${link}\">${link}</a>`;
        setData('text/html', href);"
    Icon=\xf127
    Input=text/plain
    Match=^(https?|ftps?|file|mailto)://
    Name=Linkify

Highlight Text
~~~~~~~~~~~~~~

Highlight all occurrences of a text (change ``x = "text"`` to match
something else than ``text``).

.. code-block:: ini

    [Command]
    Name=Highlight Text
    Command="
        copyq:
        x = 'text'
        style = 'background: yellow; text-decoration: underline'
        
        text = str(input())
        x = x.toLowerCase()
        lowertext = text.toLowerCase()
        html = ''
        a = 0
        esc = function(a, b) {
            return escapeHTML( text.substr(a, b - a) )
        }
        
        while (1) {
            b = lowertext.indexOf(x, a)
            if (b != -1) {
                html += esc(a, b) + '<span>' + esc(b, b + x.length) + '</span>'
            } else {
                html += esc(a, text.length)
                break
            }
            a = b + x.length;
        }
        
        tab( selectedtab() )
        write(
            index(),
            'text/plain', text,
            'text/html',
                '<html><head><style>span{'
                + style +
                '}</style></head><body>'
                + html +
                '</body></html>'
        )"
    Input=text/plain
    Wait=true
    InMenu=true

Render HTML
~~~~~~~~~~~

Render HTML code.

.. code-block:: ini

    [Command]
    Name=Render HTML
    Match=^\\s*<(!|html)
    Command="
        copyq:
        tab(selectedtab())
        write(index() + 1, 'text/html', input())"
    Input=text/plain
    InMenu=true

Translate to English
~~~~~~~~~~~~~~~~~~~~

Pass to text to `Google Translate <https://translate.google.com/>`__.

.. code-block:: ini

    [Command]
    Name=Translate to English
    Command="
        copyq:
        text = str(input())
        url = \"https://translate.google.com/#auto/en/???\"
        
        x = url.replace(\"???\", encodeURIComponent(text))
        html = '<html><head><meta http-equiv=\"refresh\" content=\"0;url=' + x + '\" /></head></html>'
        
        tab(selectedtab())
        write(index() + 1, \"text/html\", html)"
    Input=text/plain
    InMenu=true

Paste and Forget
~~~~~~~~~~~~~~~~

Paste selected items and clear clipboard.

.. code-block:: ini

    [Command]
    Name=Paste and Forget
    Command="
        copyq:
        tab(selectedtab())
        items = selecteditems()
        if (items.length > 1) {
            text = ''
            for (i in items)
                text += read(items[i]);
            copy(text)
        } else {
            select(items[0])
        }
        
        hide()
        paste()
        copy('')"
    InMenu=true
    Icon=\xf0ea
    Shortcut=Ctrl+Return

Render Math Equations
~~~~~~~~~~~~~~~~~~~~~

Render math equations using `MathJax <http://www.mathjax.org/>`__ (e.g.
``$$x = {-b \pm \sqrt{b^2-4ac} \over 2a}$$``).

.. code-block:: ini

    [Command]
    Name=Render Math Equations
    Command="
        copyq:
        text = str(input())
        js = 'http://cdn.mathjax.org/mathjax/latest/MathJax.js?config=TeX-AMS-MML_HTMLorMML'
        
        html = '<html><head><script type=\"text/javascript\" src=\"' + js + '\"></script></head><body>' + escapeHTML(text) + '</body></html>';
        
        tab(selectedtab())
        write(index() + 1, 'text/html', html)"
    Input=text/plain
    InMenu=true
    Icon=\xf12b

Move Images to Other Tab
~~~~~~~~~~~~~~~~~~~~~~~~

With this command active, images won't be saved in the first tab. This
can make application a bit more snappier since big image data won't need
to be loaded when main window is displayed or clipboard is stored for
the first time.

.. code-block:: ini

    [Command]
    Name=Move Images to Other Tab
    Input=image/png
    Automatic=true
    Remove=true
    Icon=\xf03e
    Tab=&Images

Copy Clipboard to Window Tabs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Following command automatically adds new clipboard to tab with same name
as title of the window where copy operation was performed.

.. code-block:: ini

    [Command]
    Name=Window Tabs
    Command="copyq:
        item = unpack(input())
        window_title = item[\"application/x-copyq-owner-window-title\"]
        if (window_title) {
            // Remove the part of window title before dash
            // (it's usually document name or URL).
            tabname = str(window_title).replace(/.* (-|\x2013) /, \"\")
            tab(\"Windows/\" + tabname)
            write(\"application/x-copyq-item\", input())
        }
        "
    Input=application/x-copyq-item
    Automatic=true
    Icon=\xf009

Quickly Show Current Clipboard Content
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Quickly pop up notification with text in clipboard using ``Win+Alt+C``
system shortcut.

.. code-block:: ini

    [Command]
    Name=Show clipboard
    Command="
        copyq:
        seconds = 2;
        popup(\"\", clipboard(), seconds * 1000)"
    GlobalShortcut=Meta+Alt+C

Replace All Occurrences in Selected Text
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ini

    [Command]
    Name=Replace in Selection
    Command="
        copyq:
        // Copy without changing Linux mouse selection (on Windows you can use "copy" instead).
        function copy2() {
          try {
            var x = config('copy_clipboard')
            config('copy_clipboard', false)
            try {
              copy.apply(this, arguments)
            } finally {
              config('copy_clipboard', x)
            }
          } catch(e) {
            copy.apply(this, arguments)
          }
        }
       
        copy2()
        var text = str(clipboard())
       
        if (text) {
          var r1 = 'Text'
          var r2 = 'Replace with'
          var reply = dialog(r1, '', r2, '')
       
          if (reply) {
            copy2(text.replace(new RegExp(reply[r1], 'g'), reply[r2]))
            paste()
          }
        }"
    Icon=\xf040
    GlobalShortcut=Meta+Alt+R

Copy Nth Item
~~~~~~~~~~~~~

Copy item in row depending on which shortcut was pressed. E.g. Ctrl+2
for item in row "2".

.. code-block:: ini

    [Command]
    Name=Copy Nth Item
    Command="
        copyq:
        var shortcut = str(data(\"application/x-copyq-shortcut\"));
        var row = shortcut ? shortcut.replace(/^\\D+/g, '') : currentItem();
        var itemIndex = (config('row_index_from_one') == 'true') ? row - 1 : row;
        selectItems(itemIndex);
        copy(\"application/x-copyq-item\", pack(getItem(itemIndex)));"
    InMenu=true
    Icon=\xf0cb
    Shortcut=ctrl+1, ctrl+2, ctrl+3, ctrl+4, ctrl+5, ctrl+6, ctrl+7, ctrl+8, ctrl+9, ctrl+0

Edit Files
~~~~~~~~~~

Opens files referenced by selected item in external editor (uses
"External editor command" from "History" config tab).

Works with following path formats (some editors may not support all of
these).

-  ``C:/...``
-  ``file://...``
-  ``~...`` (some shells)
-  ``%...%...`` (Windows environment variables)
-  ``$...`` (environment variables)
-  ``/c/...`` (gitbash)

.. code-block:: ini

    [Command]
    Name=Edit Files
    Match=^([a-zA-Z]:[\\\\/]|~|file://|%\\w+%|$\\w+|/)
    Command="
        copyq:
        var editor = config('editor')
            .replace(/ %1/, '')

        var filePaths = str(input())
            .replace(/^file:\\/{2}/gm, '')
            .replace(/^\\/(\\w):?\\//gm, '$1:/')
            .split('\\n')

        var args = [editor].concat(filePaths)

        execute.apply(this, args)"
    Input=text/plain
    InMenu=true
    Icon=\xf040
    Shortcut=f4

Change Monitoring State Permanently
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Disables clipboard monitoring permanently, i.e. the state is restored
when clipboard changes even after application is restarted.

Should be the first automatic command in the list of commands so other
commands are not invoked.

.. code-block:: ini

    [Command]
    Automatic=true
    Command="
        copyq:
        var option = 'disable_monitoring'
        var disabled = str(settings(option)) === 'true'
        
        if (str(data('application/x-copyq-shortcut'))) {
          disabled = !disabled
          settings(option, disabled)
          popup('', disabled ? 'Monitoring disabled' : 'Monitoring enabled')
        }
        
        if (disabled) {
          disable()
          ignore()
        } else {
          enable()
        }"
    GlobalShortcut=meta+alt+x
    Icon=\xf05e
    Name=Toggle Monitoring

Show Window Title
~~~~~~~~~~~~~~~~~

Shows source application window title for new items in tag ("Tags"
plugin must be enabled in "Items" config tab).

.. code-block:: ini

    [Command]
    Automatic=true
    Command="
        copyq:
        var window = str(data('application/x-copyq-owner-window-title'))
        var tagsMime = 'application/x-copyq-tags'
        var tags = str(data(tagsMime)) + ', ' + window
        setData(tagsMime, tags)"
    Icon=\xf009
    Name=Store Window Title

Show Copy Time
~~~~~~~~~~~~~~

Shows copy time of new items in tag ("Tags" plugin must be enabled in
"Items" config tab).

.. code-block:: ini

    [Command]
    Automatic=true
    Command="
        copyq:
        var time = dateString('yyyy-MM-dd hh:mm:ss')
        setData('application/x-copyq-user-copy-time', time)
        
        var tagsMime = 'application/x-copyq-tags'
        var tags = str(data(tagsMime)) + ', ' + time
        setData(tagsMime, tags)"
    Icon=\xf017
    Name=Store Copy Time

Mark Selected Items
~~~~~~~~~~~~~~~~~~~

Toggles highlighting of selected items.

.. code-block:: ini

    [Command]
    Command="
        copyq:
        var color = 'rgba(255, 255, 0, 0.5)'
        var mime = 'application/x-copyq-color'
        
        var firstSelectedItem = selectedItems()[0]
        var currentColor = str(read(mime, firstSelectedItem))
        if (currentColor != color)
          setData(mime, color)
        else
          removeData(mime)"
    Icon=\xf1fc
    InMenu=true
    Name=Mark/Unmark Items
    Shortcut=ctrl+m

Change Upper/Lower Case of Selected Text
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ini

    [Command]
    Command="
        copyq:
        if (!copy())
          abort()
        
        var text = str(clipboard())
        
        var newText = text.toUpperCase()
        if (text == newText)
          newText = text.toLowerCase()
        
        if (text == newText)
          abort();
        
        copy(newText)
        paste()"
    GlobalShortcut=meta+ctrl+u
    Icon=\xf034
    Name=Toggle Upper/Lower Case



Change Copied Text to Title Case
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: ini

    [Command]
    Name=Paste as title case
    Command="
        copyq: 
        function toTitleCase(str) {
          return str.replace(
            /\\w\\S*/g,
            function(txt) {
              return txt.charAt(0).toUpperCase() + txt.substr(1).toLowerCase();
            }
          );
        }
        copy(toTitleCase(str(input())))
               paste()
        "
    Input=text/plain
    IsGlobalShortcut=true
    HideWindow=true
    Icon=\xf15b
    GlobalShortcut=meta+ctrl+t

