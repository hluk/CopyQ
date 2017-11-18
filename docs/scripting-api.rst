.. _scripting-api:

Scripting API
=============

CopyQ provides scripting capabilities to automatically handle clipboard
changes, organize items, change settings and much more.

In addition to features provided by Qt Script there are following
`functions`_, `types`_, `objects`_ and `MIME types`_.

Execute Script
--------------

The scripts can be executed:

-  from commands (in Action or Command dialogs -- F5, F6 shortcuts) if
   the first line starts with ``copyq:``,
-  from command line as ``copyq eval '<SCRIPT>'``,
-  from command line as ``cat script.js | copyq eval -``,
-  from command line as
   ``copyq <SCRIPT_FUNCTION> <FUNCTION_ARGUMENT_1> <FUNCTION_ARGUMENT_2> ...``.

When run from command line, result of last expression is printed on
stdout.

Command exit values are:

-  0 - script finished without error,
-  1 - ``fail()`` was called,
-  2 - bad syntax,
-  3 - exception was thrown.

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

E.g.

.. code-block:: bash

    copyq tab clipboard separator "," read 0 1 2

After ``eval`` no arguments are treated as functions since it can access
all arguments.

Arguments recognize escape sequences ``\n`` (new line), ``\t``
(tabulator character) and ``\\`` (backslash).

Argument ``-e`` is identical to ``eval``.

Argument ``-`` is replaced with data read from stdin.

Argument ``--`` is skipped and all the remaining arguments are
interpreted as they are (escape sequences are ignored and ``-e``, ``-``,
``--`` are left unchanged).

Functions
---------

Argument list parts ``...`` and ``[...]`` are optional and can be
omitted.

.. js:function:: String version()

   Returns version string.

.. js:function:: String help()

   Returns help string.

.. js:function:: String help(searchString, ...)

   Returns help for matched commands.

.. js:function:: show()

   Shows main window.

.. js:function:: show(tabName)

   Shows tab.

.. js:function:: showAt()

   Shows main window under mouse cursor.

.. js:function:: showAt(x, y, [width, height])

   Shows main window with given geometry.

.. js:function:: showAt(x, y, width, height, tabName)

   Shows tab with given geometry.

.. js:function:: hide()

   Hides main window.

.. js:function:: bool toggle()

   Shows or hides main window.

   Returns true only if main window is being shown.

.. js:function:: menu()

   Opens context menu.

.. js:function:: menu(tabName, [maxItemCount, [x, y]])

   Shows context menu for given tab.

   This menu doesn't show clipboard and doesn't have any special actions.

   Second argument is optional maximum number of items. The default value
   same as for tray (i.e. value of ``config('tray_items')``).

   Optional arguments x, y are coordinates in pixels on screen where menu
   should show up. By default menu shows up under the mouse cursor.

.. js:function:: exit()

   Exits server.

.. js:function:: disable(), enable()

   Disables or enables clipboard content storing.

.. js:function:: bool monitoring()

   Returns true only if clipboard storing is enabled.

.. js:function:: bool visible()

   Returns true only if main window is visible.

.. js:function:: bool focused()

   Returns true only if main window has focus.

.. js:function:: filter(filterText)

   Sets text for filtering items in main window.

.. js:function:: ignore()

   Ignores current clipboard content (used for automatic commands).

   This does all of the below.

   -  Skips any next automatic commands.
   -  Omits changing window title and tray tool tip.
   -  Won't store content in clipboard tab.

.. js:function:: ByteArray clipboard([mimeType])

   Returns clipboard data for MIME type (default is text).

   Pass argument ``"?"`` to list available MIME types.

.. js:function:: ByteArray selection([mimeType])

   Same as ``clipboard()`` for Linux/X11 mouse selection.

.. js:function:: bool hasClipboardFormat(mimeType)

   Returns true only if clipboard contains MIME type.

.. js:function:: bool hasSelectionFormat(mimeType)

   Same as ``hasClipboardFormat()`` for Linux/X11 mouse selection.

.. js:function:: bool isClipboard()

   Returns true only in automatic command triggered by clipboard change.

   This can be used to check if current automatic command was triggered by
   clipboard and not Linux/X11 mouse selection change.

.. js:function:: bool copy(text)

   Sets clipboard plain text.

   Same as ``copy(mimeText, text)``.

.. js:function:: bool copy(mimeType, data, [mimeType, data]...)

   Sets clipboard data.

   This also sets ``mimeOwner`` format so automatic commands are not run on
   the new data and it's not store in clipboard tab.

   Exception is thrown if clipboard fails to be set.

   Example (set both text and rich text):

   .. code-block:: js

       copy(mimeText, 'Hello, World!',
            mimeHtml, '<p>Hello, World!</p>')

.. js:function:: bool copy()

   Sends ``Ctrl+C`` to current window.

   Exception is thrown if clipboard doesn't change (clipboard is reset
   before sending the shortcut).

.. js:function:: ByteArray copySelection(...)

   Same as ``copy(...)`` for Linux/X11 mouse selection.

.. js:function:: paste()

   Pastes current clipboard.

   This is basically only sending ``Shift+Insert`` shortcut to current
   window.

   Correct functionality depends a lot on target application and window
   manager.

.. js:function:: Array tab()

   Returns array of with tab names.

.. js:function:: tab(tabName)

   Sets current tab for the script.

   E.g. following script selects third item (index is 2) from tab "Notes".

   .. code-block:: js

       tab('Notes')
       select(2)

.. js:function:: removeTab(tabName)

   Removes tab.

.. js:function:: renameTab(tabName, newTabName)

   Renames tab.

.. js:function:: String tabIcon(tabName)

   Returns path to icon for tab.

.. js:function:: tabIcon(tabName, iconPath)

   Sets icon for tab.

.. js:function:: count(), length(), size()

   Returns amount of items in current tab.

.. js:function:: select(row)

   Copies item in the row to clipboard.

   Additionally, moves selected item to top depending on settings.

.. js:function:: next()

   Copies next item from current tab to clipboard.

.. js:function:: previous()

   Copies previous item from current tab to clipboard.

.. js:function:: add(text, ...)

   Adds new text items to current tab.

   Throws an exception if space for the items cannot be allocated.

.. js:function:: insert(row, text)

   Inserts new text items to current tab.

.. js:function:: remove(row, ...)

   Removes items in current tab.

   Throws an exception if some items cannot be removed.

.. js:function:: edit([row|text] ...)

   Edits items in current tab.

   Opens external editor if set, otherwise opens internal editor.

.. js:function:: ByteArray read([mimeType]);

   Same as ``clipboard()``.

.. js:function:: ByteArray read(mimeType, row, ...);

   Returns concatenated data from items or clipboard if row is negative.

   Pass argument ``"?"`` to list available MIME types.

.. js:function:: write(row, mimeType, data, [mimeType, data]...)

   Inserts new item to current tab.

   Throws an exception if space for the items cannot be allocated.

.. js:function:: change(row, mimeType, data, [mimeType, data]...)

   Changes data in item in current tab.

   If data is ``undefined`` the format is removed from item.

.. js:function:: String separator()

   Returns item separator (used when concatenating item data).

.. js:function:: separator(separator)

   Sets item separator for concatenating item data.

.. js:function:: action()

   Opens action dialog.

.. js:function:: action(row, ..., command, outputItemSeparator)

   Runs command for items in current tab.

.. js:function:: popup(title, message, [time=8000])

   Shows popup message for given time in milliseconds.

   If ``time`` argument is set to -1, the popup is hidden only after mouse
   click.

.. js:function:: notification(...)

   Shows popup message with icon and buttons.

   Each button can have script and data.

   If button is clicked the notification is hidden and script is executed
   with the data passed as stdin.

   The function returns immediatelly (doesn't wait on user input).

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

.. js:function:: importTab(fileName)

   Imports items from file to a new tab.

.. js:function:: String config()

   Returns help with list of available options.

.. js:function:: String config(optionName)

   Returns value of given option.

   Throws an exception if the option is invalid.

.. js:function:: String config(optionName, value)

   Sets option and returns new value.

   Throws an exception if the option is invalid.

.. js:function:: String config(optionName, value, ...)

   Sets multiple options and return list with values in format ``optionName=newValue``.

   Throws an exception if there is an invalid option in which case it won't
   set any options.

.. js:function:: bool toggleConfig(optionName)

   Toggles an option (true to false and vice versa) and returns the new value.

.. js:function:: String info([pathName])

   Returns paths and flags used by the application.

   E.g. following command prints path to configuration file.

   .. code-block:: bash

       copyq info config

.. js:function:: Value eval(script)

   Evaluates script and returns result.

.. js:function:: Value source(fileName)

   Evaluates script file and returns result of last expression in the script.

   This is useful to move some common code out of commands.

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

.. js:function:: String currentPath([path])

   Get or set current path.

.. js:function:: String str(value)

   Converts a value to string.

   If ByteArray object is the argument, it assumes UTF8 encoding. To use
   different encoding, use ``toUnicode()``.

.. js:function:: ByteArray input()

   Returns standard input passed to the script.

.. js:function:: String toUnicode(ByteArray, encodingName)

   Returns string for bytes with given encoding.

.. js:function:: String toUnicode(ByteArray)

   Returns string for bytes with encoding detected by checking Byte Order Mark (BOM).

.. js:function:: ByteArray fromUnicode(String, encodingName)

   Returns encoded text.

.. js:function:: ByteArray data(mimeType)

   Returns data for automatic commands or selected items.

   If run from menu or using non-global shortcut the data are taken from
   selected items.

   If run for automatic command the data are clipboard content.

.. js:function:: ByteArray setData(mimeType, data)

   Modifies data for ``data()`` and new clipboard item.

   Next automatic command will get updated data.

   This is also the data used to create new item from clipboard.

   E.g. following automatic command will add creation time data and tag to
   new items.

   ::

       copyq:
       var timeFormat = 'yyyy-MM-dd hh:mm:ss'
       setData('application/x-copyq-user-copy-time', dateString(timeFormat))
       setData(mimeTags, 'copied: ' + time)

   E.g. following menu command will add tag to selected items.

   ::

       copyq:
       setData('application/x-copyq-tags', 'Important')

.. js:function:: ByteArray removeData(mimeType)

   Removes data for ``data()`` and new clipboard item.

.. js:function:: Array dataFormats()

   Returns formats available for ``data()``.

.. js:function:: print(value)

   Prints value to standard output.

.. js:function:: abort()

   Aborts script evaluation.

.. js:function:: fail()

   Aborts script evaluation with nonzero exit code.

.. js:function:: setCurrentTab(tabName)

   Focus tab without showing main window.

.. js:function:: selectItems(row, ...)

   Selects items in current tab.

.. js:function:: String selectedTab()

   Returns tab that was selected when script was executed.

   See `Selected Items`_.

.. js:function:: [row, ...] selectedItems()

   Returns selected rows in current tab.

   See `Selected Items`_.

.. js:function:: Item selectedItemData(index)

   Returns data for given selected item.

   The data can empty if the item was removed during execution of the
   script.

   See `Selected Items`_.

.. js:function:: bool setSelectedItemData(index, Item)

   Set data for given selected item.

   Returns false only if the data cannot be set, usually if item was
   removed.

   See `Selected Items`_.

.. js:function:: Item[] selectedItemsData()

   Returns data for all selected item.

   Some data can empty if the item was removed during execution of the
   script.

   See `Selected Items`_.

.. js:function:: void setSelectedItemsData(Item[])

   Set data to all selected items.

   Some data may not be set if the item was removed during execution of the
   script.

   See `Selected Items`_.

.. js:function:: int currentItem(), int index()

   Returns current row in current tab.

   See `Selected Items`_.

.. js:function:: String escapeHtml(text)

   Returns text with special HTML characters escaped.

.. js:function:: Item unpack(data)

   Returns deserialized object from serialized items.

.. js:function:: ByteArray pack(item)

   Returns serialized item.

.. js:function:: Item getItem(row)

   Returns an item in current tab.

.. js:function:: setItem(row, item)

   Inserts item to current tab.

.. js:function:: String toBase64(data)

   Returns base64-encoded data.

.. js:function:: ByteArray fromBase64(base64String)

   Returns base64-decoded data.

.. js:function:: bool open(url, ...)

   Tries to open URLs in appropriate applications.

   Returns true only if all URLs were successfully opened.

.. js:function:: FinishedCommand execute(argument, ..., null, stdinData, ...)

   Executes a command.

   All arguments after ``null`` are passed to standard input of the
   command.

   If arguments is function it will be called with array of lines read from
   stdout whenever available.

   E.g. create item for each line on stdout:

   .. code-block:: js

       execute('tail', '-f', 'some_file.log',
               function(lines) { add.apply(this, lines) })

   Returns object for the finished command or ``undefined`` on failure.

.. js:function:: String currentWindowTitle()

   Returns window title of currently focused window.

.. js:function:: Value dialog(...)

   Shows messages or asks user for input.

   Arguments are names and associated values.

   Special arguments:

   -  '.title' - dialog title
   -  '.icon' - dialog icon (see below for more info)
   -  '.style' - Qt style sheet for dialog
   -  '.height', '.width', '.x', '.y' - dialog geometry
   -  '.label' - dialog message (can contain basic HTML)

   .. code-block:: js

       dialog(
         '.title', 'Command Finished',
         '.label', 'Command <b>successfully</b> finished.'
         )

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

   Editable combo box can be created by passing array. Current value can be
   provided using ``.defaultChoice`` (by default it's the first item).

   .. code-block:: js

       var text = dialog('.defaultChoice', '', 'Select', ['a', 'b', 'c'])

   List can be created by prefixing name/label with ``.list:`` and passing
   array.

   .. code-block:: js

       var items = ['a', 'b', 'c']
       var selected_index = dialog('.list:Select', items)
       if (selected_index)
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

.. js:function:: Array settings()

   Returns array with names of all custom options.

.. js:function:: Value settings(optionName)

   Returns value for an option.

.. js:function:: settings(optionName)

   Sets value for a new option or overrides existing option.

.. js:function:: String dateString(format)

   Returns text representation of current date and time.

   See
   `QDateTime::toString() <http://doc.qt.io/qt-5/qdatetime.html#toString>`__
   for details on formatting date and time.

   Example:

   .. code-block:: js

       var now = dateString('yyyy-MM-dd HH:mm:ss')

.. js:function:: Command[] commands()

   Return list of all commands.

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

.. js:function:: String exportCommands(Command[])

   Return exported command text.

.. js:function:: NetworkReply networkGet(url)

   Sends HTTP GET request.

   Returns reply.

.. js:function:: NetworkReply networkPost(url, postData)

   Sends HTTP POST request.

   Returns reply.

.. js:function:: ByteArray env(name)

   Returns value of environment variable with given name.

.. js:function:: bool setEnv(name, value)

   Sets environment variable with given name to given value.

   Returns true only if the variable was set.

.. js:function:: sleep(time)

   Wait for given time in milliseconds.

.. js:function:: ByteArray screenshot(format='png', [screenName])

   Returns image data with screenshot.

   Default ``screenName`` is name of the screen with mouse cursor.

   You can list valid values for ``screenName`` with ``screenNames()``.

   Example:

   .. code-block:: js

       copy('image/png', screenshot())

.. js:function:: ByteArray screenshotSelect(format='png', [screenName])

   Same as ``screenshot()`` but allows to select an area on screen.

.. js:function:: String[] screenNames()

   Returns list of available screen names.

.. js:function:: String[] queryKeyboardModifiers()

   Returns list of currently pressed keyboard modifiers which can be 'Ctrl', 'Shift', 'Alt', 'Meta'.

.. js:function:: iconColor([colorName])

   Get or set current tray and window icon color name.

   Throws exception is the color name is invalid.

   .. code-block:: js

       // Flash icon for few moments to get attention.
       var color = iconColor()
       for (var i = 0; i < 10; ++i) {
         iconColor("red")
         sleep(500)
         iconColor(color)
         sleep(500)
       }

.. js:function:: iconTag([tag])

   Get or set current tray and window tag text.

.. js:function:: iconTagColor([colorName])

   Get or set current tray and window tag color name.

   Throws exception is the color name is invalid.

Types
-----

.. js:class:: ByteArray

   Wrapper for QByteArray Qt class.

   See `QByteArray <http://doc.qt.io/qt-5/qbytearray.html>`__.

   ``ByteArray`` is used to store all item data (image data, HTML and even
   plain text).

   Use ``str()`` to convert it to string. Strings are usually more
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
   directory.

   .. code-block:: js

       var f = new File('README.md')
       if (!f.openReadOnly())
         raise 'Failed to open the file: ' + f.errorString()
       var bytes = f.readAll()

   Following code writes to a file in home directory.

   .. code-block:: js

       var dataToWrite = 'Hello, World!'
       var filePath = Dir().homePath() + '/copyq.txt'
       var f = new File(filePath)
       if (!f.openWriteOnly() || f.write(dataToWrite) == -1)
         raise 'Failed to save the file: ' + f.errorString()

       // Always flush the data and close the file,
       // before opening the file in other application.
       f.close()

.. js:class:: Dir

   Wrapper for QDir Qt class.

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

.. js:class:: Item (Object)

   Object with MIME types of an item.

   Each property is MIME type with data.

   Example:

   .. code-block:: js

       var item = {}
       item[mimeText] = 'Hello, World!'
       item[mimeHtml] = '<p>Hello, World!</p>'
       write(mimeItems, pack(item))

.. js:class:: FinishedCommand (Object)

   Properties of finished command.

   Properties are:

   -  ``stdout`` - standard output
   -  ``stderr`` - standard error output
   -  ``exit_code`` - exit code

.. js:class:: NetworkReply (Object)

   Received network reply object.

   Properties are:

   -  ``data`` - reply data
   -  ``error`` - error string (set only if an error occurred)
   -  ``redirect`` - URL for redirection (set only if redirection is
      needed)
   -  ``headers`` - reply headers (array of pairs with header name and
      header content)

.. js:class:: Command (Object)

   Wrapper for a command (from Command dialog).

   Properties are same as members of `Command
   struct <https://github.com/hluk/CopyQ/blob/master/src/common/command.h>`__.

Objects
-------

.. js:data:: arguments (Array)

   Array for accessing arguments passed to current function or the script
   (``arguments[0]`` is the script itself).

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

.. js:data:: mimeText (text/plain)

   Data contains plain text content.

.. js:data:: mimeHtml (text/html)

   Data contains HTML content.

.. js:data:: mimeUriList (text/uri-list)

   Data contains list of links to files, web pages etc.

.. js:data:: mimeWindowTitle (application/x-copyq-owner-window-title)

   Current window title for copied clipboard.

.. js:data:: mimeItems (application/x-copyq-item)

   Serialized items.

.. js:data:: mimeItemNotes (application/x-copyq-item-notes)

   Data contains notes for item.

.. js:data:: mimeOwner (application/x-copyq-owner)

   If available, the clipboard was set from CopyQ (from script or copied items).

   Such clipboard is ignored in CopyQ, i.e. it won't be stored in clipboard
   tab and automatic commands won't be executed on it.

.. js:data:: mimeClipboardMode (application/x-copyq-clipboard-mode)

   Contains ``selection`` if data is from X11 mouse selection.

.. js:data:: mimeCurrentTab (application/x-copyq-current-tab)

   Current tab name when invoking command from main window.

   Following command print the tab name when invoked from main window.

   ::

       copyq data application/x-copyq-current-tab
       copyq selectedTab

.. js:data:: mimeSelectedItems (application/x-copyq-selected-items)

   Selected items when invoking command from main window.

.. js:data:: mimeCurrentItem (application/x-copyq-current-item)

   Current item when invoking command from main window.

.. js:data:: mimeHidden (application/x-copyq-hidden)

   If set to ``1``, the clipboard or item content will be hidden in GUI.

   This won't hide notes and tags.

   E.g. if you run following, window title and tool tip will be cleared.

   ::

       copyq copy application/x-copyq-hidden 1 plain/text "This is secret"

.. js:data:: mimeShortcut (application/x-copyq-shortcut)

   Application or global shortcut which activated the command.

   ::

       copyq:
       var shortcut = data(mimeShortcut)
       popup("Shortcut Pressed", shortcut)

.. js:data:: mimeColor (application/x-copyq-color)

   Item color (same as the one used by themes).

   Examples: #ffff00 rgba(255,255,0,0.5) bg - #000099

.. js:data:: mimeOutputTab (application/x-copyq-output-tab)

   Name of the tab where to store new item.

   The clipboard data will be stored in tab with this name after all
   automatic commands are run.

   Clear or remove the format to omit storing the data.

   E.g. to omit storing the clipboard data use following in an automatic
   command.

   .. code-block:: js

       removeData(mimeOutputTab)

   Valid only in automatic commands.

.. js:data:: mimeSyncToClipboard (application/x-copyq-sync-to-selection)

   If exists the X11 selection data will be copied to clipboard.

   The synchronization will happend after all automatic commands are run.

   .. code-block:: js

       removeData(mimeSyncToClipboard)

   Valid only in Linux/X11 in automatic commands.

.. js:data:: mimeSyncToSelection (application/x-copyq-sync-to-clipboard)

   If exists the clipboard data will be copied to X11 selection.

   The synchronization will happend after all automatic commands are run.

   .. code-block:: js

       removeData(mimeSyncToSelection)

   Valid only in Linux/X11 in automatic commands.

Selected Items
--------------

Functions that get and set data for selected items and current tab are
only available if called from Action dialog or from a command which is
in menu.

Selected items are indexed from top to bottom as they appeared in the
current tab at the time the command is executed.
