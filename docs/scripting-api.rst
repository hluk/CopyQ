.. _scripting-api:

Scripting API
=============

CopyQ provides scripting capabilities to automatically handle clipboard
changes, organize items, change settings and much more.

Supported language features and base function can be found at `ECMAScript
Reference <http://doc.qt.io/qt-5/ecmascript.html>`__. The language is mostly
equivalent to modern JavaScript. Some features may be missing but feel free to
use for example `JavaScript reference on MDN
<https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/>`__.

CopyQ-specific features described in this document:

- `Functions`_
- `Types`_
- `Objects`_
- `MIME types`_
- `Plugins`_

.. note::

    These terms are equivalent: format, MIME type, media type

Execute Script
--------------

The scripts can be executed from:

a.  Action or Command dialogs (F5, F6 shortcuts), if
    the first line starts with ``copyq:``
b.  command line as ``copyq eval '<SCRIPT>'``
c.  command line as ``cat script.js | copyq eval -``
d.  command line as
    ``copyq <SCRIPT_FUNCTION> <FUNCTION_ARGUMENT_1> <FUNCTION_ARGUMENT_2> ...``

When run from command line, result of last expression is printed on
stdout.

Command exit values are:

-  0 - script finished without error
-  1 - :js:func:`fail` was called
-  2 - bad syntax
-  3 - exception was thrown

Command Line
------------

If number of arguments that can be passed to function is limited you can
use

.. code-block:: bash

    copyq <FUNCTION1> <FUNCTION1_ARGUMENT_1> <FUNCTION1_ARGUMENT_2> \
              <FUNCTION2> <FUNCTION2_ARGUMENT> \
                  <FUNCTION3> <FUNCTION3_ARGUMENTS> ...

where ``<FUNCTION1>`` and ``<FUNCTION2>`` are scripts where result of
last expression is functions that take two and one arguments
respectively.

Example:

.. code-block:: bash

    copyq tab clipboard separator "," read 0 1 2

After :js:func:`eval` no arguments are treated as functions since it can access
all arguments.

Arguments recognize escape sequences ``\n`` (new line), ``\t``
(tabulator character) and ``\\`` (backslash).

Argument ``-e`` is identical to :js:func:`eval`.

Argument ``-`` is replaced with data read from stdin.

Argument ``--`` is skipped and all the remaining arguments are
interpreted as they are (escape sequences are ignored and ``-e``, ``-``,
``--`` are left unchanged).

Functions
---------

Argument list parts ``...`` and ``[...]`` are optional and can be
omitted.

Comment `/*set*/` in function declaration indicates a specific function
overload.

Item **row** values in scripts always **start from 0** (like array index),
unlike in GUI, where row numbers start from 1 by default.

.. js:function:: version()

   Returns version string.

   :returns: Version string.
   :rtype: string

   Example of the version string::

       CopyQ Clipboard Manager v4.0.0-19-g93d95a7f
       Qt: 5.15.2
       KNotifications: 5.79.0
       Compiler: GCC
       Arch: x86_64-little_endian-lp64
       OS: Fedora 33 (Workstation Edition)

.. js:function:: help()

   Returns help string.

   :returns: Help string.
   :rtype: string

.. js:function:: /*search*/ help(searchString, ...)

   Returns help for matched commands.

   :returns: Help string.
   :rtype: string

.. js:function:: show()

   Shows main window.

   This uses the last window position and size which is updated whenever the
   window is moved or resized.

.. js:function:: /*tab*/ show(tabName)

   Shows tab.

   This uses the last window position and size which is updated whenever the
   window is moved or resized.

.. js:function:: showAt(x, y, [width, height])

   Shows main window with given geometry.

   The new window position and size will not be stored for ``show()``.

.. js:function:: /*cursor*/ showAt()

   Shows main window under mouse cursor.

   The new window position will not be stored for ``show()``.

.. js:function:: /*tab*/ showAt(x, y, width, height, tabName)

   Shows tab with given geometry.

   The new window position and size will not be stored for ``show()``.

.. js:function:: hide()

   Hides main window.

.. js:function:: toggle()

   Shows or hides main window.

   This uses the last window position and size which is updated whenever the
   window is moved or resized.

   :returns: ``true`` only if main window is being shown, otherwise ``false``.
   :rtype: bool

.. js:function:: menu()

   Opens context menu.

.. js:function:: /*tab*/ menu(tabName, [maxItemCount, [x, y]])

   Shows context menu for given tab.

   This menu doesn't show clipboard and doesn't have any special actions.

   Second argument is optional maximum number of items. The default value
   same as for tray (i.e. value of ``config('tray_items')``).

   Optional arguments x, y are coordinates in pixels on screen where menu
   should show up. By default menu shows up under the mouse cursor.

.. js:function:: exit()

   Exits server.

.. js:function:: disable()
                 enable()

   Disables or enables clipboard content storing.

.. js:function:: monitoring()

   Returns true only if clipboard storing is enabled.

   :returns: ``true`` if clipboard storing is enabled, otherwise ``false``.
   :rtype: bool

.. js:function:: visible()

   Returns true only if main window is visible.

   :returns: ``true`` if main window is visible, otherwise ``false``.
   :rtype: bool

.. js:function:: focused()

   Returns true only if main window has focus.

   :returns: ``true`` if main window has focus, otherwise ``false``.
   :rtype: bool

.. js:function:: focusPrevious()

   Activates window that was focused before the main window.

   :throws Error: Thrown if previous window cannot be activated.

.. js:function:: preview([true|false])

   Shows/hides item preview and returns true only if preview was visible.

   Example -- toggle the preview:

   .. code-block:: js

       preview(false) || preview(true)

.. js:function:: filter()

   Returns the current text for filtering items in main window.

   :returns: Current filter.
   :rtype: string

.. js:function:: /*set*/ filter(filterText)

   Sets text for filtering items in main window.

.. js:function:: ignore()

   Ignores current clipboard content (used for automatic commands).

   This does all of the below.

   -  Skips any next automatic commands.
   -  Omits changing window title and tray tool tip.
   -  Won't store content in clipboard tab.

.. js:function:: clipboard([mimeType])

   Returns clipboard data for MIME type (default is text).

   Pass argument ``"?"`` to list available MIME types.

   :returns: Clipboard data.
   :rtype: :js:class:`ByteArray`

.. js:function:: selection([mimeType])

   Same as :js:func:`clipboard` for `Linux mouse selection`_.

   :returns: Selection data.
   :rtype: :js:class:`ByteArray`

.. js:function:: hasClipboardFormat(mimeType)

   Returns true only if clipboard contains MIME type.

   :returns: ``true`` if clipboard contains the format, otherwise ``false``.
   :rtype: bool

.. js:function:: hasSelectionFormat(mimeType)

   Same as :js:func:`hasClipboardFormat` for `Linux mouse selection`_.

   :returns: ``true`` if selection contains the format, otherwise ``false``.
   :rtype: bool

.. js:function:: isClipboard()

   Returns true only in automatic command triggered by clipboard change.

   This can be used to check if current automatic command was triggered by
   clipboard and not `Linux mouse selection`_ change.

   :returns: ``true`` if current automatic command is triggered by clipboard
             change, otherwise ``false``.
   :rtype: bool

.. js:function:: copy(text)

   Sets clipboard plain text.

   Same as ``copy(mimeText, text)``.

   :throws Error: Thrown if clipboard fails to be set.

.. js:function:: /*data*/ copy(mimeType, data, [mimeType, data]...)

   Sets clipboard data.

   This also sets :js:data:`mimeOwner` format so automatic commands are not run
   on the new data and it's not stored in clipboard tab.

   All other data formats are dropped from clipboard.

   :throws Error: Thrown if clipboard fails to be set.

   Example -- set both text and rich text:

   .. code-block:: js

       copy(mimeText, 'Hello, World!',
            mimeHtml, '<p>Hello, World!</p>')

.. js:function:: /*item*/ copy(Item)

   Function override with an item argument.

   :throws Error: Thrown if clipboard fails to be set.

   Example -- set both text and rich text:

   .. code-block:: js

       var item = {}
       item[mimeText] = 'Hello, World!'
       item[mimeHtml] = '<p>Hello, World!</p>'
       copy(item)

.. js:function:: /*window*/ copy()

   Sends ``Ctrl+C`` to current window.

   :throws Error: Thrown if clipboard doesn't change (clipboard is reset before
                  sending the shortcut).

   Example:

   .. code-block:: js

       try {
           copy(arguments)
       } catch (e) {
           // Coping failed!
           popup('Coping Failed', e)
           abort()
       }
       var text = str(clipboard())
       popup('Copied Text', text)

.. js:function:: copySelection(...)

   Same as :js:func:`copy` for `Linux mouse selection`_.

   There is no ``copySelection()`` without parameters.

   :throws Error: Thrown if selection fails to be set.

.. js:function:: paste()

   Pastes current clipboard.

   This is basically only sending ``Shift+Insert`` shortcut to current
   window.

   Correct functionality depends a lot on target application and window
   manager.

   :throws Error: Thrown if paste operation fails.

   Example:

   .. code-block:: js

       try {
           paste()
       } catch (e) {
           // Pasting failed!
           popup('Pasting Failed', e)
           abort()
       }
       popup('Pasting Successful')

.. js:function:: tab()

   Returns tab names.

   :returns: Array with names of existing tab.
   :rtype: array of strings

.. js:function:: /*set*/ tab(tabName)

   Sets current tab for the script.

   Example -- select third item at index 2 from tab "Notes":

   .. code-block:: js

       tab('Notes')
       select(2)

.. js:function:: removeTab(tabName)

   Removes tab.

.. js:function:: renameTab(tabName, newTabName)

   Renames tab.

.. js:function:: tabIcon(tabName)

   Returns path to icon for tab.

   :returns: Path to icon for tab.
   :rtype: string

.. js:function:: /*set*/ tabIcon(tabName, iconPath)

   Sets icon for tab.

.. js:function:: unload([tabNames...])

   Unload tabs (i.e. items from memory).

   If no tabs are specified, unloads all tabs.

   If a tab is open and visible or has an editor open, it won't be unloaded.

   :returns: Array of successfully unloaded tabs.
   :rtype: array of strings

.. js:function:: forceUnload([tabNames...])

   Force-unload tabs (i.e. items from memory).

   If no tabs are specified, unloads all tabs.

   Refresh button needs to be clicked to show the content of a force-unloaded
   tab.

   If a tab has an editor open, the editor will be closed first even if it has
   unsaved changes.

.. js:function:: count()
                 length()
                 size()

   Returns amount of items in current tab.

   :returns: Item count.
   :rtype: int

.. js:function:: select(row)

   Copies item in the row to clipboard.

   Additionally, moves selected item to top depending on settings.

.. js:function:: next()

   Copies next item from current tab to clipboard.

.. js:function:: previous()

   Copies previous item from current tab to clipboard.

.. js:function:: add(text|Item...)

   Same as ``insert(0, ...)``.

.. js:function:: insert(row, text|Item...)

   Inserts new items to current tab.

   :throws Error: Thrown if space for the items cannot be allocated.

.. js:function:: remove(row, ...)

   Removes items in current tab.

   :throws Error: Thrown if some items cannot be removed.

.. js:function:: move(row)

    Moves selected items to given row in same tab.

.. js:function:: edit([row|text] ...)

   Edits items in the current tab.

   Opens external editor if set, otherwise opens internal editor.

   If row is -1 (or other negative number) edits clipboard instead
   and creates new item.

.. js:function:: editItem(row, [mimeType, [data]])

   Edits specific format for the item.

   Opens external editor if set, otherwise opens internal editor.

   If row is -1 (or other negative number) edits clipboard instead
   and creates new item.

.. js:function:: read([mimeType])

   Same as :js:func:`clipboard`.

.. js:function:: /*row*/ read(mimeType, row, ...)

   Returns concatenated data from items, or clipboard if row is negative.

   Pass argument ``"?"`` to list available MIME types.

   :returns: Concatenated data in the rows.
   :rtype: :js:class:`ByteArray`

.. js:function:: write(row, mimeType, data, [mimeType, data]...)

   Inserts new item to current tab.

   :throws Error: Thrown if space for the items cannot be allocated.

.. js:function:: /*item*/ write(row, Item...)

   Function override with one or more item arguments.

.. js:function:: /*items*/ write(row, Item[])

   Function override with item list argument.

.. js:function:: change(row, mimeType, data, [mimeType, data]...)

   Changes data in item in current tab.

   If data is ``undefined`` the format is removed from item.

.. js:function:: /*item*/ change(row, Item...)

   Function override with one or more item arguments.

.. js:function:: /*items*/ change(row, Item[])

   Function override with item list argument.

.. js:function:: separator()

   Returns item separator (used when concatenating item data).

   :returns: Current separator.
   :rtype: string

.. js:function:: /*set*/ separator(separator)

   Sets item separator for concatenating item data.

.. js:function:: action()

   Opens action dialog.

.. js:function:: /*row*/ action([rows, ...], command, [outputItemSeparator])

   Runs command for items in current tab.

   If rows arguments is specified, ``%1`` in the command will be replaced with
   concatenated text of the rows.

   If no rows are specified, ``%1`` in the command will be replaced with
   clipboard text.

   The concatenated text (if rows are defined) or clipboard text is also passed
   on standard input of the command.

.. js:function:: popup(title, message, [time=8000])

   Shows popup message for given time in milliseconds.

   If ``time`` argument is set to -1, the popup is hidden only after mouse
   click.

.. js:function:: notification(...)

   Shows popup message with icon and buttons.

   Each button can have script and data.

   If button is clicked the notification is hidden and script is executed
   with the data passed as stdin.

   The function returns immediately (doesn't wait on user input).

   Special arguments:

   -  '.title' - notification title
   -  '.message' - notification message (can contain basic HTML)
   -  '.icon' - notification icon (path to image or font icon)
   -  '.id' - notification ID - this replaces notification with same ID
   -  '.time' - duration of notification in milliseconds (default is -1,
      i.e. waits for mouse click)
   -  '.button' - adds button (three arguments: name, script and data)

   Example:

   .. code-block:: js

       notification(
             '.title', 'Example',
             '.message', 'Notification with button',
             '.button', 'Cancel', '', '',
             '.button', 'OK', 'copyq:popup(input())', 'OK Clicked'
             )

.. js:function:: exportTab(fileName)

   Exports current tab into file.

   :throws Error: Thrown if export fails.

.. js:function:: importTab(fileName)

   Imports items from file to a new tab.

   :throws Error: Thrown if import fails.

.. js:function:: exportData(fileName)

   Exports all tabs and configuration into file.

   :throws Error: Thrown if export fails.

.. js:function:: importData(fileName)

   Imports all tabs and configuration from file.

   :throws Error: Thrown if import fails.

.. js:function:: config()

   Returns help with list of available application options.

   Users can change most of these options via the CopyQ GUI, mainly via
   the "Preferences" window.

   These options are persisted within the ``[Options]`` section of a corresponding
   ``copyq.ini`` or ``copyq.conf`` file (``copyq.ini`` is used on Windows).

   :returns: Available options.
   :rtype: string

.. js:function:: /*get*/ config(optionName)

   Returns value of given application option.

   :returns: Current value of the option.
   :rtype: string
   :throws Error: Thrown if the option is invalid.

.. js:function:: /*set*/ config(optionName, value)

   Sets application option and returns new value.

   :returns: New value of the option.
   :rtype: string
   :throws Error: Thrown if the option is invalid.

.. js:function:: /*set-more*/ config(optionName, value, ...)

   Sets multiple application options and return list with values in format
   ``optionName=newValue``.

   :returns: New values of the options.
   :rtype: string
   :throws Error: Thrown if there is an invalid option in which case it won't set
                  any options.

.. js:function:: toggleConfig(optionName)

   Toggles an option (true to false and vice versa) and returns the new value.

   :returns: New value of the option.
   :rtype: bool

.. js:function:: info([pathName])

   Returns paths and flags used by the application.

   :returns: Path for given identifier.
   :rtype: string

   Example -- print path to the configuration file:

   .. code-block:: js

       info('config')

.. js:function:: eval(script)

   Evaluates script and returns result.

   :returns: Result of the last expression.

.. js:function:: source(fileName)

   Evaluates script file and returns result of last expression in the script.

   This is useful to move some common code out of commands.

   :returns: Result of the last expression.

   .. code-block:: js

       // File: c:/copyq/replace_clipboard_text.js
       replaceClipboardText = function(replaceWhat, replaceWith)
       {
           var text = str(clipboard())
           var newText = text.replace(replaceWhat, replaceWith)
           if (text != newText)
               copy(newText)
       }

   .. code-block:: js

       source('c:/copyq/replace_clipboard_text.js')
       replaceClipboardText('secret', '*****')

.. js:function:: currentPath()

   Get current path.

   :returns: Current path.
   :rtype: string

   .. code-block:: bash

       cd /tmp
       copyq currentPath
       # Prints: /tmp

.. js:function:: /*set*/ currentPath(path)

   Set current path.

.. js:function:: str(value)

   Converts a value to string.

   If ByteArray object is the argument, it assumes UTF8 encoding. To use
   different encoding, use :js:func`toUnicode`.

   :returns: Value as string.
   :rtype: string

.. js:function:: input()

   Returns standard input passed to the script.

   :returns: Data on stdin.
   :rtype: :js:class:`ByteArray`

.. js:function:: toUnicode(ByteArray)

   Returns string for bytes with encoding detected by checking Byte Order Mark (BOM).

   :returns: Value as string.
   :rtype: string

.. js:function:: /*encoding*/ toUnicode(ByteArray, encodingName)

   Returns string for bytes with given encoding.

   :returns: Value as string.
   :rtype: string

.. js:function:: fromUnicode(String, encodingName)

   Returns encoded text.

   :returns: Value as ByteArray.
   :rtype: :js:class:`ByteArray`

.. js:function:: data(mimeType)

   Returns data for automatic commands or selected items.

   If run from menu or using non-global shortcut the data are taken from
   selected items.

   If run for automatic command the data are clipboard content.

   :returns: Data for the format.
   :rtype: :js:class:`ByteArray`

.. js:function:: setData(mimeType, data)

   Modifies data for :js:func:`data` and new clipboard item.

   Next automatic command will get updated data.

   This is also the data used to create new item from clipboard.

   :returns: ``true`` if data were set, ``false`` if parsing data failed (in
             case of :js:data:`mimeItems`).
   :rtype: bool

   Example -- automatic command that adds a creation time data and tag to new
   items:

   ::

       copyq:
       var timeFormat = 'yyyy-MM-dd hh:mm:ss'
       setData('application/x-copyq-user-copy-time', dateString(timeFormat))
       setData(mimeTags, 'copied: ' + time)

   Example -- menu command that adds a tag to selected items:

   ::

       copyq:
       setData('application/x-copyq-tags', 'Important')

.. js:function:: removeData(mimeType)

   Removes data for :js:func:`data` and new clipboard item.

.. js:function:: dataFormats()

   Returns formats available for :js:func:`data`.

   :returns: Array of data formats.
   :rtype: array of strings

.. js:function:: print(value)

   Prints value to standard output.

.. js:function:: serverLog(value)

   Prints value to application log.

.. js:function:: logs()

   Returns application logs.

   :returns: Application logs.
   :rtype: string

.. js:function:: abort()

   Aborts script evaluation.

.. js:function:: fail()

   Aborts script evaluation with nonzero exit code.

.. js:function:: setCurrentTab(tabName)

   Focus tab without showing main window.

.. js:function:: selectItems(row, ...)

   Selects items in current tab.

.. js:function:: selectedTab()

   Returns tab that was selected when script was executed.

   :returns: Currently selected tab name (see `Selected Items`_).
   :rtype: string

.. js:function:: selectedItems()

   Returns selected rows in current tab.

   :returns: Currently selected rows (see `Selected Items`_).
   :rtype: array of ints

.. js:function:: selectedItemData(index)

   Returns data for given selected item.

   The data can empty if the item was removed during execution of the
   script.

   :returns: Currently selected items (see `Selected Items`_).
   :rtype: array of :js:class:`Item`

.. js:function:: setSelectedItemData(index, Item)

   Set data for given selected item.

   Returns false only if the data cannot be set, usually if item was
   removed.

   See `Selected Items`_.

   :returns: ``true`` if data were set, otherwise ``false``.
   :rtype: bool

.. js:function:: selectedItemsData()

   Returns data for all selected items.

   Some data can be empty if the item was removed during execution of the
   script.

   :returns: Currently selected item data (see `Selected Items`_).
   :rtype: array of :js:class:`Item`

.. js:function:: setSelectedItemsData(Item[])

   Set data to all selected items.

   Some data may not be set if the item was removed during execution of the
   script.

   See `Selected Items`_.

.. js:function:: currentItem()
                 index()

   Returns current row in current tab.

   See `Selected Items`_.

   :returns: Current row (see `Selected Items`_).
   :rtype: int

.. js:function:: escapeHtml(text)

   Returns text with special HTML characters escaped.

   :returns: Escaped HTML text.
   :rtype: string

.. js:function:: unpack(data)

   Returns deserialized object from serialized items.

   :returns: Deserialize item.
   :rtype: :js:class:`Item`

.. js:function:: pack(Item)

   Returns serialized item.

   :returns: Serialize item.
   :rtype: :js:class:`ByteArray`

.. js:function:: getItem(row)

   Returns an item in current tab.

   :returns: Item data for the row.
   :rtype: :js:class:`Item`

   Example -- show data of the first item in a tab in popups:

   .. code-block:: js

       tab('work')  // change current tab for the script to 'work'
       var item = getItem(0)
       for (var format in item) {
           var data = item[format]
           popup(format, data)
       }

   .. seealso::

      - :js:func:`selectedItemsData`

.. js:function:: setItem(row, text|Item)

   Inserts item to current tab.

   Same as ``insert(row, something)``.

   .. seealso::

      - :js:func:`insert`
      - :js:func:`setSelectedItemsData`

.. js:function:: toBase64(data)

   Returns base64-encoded data.

   :returns: Base64-encoded data.
   :rtype: string

.. js:function:: fromBase64(base64String)

   Returns base64-decoded data.

   :returns: Base64-decoded data.
   :rtype: :js:class:`ByteArray`

.. js:function:: md5sum(data)

   Returns MD5 checksum of data.

   :returns: MD5 checksum of the data.
   :rtype: :js:class:`ByteArray`

.. js:function:: sha1sum(data)

   Returns SHA1 checksum of data.

   :returns: SHA1 checksum of the data.
   :rtype: :js:class:`ByteArray`

.. js:function:: sha256sum(data)

   Returns SHA256 checksum of data.

   :returns: SHA256 checksum of the data.
   :rtype: :js:class:`ByteArray`

.. js:function:: sha512sum(data)

   Returns SHA512 checksum of data.

   :returns: SHA512 checksum of the data.
   :rtype: :js:class:`ByteArray`

.. js:function:: open(url, ...)

   Tries to open URLs in appropriate applications.

   :returns: ``true`` if all URLs were successfully opened, otherwise ``false``.
   :rtype: bool

.. js:function:: execute(argument, ..., null, stdinData, ...)

   Executes a command.

   All arguments after ``null`` are passed to standard input of the
   command.

   If argument is function it will be called with array of lines read from
   stdout whenever available.

   An exception is thrown if executable was not found or could not be executed.

   :returns: Finished command properties.
   :rtype: :js:class:`FinishedCommand`

   Example -- create item for each line on stdout:

   .. code-block:: js

       execute('tail', '-f', 'some_file.log',
               function(lines) { add.apply(this, lines) })

   Returns object for the finished command or ``undefined`` on failure.

.. js:function:: String currentWindowTitle()

   Returns window title of currently focused window.

   :returns: Current window title.
   :rtype: string

.. js:function:: String currentClipboardOwner()

   Returns name of the current clipboard owner.

   The default implementation returns `currentWindowTitle()`.

   This is used to set `mimeWindowTitle` format for the clipboard data in
   automatic commands and filtering by window title.

   Depending on the current system, option `update_clipboard_owner_delay_ms`
   can introduce a delay before any new owner value return by this function is
   used. The reason is to avoid using an incorrect clipboard owner from the
   current window title if the real clipboard owner set the clipboard after or
   just before hiding its window (like with some password managers).

   :returns: Current clipboard owner name.
   :rtype: string

.. js:function:: dialog(...)

   Shows messages or asks user for input.

   Arguments are names and associated values.

   Special arguments:

   -  '.title' - dialog title
   -  '.icon' - dialog icon (see below for more info)
   -  '.style' - Qt style sheet for dialog
   -  '.height', '.width', '.x', '.y' - dialog geometry
   -  '.label' - dialog message (can contain basic HTML)
   -  '.modal' - set to true to make the dialog modal (to avoid other windows to get input focus)
   -  '.onTop' - set to true for the dialog to stay above other windows
   -  '.noParent' - set to true to avoid attaching the dialog to the main window
   -  '.popupWindow', '.sheetWindow', '.toolWindow', '.foreignWindow' - set/unset the type of dialog window (see `Qt::WindowFlags <https://doc.qt.io/qt-6/qt.html#WindowType-enum>`__ for details)

   :returns: Value or values from accepted dialog or ``undefined`` if dialog
             was canceled.

   .. code-block:: js

       dialog(
         '.title', 'Command Finished',
         '.label', 'Command <b>successfully</b> finished.'
         )

   Accepting a dialog containing only a question returns ``true``
   (rejecting/cancelling the dialog returns ``undefined``).

   .. code-block:: js

       const remove = dialog(
         '.title', 'Remove Items',
         '.label', 'Do you really want to remove all items?'
         )
       if (!remove)
           abort();

   Other arguments are used to get user input.

   .. code-block:: js

       var amount = dialog('.title', 'Amount?', 'Enter Amount', 'n/a')
       var filePath = dialog('.title', 'File?', 'Choose File', new File('/home'))

   If multiple inputs are required, object is returned.

   .. code-block:: js

       var result = dialog(
         'Enter Amount', 'n/a',
         'Choose File', new File(str(currentPath))
         )
       print('Amount: ' + result['Enter Amount'] + '\n')
       print('File: ' + result['Choose File'] + '\n')

   A combo box with an editable custom text/value can be created by passing an
   array argument. The default text can be provided using ``.defaultChoice``
   (by default it's the first item).

   .. code-block:: js

       var text = dialog('.defaultChoice', '', 'Select', ['a', 'b', 'c'])

   A combo box with non-editable text can be created by prefixing the label
   argument with ``.combo:``.

   .. code-block:: js

       var text = dialog('.combo:Select', ['a', 'b', 'c'])

   An item list can be created by prefixing the label argument with ``.list:``.

   .. code-block:: js

       var items = ['a', 'b', 'c']
       var selected_index = dialog('.list:Select', items)
       if (selected_index !== undefined)
           print('Selected item: ' + items[selected_index])

   Icon for custom dialog can be set from icon font, file path or theme.
   Icons from icon font can be copied from icon selection dialog in Command
   dialog or dialog for setting tab icon (in menu 'Tabs/Change Tab Icon').

   .. code-block:: js

       var search = dialog(
         '.title', 'Search',
         '.icon', 'search', // Set icon 'search' from theme.
         'Search', ''
         )

.. js:function:: menuItems(text...)

   Opens menu with given items and returns selected item or an empty string.

   :returns: Selected item or empty string if menu was canceled.
   :rtype: string

   .. code-block:: js

       var selectedText = menuItems('x', 'y', 'z')
       if (selectedText)
           popup('Selected', selectedText)

.. js:function:: /*items*/ menuItems(items[])

   Opens menu with given items and returns index of selected item or -1.

   Menu item label is taken from :js:data:`mimeText` format an icon is taken
   from :js:data:`mimeIcon` format.

   :returns: Selected item index or `-1` if menu was canceled.
   :rtype: int

   .. code-block:: js

       var items = selectedItemsData()
       var selectedIndex = menuItems(items)
       if (selectedIndex != -1)
           popup('Selected', items[selectedIndex][mimeText])

.. js:function:: settings()

   Returns array with names of all custom user options.

   These options can be managed by various commands, much like cookies
   are used by web applications in a browser. A typical usage is to remember
   options lastly selected by user in a custom dialog displayed by a command.

   These options are persisted within the ``[General]`` section of a corresponding
   ``copyq-scripts.ini`` file. But if an option is named like ``group/...``,
   then it is written to a section named ``[group]`` instead.
   By grouping options like this, we can avoid potential naming collisions
   with other commands.

   :returns: Available custom options.
   :rtype: array of strings

.. js:function:: /*get*/ Value settings(optionName)

   Returns value for a custom user option.

   :returns: Current value of the custom options, ``undefined`` if the option
             was not set.

.. js:function:: /*set*/ settings(optionName, value)

   Sets value for a new custom user option or overrides existing one.

.. js:function:: dateString(format)

   Returns text representation of current date and time.

   See `Date QML Type
   <https://doc.qt.io/qt-5/qml-qtqml-date.html#format-strings>`__ for details
   on formatting date and time.

   :returns: Current date and time as string.
   :rtype: string

   Example:

   .. code-block:: js

       var now = dateString('yyyy-MM-dd HH:mm:ss')

.. js:function:: commands()

   Return list of all commands.

   :returns: Array of all commands.
   :rtype: array of :js:class:`Command`

.. js:function:: setCommands(Command[])

   Clear previous commands and set new ones.

   To add new command:

   .. code-block:: js

       var cmds = commands()
       cmds.unshift({
               name: 'New Command',
               automatic: true,
               input: 'text/plain',
               cmd: 'copyq: popup("Clipboard", input())'
               })
       setCommands(cmds)

.. js:function:: Command[] importCommands(String)

   Return list of commands from exported commands text.

   :returns: Array of commands loaded from a file path.
   :rtype: array of :js:class:`Command`

.. js:function:: String exportCommands(Command[])

   Return exported command text.

   :returns: Serialized commands.
   :rtype: string

.. js:function:: addCommands(Command[])

   Opens Command dialog, adds commands and waits for user to confirm the
   dialog.

.. js:function:: NetworkReply networkGet(url)

   Sends HTTP GET request.

   :returns: HTTP reply.
   :rtype: :js:class:`NetworkReply`

.. js:function:: NetworkReply networkPost(url, postData)

   Sends HTTP POST request.

   :returns: HTTP reply.
   :rtype: :js:class:`NetworkReply`

.. js:function:: NetworkReply networkGetAsync(url)

   Same as :js:func:`networkGet` but the request is asynchronous.

   The request is handled asynchronously and may not be finished until you get
   a property of the reply.

   :returns: HTTP reply.
   :rtype: :js:class:`NetworkReply`

.. js:function:: NetworkReply networkPostAsync(url, postData)

   Same as :js:func:`networkPost` but the request is asynchronous.

   The request is handled asynchronously and may not be finished until you get
   a property of the reply.

   :returns: HTTP reply.
   :rtype: :js:class:`NetworkReply`

.. js:function:: env(name)

   Returns value of environment variable with given name.

   :returns: Value of the environment variable.
   :rtype: :js:class:`ByteArray`

.. js:function:: setEnv(name, value)

   Sets environment variable with given name to given value.

   :returns: ``true`` if the variable was set, otherwise ``false``.
   :rtype: bool

.. js:function:: sleep(time)

   Wait for given time in milliseconds.

.. js:function:: afterMilliseconds(time, function)

   Executes function after given time in milliseconds.

.. js:function:: screenNames()

   Returns list of available screen names.

   :returns: Available screen names.
   :rtype: array of strings

.. js:function:: screenshot(format='png', [screenName])

   Returns image data with screenshot.

   Default ``screenName`` is name of the screen with mouse cursor.

   You can list valid values for ``screenName`` with :js:func:`screenNames`.

   :returns: Image data.
   :rtype: :js:class:`ByteArray`

   Example:

   .. code-block:: js

       copy('image/png', screenshot())

.. js:function:: screenshotSelect(format='png', [screenName])

   Same as :js:func:`screenshot` but allows to select an area on screen.

   :returns: Image data.
   :rtype: :js:class:`ByteArray`

.. js:function:: queryKeyboardModifiers()

   Returns list of currently pressed keyboard modifiers which can be 'Ctrl',
   'Shift', 'Alt', 'Meta'.

   :returns: Currently pressed keyboard modifiers.
   :rtype: array of strings

.. js:function:: pointerPosition()

   Returns current mouse pointer position (x, y coordinates on screen).

   :returns: Current mouse pointer coordinates.
   :rtype: array of ints (with two elements)

.. js:function:: setPointerPosition(x, y)

   Moves mouse pointer to given coordinates on screen.

   :throws Error: Thrown if the pointer position couldn't be set (for example,
                  unsupported on current the system).

.. js:function:: iconColor()

   Get current tray and window icon color name.

   :returns: Current icon color.
   :rtype: string

.. js:function:: /*set*/ iconColor(colorName)

   Set current tray and window icon color name (examples: 'orange', '#ffa500', '#09f').

   Resets color if color name is empty string.

   :throws Error: Thrown if the color name is empty or invalid.

   .. code-block:: js

       // Flash icon for few moments to get attention.
       var color = iconColor()
       for (var i = 0; i < 10; ++i) {
         iconColor("red")
         sleep(500)
         iconColor(color)
         sleep(500)
       }

   .. seealso::

      :js:data:`mimeColor`

.. js:function:: iconTag()

   Get current tray and window icon tag text.

   :returns: Current icon tag.
   :rtype: string

.. js:function:: /*set*/ iconTag(tag)

   Set current tray and window tag text.

.. js:function:: iconTagColor()

   Get current tray and window tag color name.

   :returns: Current icon tag color.
   :rtype: string

.. js:function:: /*set*/ iconTagColor(colorName)

   Set current tray and window tag color name.

   :throws Error: Thrown if the color name is invalid.

.. js:function:: loadTheme(path)

   Loads theme from an INI file.

   :throws Error: Thrown if the file cannot be read or is not valid INI format.

.. js:function:: onClipboardChanged()

   Called when clipboard or `Linux mouse selection`_ changes and is not set by
   CopyQ, is not marked as hidden nor secret (see the other callbacks).

   Default implementation is:

   .. code-block:: js

       if (!hasData()) {
           updateClipboardData();
       } else if (runAutomaticCommands()) {
           saveData();
           updateClipboardData();
       } else {
           clearClipboardData();
       }

.. js:function:: onOwnClipboardChanged()

   Called when clipboard or `Linux mouse selection`_ is set by CopyQ and is not
   marked as hidden nor secret (see the other callbacks).

   Owned clipboard data contains :js:data:`mimeOwner` format.

   Default implementation calls :js:func:`updateClipboardData`.

.. js:function:: onHiddenClipboardChanged()

   Called when clipboard or `Linux mouse selection`_ changes and is marked as
   hidden but not secret (see the other callbacks).

   Hidden clipboard data contains :js:data:`mimeHidden` format set to ``1``.

   Default implementation calls :js:func:`updateClipboardData`.

.. js:function:: onSecretClipboardChanged()

   Called if the clipboard or `Linux mouse selection`_ changes and contains a
   password or other secret (for example, copied from clipboard manager).

   The default implementation clears all data, so they are not accessible using
   :js:func:`data` and :js:func:`dataFormats`, except :js:data:`mimeSecret`,
   and calls :js:func:`updateClipboardData`.

   **Be careful overriding** this function (via a Script command). Calling
   `onClipboardChanged()` without clearing the data and without any further
   checks can cause storing and processing secrets from password managers. On
   the other hand, it can help to get access to the data copied, for example
   from a web browser in private mode.

.. js:function:: onClipboardUnchanged()

   Called when clipboard or `Linux mouse selection`_ changes but data remained the same.

   Default implementation does nothing.

.. js:function:: onStart()

   Called when application starts.

.. js:function:: onExit()

   Called just before application exists.

.. js:function:: runAutomaticCommands()

   Executes automatic commands on current data.

   If an executed command calls :js:func:`ignore` or have "Remove Item" or
   "Transform" check box enabled, following automatic commands won't be
   executed and the function returns ``false``. Otherwise ``true`` is returned.

   :returns: ``true`` if clipboard data should be stored, otherwise ``false``.
   :rtype: bool

.. js:function:: clearClipboardData()

   Clear clipboard visibility in GUI.

   Default implementation is:

   .. code-block:: js

       if (isClipboard()) {
           setTitle();
           hideDataNotification();
       }

.. js:function:: updateTitle()

   Update main window title and tool tip from current data.

   Called when clipboard changes.

.. js:function:: updateClipboardData()

   Sets current clipboard data for tray menu, window title and notification.

   Default implementation is:

   .. code-block:: js

       if (isClipboard()) {
           updateTitle();
           showDataNotification();
           setClipboardData();
       }

.. js:function:: setTitle([title])

   Set main window title and tool tip.

.. js:function:: synchronizeToSelection(text)

   Synchronize current data from clipboard to `Linux mouse selection`_.

   Called automatically from clipboard monitor process if option
   ``copy_clipboard`` is enabled.

   Default implementation calls :js:func:`provideSelection`.

.. js:function:: synchronizeFromSelection(text)

   Synchronize current data from `Linux mouse selection`_ to clipboard.

   Called automatically from clipboard monitor process if option
   ``copy_selection`` is enabled.

   Default implementation calls :js:func:`provideClipboard`.

.. js:function:: clipboardFormatsToSave()

   Returns list of clipboard format to save automatically.

   :returns: Formats to get and save automatically from clipboard.
   :rtype: array of strings

   Override the function, for example, to save only plain text:

   .. code-block:: js

       global.clipboardFormatsToSave = function() {
           return ["text/plain"]
       }

   Or to save additional formats:

   .. code-block:: js

       var originalFunction = global.clipboardFormatsToSave;
       global.clipboardFormatsToSave = function() {
           return originalFunction().concat([
               "text/uri-list",
               "text/xml"
           ])
       }

.. js:function:: saveData()

   Save current data (depends on `mimeOutputTab`).

.. js:function:: hasData()

   Returns true only if some non-empty data can be returned by data().

   Empty data is combination of whitespace and null characters or some internal
   formats (`mimeWindowTitle`, `mimeClipboardMode` etc.)

   :returns: ``true`` if there are some data, otherwise ``false``.
   :rtype: bool

.. js:function:: showDataNotification()

   Show notification for current data.

.. js:function:: hideDataNotification()

   Hide notification for current data.

.. js:function:: setClipboardData()

   Sets clipboard data for menu commands.

.. js:function:: styles()

   List available styles for ``style`` option.

   :returns: Style identifiers.
   :rtype: array of strings

   To change or update style use:

   .. code-block:: js

       config("style", styleName)

.. js:function:: onItemsAdded()

   Called when items are added to a tab.

   The target tab is returned by `selectedTab()`.

   The new items can be accessed with `selectedItemsData()`,
   `selectedItemData()`, `selectedItems()` and `ItemSelection().current()`.

.. js:function:: onItemsRemoved()

   Called when items are being removed from a tab.

   The target tab is returned by `selectedTab()`.

   The items scheduled for removal can be accessed with `selectedItemsData()`,
   `selectedItemData()`, `selectedItems()` and `ItemSelection().current()`.

   If the exit code is non-zero (for example `fail()` is called), items will
   not be removed. But this can also cause a new items not to be added if the
   tab is full.

.. js:function:: onItemsChanged()

   Called when data in items change.

   The target tab is returned by `selectedTab()`.

   The modified items can be accessed with `selectedItemsData()`,
   `selectedItemData()`, `selectedItems()` and `ItemSelection().current()`.

.. js:function:: onTabSelected()

   Called when another tab is opened.

   The newly selected tab is returned by `selectedTab()`.

   The changed items can be accessed with `selectedItemsData()`,
   `selectedItemData()`, `selectedItems()` and `ItemSelection().current()`.

.. js:function:: onItemsLoaded()

   Called when all items are loaded into a tab.

   The target tab is returned by `selectedTab()`.

Types
-----

.. js:class:: ByteArray

   Wrapper for QByteArray Qt class.

   See `QByteArray <http://doc.qt.io/qt-5/qbytearray.html>`__.

   ``ByteArray`` is used to store all item data (image data, HTML and even
   plain text).

   Use :js:func:`str` to convert it to string. Strings are usually more
   versatile. For example to concatenate two items, the data need to be
   converted to strings first.

   .. code-block:: js

       var text = str(read(0)) + str(read(1))

.. js:class:: File

   Wrapper for QFile Qt class.

   See `QFile <http://doc.qt.io/qt-5/qfile.html>`__.

   To open file in different modes use:

   - `open()` - read/write
   - `openReadOnly()` - read only
   - `openWriteOnly()` - write only, truncates the file
   - `openAppend()` - write only, appends to the file

   Following code reads contents of "README.md" file from current
   directory:

   .. code-block:: js

       var f = new File('README.md')
       if (!f.openReadOnly())
         throw 'Failed to open the file: ' + f.errorString()
       var bytes = f.readAll()

   Following code writes to a file in home directory:

   .. code-block:: js

       var dataToWrite = 'Hello, World!'
       var filePath = Dir().homePath() + '/copyq.txt'
       var f = new File(filePath)
       if (!f.openWriteOnly() || f.write(dataToWrite) == -1)
         throw 'Failed to save the file: ' + f.errorString()

       // Always flush the data and close the file,
       // before opening the file in other application.
       f.close()

.. js:class:: Dir

   Wrapper for QDir Qt class.

   Use forward slash as path separator, for example "D:/Documents/".

   See `QDir <http://doc.qt.io/qt-5/qdir.html>`__.

.. js:class:: TemporaryFile

   Wrapper for QTemporaryFile Qt class.

   See `QTemporaryFile <https://doc.qt.io/qt-5/qtemporaryfile.html>`__.

   .. code-block:: js

       var f = new TemporaryFile()
       f.open()
       f.setAutoRemove(false)
       popup('New temporary file', f.fileName())

   To open file in different modes, use same open methods as for `File`.

.. js:class:: Settings

   Reads and writes INI configuration files. Wrapper for QSettings Qt class.

   See `QSettings <https://doc.qt.io/qt-5/qsettings.html>`__.

   .. code-block:: js

       // Open INI file
       var configPath = Dir().homePath() + '/copyq.ini'
       var settings = new Settings(configPath)

       // Save an option
       settings.setValue('option1', 'test')

       // Store changes to the config file now instead of at the end of
       // executing the script
       settings.sync()

       // Read the option value
       var value = settings.value('option1')

   Working with arrays:

   .. code-block:: js

       // Write array
       var settings = new Settings(configPath)
       settings.beginWriteArray('array1')
       settings.setArrayIndex(0)
       settings.setValue('some_option', 1)
       settings.setArrayIndex(1)
       settings.setValue('some_option', 2)
       settings.endArray()
       settings.sync()

       // Read array
       var settings = new Settings(configPath)
       const arraySize = settings.beginReadArray('array1')
       for (var i = 0; i < arraySize; i++) {
           settings.setArrayIndex(i);
           print('Index ' + i + ': ' + settings.value('some_option') + '\n')
       }

.. js:class:: Item

   Object with MIME types of an item.

   Each property is MIME type with data.

   Example:

   .. code-block:: js

       var item = {}
       item[mimeText] = 'Hello, World!'
       item[mimeHtml] = '<p>Hello, World!</p>'
       write(mimeItems, pack(item))

.. js:class:: ItemSelection

   List of items from given tab.

   An item in the list represents the same item in tab even if it is moved to a
   different row.

   New items in the tab are not added automatically into the selection.

   To create new empty selection use ``ItemSelection()`` then add items with
   ``select*()`` methods.

   Example - move matching items to the top of the tab:

   .. code-block:: js

       ItemSelection().select(/^prefix/).move(0)

   Example - remove all items from given tab but keep pinned items:

   .. code-block:: js

       ItemSelection(tabName).selectRemovable().removeAll();

   Example - modify items containing "needle" text:

   .. code-block:: js

       var sel = ItemSelection().select(/needle/, mimeText);
       for (var index = 0; index < sel.length; ++index) {
           var item = sel.itemAtIndex(index);
           item[mimeItemNotes] = 'Contains needle';
           sel.setItemAtIndex(index, item);
       }

   Example - selection with new items only:

   .. code-block:: js

       var sel = ItemSelection().selectAll()
       add("New Item 1")
       add("New Item 2")
       sel.invert()
       sel.items();

   Example - sort items alphabetically:

   .. code-block:: js

       var sel = ItemSelection().selectAll();
       const texts = sel.itemsFormat(mimeText);
       sel.sort(function(i,j){
           return texts[i] < texts[j];
       });

   .. js:attribute:: tab

       Tab name

   .. js:attribute:: length

       Number of filtered items in the selection

   .. js:method:: selectAll()

       Select all items in the tab.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: select(regexp, [mimeType])

       Select additional items matching the regular expression.

       If regexp is a valid regular expression and ``mimeType`` is not set,
       this selects items with matching text.

       If regexp matches empty strings and ``mimeType`` is set, this selects
       items containing the MIME type.

       If regexp is ``undefined`` and ``mimeType`` is set, this select items
       not containing the MIME type.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: selectRemovable()

       Select only items that can be removed.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: invert()

       Select only items not in the selection.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: deselectIndexes(int[])

       Deselect items at given indexes in the selection.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: deselectSelection(ItemSelection)

       Deselect items in other selection.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: current()

       Deselects all and selects only the items which were selected when the
       command was triggered.

       See `Selected Items`_.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: removeAll()

       Delete all items in the selection (if possible).

       :returns: self
       :rtype: ItemSelection

   .. js:method:: move(row)

       Move all items in the selection to the target row.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: sort(row)

       Sort items with a comparison function.

       The comparison function takes two arguments, indexes to the selection,
       and returns true only if the item in the selection under the first index
       should be sorted above the item under the second index.

       Items will be reordered in the tab and in the selection object.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: copy()

       Clone the selection object.

       :returns: cloned object
       :rtype: ItemSelection

   .. js:method:: rows()

       Returns selected rows.

       :returns: Selected rows
       :rtype: array of ints

   .. js:method:: itemAtIndex(index)

       Returns item data at given index in the selection.

       :returns: Item data
       :rtype: :js:class:`Item`

   .. js:method:: setItemAtIndex(index, Item)

       Sets data to the item at given index in the selection.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: items()

       Return list of data from selected items.

       :returns: Selected item data
       :rtype: array of :js:class:`Item`

   .. js:method:: setItems(Item[])

       Set data for selected items.

       :returns: self
       :rtype: ItemSelection

   .. js:method:: itemsFormat(mimeType)

       Return list of data from selected items containing specified MIME type.

       :returns: Selected item data containing only the format
       :rtype: array of :js:class:`Item`

   .. js:method:: setItemsFormat(mimeType, data)

       Set data for given MIME type for the selected items.

       :returns: self
       :rtype: ItemSelection

.. js:class:: FinishedCommand

   Properties of finished command.

   .. js:attribute:: stdout

       Standard output

   .. js:attribute:: stderr

       Standard error output

   .. js:attribute:: exit_code

       Exit code

.. js:class:: NetworkReply

   Received network reply object.

   .. js:attribute:: data

       Reply data

   .. js:attribute:: status

       HTTP status

   .. js:attribute:: error``

       Error string (set only if an error occurred)

   .. js:attribute:: redirect

       URL for redirection (set only if redirection is needed)

   .. js:attribute:: headers

       Reply headers (array of pairs with header name and header content)

   .. js:attribute:: finished

       True only if request has been completed, false only for unfinished
       asynchronous requests

.. js:class:: Command

   Wrapper for a command (from Command dialog).

   Properties are same as members of `Command
   struct <https://github.com/hluk/CopyQ/blob/master/src/common/command.h>`__.

Objects
-------

.. js:data:: arguments (Array)

   Array for accessing arguments passed to current function or the script
   (``arguments[0]`` is the script itself).

.. js:data:: global

    Object allowing to modify global scope which contains all functions like
    :js:func:`copy` or :js:func:`add`.

    This is useful for :ref:`commands-script`.

.. js:data:: console

    Allows some logging and debugging.

   .. code-block:: js

        // Print a message if COPYQ_LOG_LEVEL=DEBUG
        // environment variable is set
        console.log(
            'Supported console properties/functions:',
            Object.getOwnPropertyNames(console))
        console.warn('Changing clipboard...')

        // Elapsed time
        console.time('copy')
        copy('TEST')
        console.timeEnd('copy')

        // Ensure a condition is true before continuing
        console.assert(str(clipboard()) == 'TEST')

MIME Types
----------

Item and clipboard can provide multiple formats for their data. Type of
the data is determined by MIME type.

Here is list of some common and builtin (start with
``application/x-copyq-``) MIME types.

These MIME types values are assigned to global variables prefixed with
``mime``.

.. note::

   Content for following types is UTF-8 encoded.

.. js:data:: mimeText

   Data contains plain text content. Value: 'text/plain'.

.. js:data:: mimeHtml

   Data contains HTML content. Value: 'text/html'.

.. js:data:: mimeUriList

   Data contains list of links to files, web pages etc. Value: 'text/uri-list'.

.. js:data:: mimeWindowTitle

   Current window title for copied clipboard. Value: 'application/x-copyq-owner-window-title'.

.. js:data:: mimeItems

   Serialized items. Value: 'application/x-copyq-item'.

.. js:data:: mimeItemNotes

   Data contains notes for item. Value: 'application/x-copyq-item-notes'.

.. js:data:: mimeIcon

   Data contains icon for item. Value: 'application/x-copyq-item-icon'.

.. js:data:: mimeOwner

   If available, the clipboard was set from CopyQ (from script or copied items). Value: 'application/x-copyq-owner'.

   Such clipboard is ignored in CopyQ, i.e. it won't be stored in clipboard
   tab and automatic commands won't be executed on it.

.. js:data:: mimeClipboardMode

   Contains ``selection`` if data is from `Linux mouse selection`_. Value: 'application/x-copyq-clipboard-mode'.

.. js:data:: mimeCurrentTab

   Current tab name when invoking command from main window. Value: 'application/x-copyq-current-tab'.

   Following command print the tab name when invoked from main window:

   ::

       copyq data application/x-copyq-current-tab
       copyq selectedTab

.. js:data:: mimeSelectedItems

   Selected items when invoking command from main window. Value: 'application/x-copyq-selected-items'.

.. js:data:: mimeCurrentItem

   Current item when invoking command from main window. Value: 'application/x-copyq-current-item'.

.. js:data:: mimeHidden

   If set to ``1``, the clipboard or item content will be hidden in GUI. Value: 'application/x-copyq-hidden'.

   This won't hide notes and tags.

   Example -- clear window title and tool tip:

   ::

       copyq copy application/x-copyq-hidden 1 plain/text "This is secret"

.. js:data:: mimeSecret

   If set to ``1``, the clipboard contains a password or other secret (for example, copied from clipboard manager).

.. js:data:: mimeShortcut

   Application or global shortcut which activated the command. Value: 'application/x-copyq-shortcut'.

   ::

       copyq:
       var shortcut = data(mimeShortcut)
       popup("Shortcut Pressed", shortcut)

.. js:data:: mimeColor

   Item color (same as the one used by themes). Value: 'application/x-copyq-color'.

   Examples::

       #ffff00
       rgba(255,255,0,0.5)
       bg - #000099

.. js:data:: mimeOutputTab

   Name of the tab where to store new item. Value: 'application/x-copyq-output-tab'.

   The clipboard data will be stored in tab with this name after all
   automatic commands are run.

   Clear or remove the format to omit storing the data.

   Example -- automatic command that avoids storing clipboard data:

   .. code-block:: js

       removeData(mimeOutputTab)

   Valid only in automatic commands.

.. js:data:: mimeDisplayItemInMenu

   Indicates if display commands run for a menu. Value: 'application/x-copyq-display-item-in-menu'.

   Set to "1" for display commands if the item data is related to a menu item
   instead of an item list.

Selected Items
--------------

The internal state for currently evaluated script/command stores references
(not rows or item data) to the current and selected items and it do not change
after the state is retrieved from GUI.

The state is retrieved before the script/command starts if it is invoked from
the application with a shortcut, from menu, toolbar or the Action dialog.
Otherwise, the state is retrieved when needed (for example the first
``selectedItems()`` call) for scripts/commands run externally (for example from
command line or from automatic commands on clipboard content change).

If a selected or current item is moved, script functions will return the new
rows. For example ``selectedItems()`` returning ``[0,1]`` will return ``[1,0]``
after the items are swapped. Same goes for selected item data.

If a selected or current item is removed, their references in the internal
state are invalidated. These references will return -1 for row and empty object
for item data. For example ``selectedItems()`` returning ``[0,1]`` will return
``[0,-1]`` after the item on the second row is removed.

If tab is renamed, all references to current and selected items are invalidated
because the tab data need to be initiated again.

Linux Mouse Selection
---------------------

In many application on Linux, if you select a text with mouse, it's possible to
paste it with middle mouse button.

The text is stored separately from normal clipboard content.

On non-Linux system, functions that support mouse selection will do nothing
(for example :js:func:`copySelection`) or return ``undefined`` (in case of
:js:func:`selection`).

Plugins
-------

Use ``plugins`` object to access functionality of plugins.

.. js:function:: plugins.itemsync.selectedTabPath()

   Returns synchronization path for current tab (mimeCurrentTab).

   .. code-block:: js

       var path = plugins.itemsync.selectedTabPath()
       var baseName = str(data(plugins.itemsync.mimeBaseName))
       var absoluteFilePath = Dir(path).absoluteFilePath(baseName)
       // NOTE: Known file suffix/extension can be missing in the full path.

.. js:class:: plugins.itemsync.tabPaths

   Object that maps tab name to synchronization path.

   .. code-block:: js

       var tabName = 'Downloads'
       var path = plugins.itemsync.tabPaths[tabName]

.. js:data:: plugins.itemsync.mimeBaseName (application/x-copyq-itemsync-basename)

   MIME type for accessing base name (without full path).

   Known file suffix/extension can be missing in the base name.

.. js:data:: plugins.itemtags.userTags (Array)

   List of user-defined tags.

.. js:function:: plugins.itemtags.tags(row, ...)

   List of tags for items in given rows.

.. js:function:: plugins.itemtags.tag(tagName, [rows, ...])

   Add given tag to items in given rows or selected items.

   See `Selected Items`_.

.. js:function:: plugins.itemtags.untag(tagName, [rows, ...])

   Remove given tag from items in given rows or selected items.

   See `Selected Items`_.

.. js:function:: plugins.itemtags.clearTags([rows, ...])

   Remove all tags from items in given rows or selected items.

   See `Selected Items`_.

.. js:function:: plugins.itemtags.hasTag(tagName, [rows, ...])

   Return true if given tag is present in any of items in given rows or
   selected items.

   See `Selected Items`_.

.. js:data:: plugins.itemtags.mimeTags (application/x-copyq-tags)

   MIME type for accessing list of tags.

   Tags are separated by comma.

.. js:function:: plugins.itempinned.isPinned(rows, ...)

   Returns true only if any item in given rows is pinned.

.. js:function:: plugins.itempinned.pin(rows, ...)

   Pin items in given rows or selected items or new item created from clipboard
   (if called from automatic command).

.. js:function:: plugins.itempinned.unpin(rows, ...)

   Unpin items in given rows or selected items.

.. js:data:: plugins.itempinned.mimePinned (application/x-copyq-item-pinned)

   Presence of the format in an item indicates that it is pinned.

