CopyQ Scripting
===============

CopyQ provides scripting capabilities to automatically handle clipboard changes,
organize items, change settings and much more.

In addition to features provided by Qt Script there are following
[functions](#functions), [types](#types), [objects](#objects) and
[useful MIME types](#mime-types).

Functions
---------

Argument list parts `...` and `[...]` are optional and can be omitted.

###### String version()

Returns version string.

###### String help()

Returns help string.

###### String help(searchString, ...)

Returns help for matched commands.

###### show()

Shows main window.

###### show(tabName)

Shows tab.

###### showAt()

Shows main window under mouse cursor.

###### showAt(x, y, [width, height])

Shows main window with given geometry.

###### showAt(x, y, width, height, tabName)

Shows tab with given geometry.

###### hide()

Hides main window.

###### bool toggle()

Shows or hides main window.

Returns true only if main window is being shown.

###### menu()

Opens context menu.

###### menu()

Shows context menu for tab.

###### exit()

Exits server.

###### disable(), enable()

Disables or enables clipboard content storing.

###### bool monitoring()

Returns true only if clipboard storing is enabled.

###### bool visible()

Available since v2.4.7.

Returns true only if main window is visible.

###### bool focused()

Available since v2.4.9.

Returns true only if main window has focus.

###### filter(filterText)

Available since v2.4.9.

Sets text for filtering items in main window.

###### ignore()

Ignores current clipboard content (used for automatic commands).

This does all of the below.

- Skips any next automatic commands.
- Omits changing window title and tray tool tip.
- Won't store content in clipboard tab.

###### ByteArray clipboard([mimeType])

Returns clipboard data for MIME type (default is text).

###### ByteArray selection([mimeType])

Same as `clipboard()` for Linux/X11 mouse selection.

###### ByteArray copy(...)

Sets clipboard content.

Argument is either text or MIME types followed by data.

Example (set both text and rich text):

```js
copy('text/plain', 'Hello, World!',
     'text/html', '<p>Hello, World!</p>'
```

###### ByteArray copySelection(...)

Same as `copy()` for Linux/X11 mouse selection.

###### paste()

Pastes current clipboard.

This is basically only sending `Shift+Insert` shortcut to current window.

Correct functionality depends a lot on target application and window manager.

###### Array tab()

Returns array of with tab names.

###### tab(tabName)

Sets current tab name.

###### removeTab(tabName)

Removes tab.

###### renameTab(tabName, newTabName)

Renames tab.

###### String tabIcon(tabName)

Returns path to icon for tab.

###### tabIcon(tabName, iconPath)

Sets icon for tab.

###### count(), length(), size()

Returns amount of items in current tab.

###### select(row)

Copies item in the row to clipboard.

Additionally, moves selected item to top depending on settings.

###### next()

Copies next item from current tab to clipboard.

###### previous()

Copies previous item from current tab to clipboard.

###### add(text, ...)

Adds new text items to current tab.

###### insert(row, text)

Inserts new text items to current tab.

###### remove(row, ...)

Removes items in current tab.

###### edit(row, ...)

Edits items in current tab.

Opens external editor if set, otherwise opens internal editor.

###### ByteArray read([mimeType]);

Same as `clipboard()`.

###### ByteArray read(mimeType, row, ...);

Returns concatenated data from items.

###### write(row, mimeType, data, [mimeType, data]...)

Inserts new item to current tab.

###### change(row, mimeType, data, [mimeType, data]...)

Changes data in item in current tab.

###### String separator()

Returns item separator (used when concatenating item data).

###### separator(separator)

Sets item separator for concatenating item data.

###### action()

Opens action dialog.

###### action(row, ..., command, outputItemSeparator)

Runs command for items in current tab.

###### popup(title, message, [timeout=8000])

Shows tray popup message for given time in milliseconds.

###### exportTab(fileName)

Exports current tab into file.

###### importTab(fileName)

Imports items from file to a new tab.

###### String config()

Returns help with list of available options.

###### String config(optionName)

Returns value of given option.

###### String config(optionName, value)

Sets option.

###### Value eval(script)

Evaluates script and returns result.

###### String currentpath()

Returns current path.

###### String str(value)

Converts a value to string.

###### ByteArray input()

Returns standard input passed to the script.

###### ByteArray data(mimeType)

Returns data for command (item data, clipboard data or text from action dialog).

###### ByteArray setData(mimeType, data)

Modifies data passed to automatic commands or selected items if run from menu or using shortcut.

Next automatic command will get updated data.

This is also the data used to create new item from clipboard.

E.g. following automatic command will add creation time data and tag to new items.

    copyq:
    var timeFormat = 'yyyy-MM-dd hh:mm:ss'
    setData('application/x-copyq-user-copy-time', dateString(timeFormat))
    setData('application/x-copyq-tags', 'copied: ' + time)

E.g. following menu command will add tag to selected items.

    copyq:
    setData('application/x-copyq-tags', 'Important')

###### print(value)

Prints value to standard output.

###### abort()

Aborts script evaluation.

###### fail()

Aborts script evaluation with nonzero exit code.

###### setCurrentTab(tabName)

Set tab as current (focus tab without showing main window).

###### selectItems(row, ...)

Selects items in current tab.

###### String selectedTab()

Returns tab that was selected when script was executed.

###### [row, ...] selectedItems()

Returns array with rows of selected items in current tab.

###### int currentItem(), int index()

Returns current row in current tab.

###### String escapeHtml(text)

Returns HTML representation of text (escapes special HTML characters).

###### Item unpack(data)

Returns deserialized object from serialized items.

###### ByteArray pack(item)

Returns serialized item.

###### Item getItem(row)

Returns an item in current tab.

###### setItem(row, item)

Inserts item to current tab.

###### String toBase64(data)

Returns base64-encoded data.

###### ByteArray fromBase64(base64String)

Returns base64-decoded data.

###### QScriptValue open(url, ...)

Tries to open URLs in appropriate applications.

Returns true only if all URLs were successfully opened.

###### FinishedCommand execute(argument, ..., null, stdinData, ...)

Executes a command.

All arguments after `null` are passed to standard input of the command.

Returns object for the finished command or `undefined` on failure.

###### String currentWindowTitle()

Returns window title of currently focused window.

###### Value dialog(...)

Shows messages or asks user for input.

Arguments are names and associated values.

Special arguments:

- '.title' - dialog title
- '.icon' - dilog icon
- '.style' - Qt style sheet for dialog
- '.height', '.width', '.x', '.y' - dialog geometry
- '.label' - dialog message (can contain basic HTML)

```js
dialog(
  '.title', 'Command Finished',
  '.label', 'Command <b>successfully</b> finished.'
  )
```

Other arguments are used to get user input.

```js
var amount = dialog('.title', 'Amount?', 'Enter Amount', 'n/a')
var filePath = dialog('.title', 'File?', 'Choose File', new File('/home'))
```

If multiple inputs are required, object is returned.

```js
var result = dialog(
  'Enter Amount', 'n/a',
  'Choose File', new File(str(currentPath))
  )
print('Amount: ' + result['Enter Amount'] + '\n')
print('File: ' + result['Choose File'] + '\n')
```

###### Array settings()

Returns array with names of all custom options.

###### Value settings(optionName)

Returns value for an option.

###### settings(optionName)

Sets value for a new option or overrides existing option.

###### String dateString(format)

Returns text representation of current date and time.

See [QDateTime::toString()](http://doc.qt.io/qt-5/qdatetime.html#toString)
for details on formatting date and time.

Example:

```js
var now = dateString('yyyy-MM-dd HH:mm:ss')
```

###### NetworkReply networkGet(url)

Sends HTTP GET request.

Returns reply.

###### NetworkReply networkPost(url, postData)

Sends HTTP POST request.

Returns reply.

###### ByteArray env(name)

Returns value of environment variable with given name.

###### bool setEnv(name, value)

Sets environment variable with given name to given value.

Returns true only if the variable was set.

Types
-----

###### ByteArray

Simple wrapper around [QByteArray](http://doc.qt.io/qt-5/qbytearray.html).

`ByteArray` is used to store all item data (image data, HTML and even plain text).

Use `str()` to convert it to string.
Strings are usually more versatile.
For example to concatenate two items, the data need to be converted to strings first.

```js
var text = str(read(0)) + str(read(1))
```

###### File

Wrapper around [QFile](http://doc.qt.io/qt-5/qfile.html).

Following code reads contents of "README.md" file from current directory.

```js
var f = new File("README.md")
f.open()
var bytes = f.readAll()
```

###### Dir

Wrapper around [QDir](http://doc.qt.io/qt-5/qdir.html).

Objects
-------

###### arguments (Array)

Array for accessing arguments passed to current function or the script
(`arguments[0]` is the script itself).

###### Item (Object)

Type is `Object` and each property is MIME type with data.

Example:

```js
var item = {
    'text/plain': 'Hello, World!',
    'text/html': '<p>Hello, World!</p>'
}
write('application/x-copyq-item', pack(item))'
```

###### FinishedCommand (Object)

Type is `Object` and properties are:

- `stdout` - standard output
- `stderr` - standard error output
- `exit_code` - exit code

###### NetworkReply (Object)

Type is `Object` and properties are:

- `data` - reply data
- `error` - error string (set only if an error occurred)
- `redirect` - URL for redirection (set only if redirection is needed)
- `headers` - reply headers (array of pairs with header name and header content)

MIME Types
----------

Item and clipboard can provide multiple formats for their data.
Type of the data is determined by MIME type.

Here is list of some common and builtin (start with `application/x-copyq-`) MIME types.

Note: Content for following types is UTF-8 encoded.

###### text/plain

Data contains plain text content.

###### text/html

Data contains HTML content.

###### text/uri-list

Data contains list of links to files, web pages etc.

###### application/x-copyq-owner-window-title

Current window title for copied clipboard.

###### application/x-copyq-item

Serialized items.

###### application/x-copyq-item-notes

Data contains notes for item.

###### application/x-copyq-owner

If this type is available, the clipboard was set from CopyQ (from script or copied items).

###### application/x-copyq-clipboard-mode

Contains `selection` if data is from X11 mouse selection.

###### application/x-copyq-current-tab

Current tab name when invoking command from main window.

Following command print the tab name when invoked from main window.

    copyq data application/x-copyq-current-tab
    copyq selectedTab

###### application/x-copyq-selected-items

Selected items when invoking command from main window.

###### application/x-copyq-current-item

Current item when invoking command from main window.

###### application/x-copyq-hidden

If set to `1`, the clipboard content will be hidden in GUI.

E.g. if you run following, window title and tool tip will be cleared.

    copyq copy application/x-copyq-hidden 1 plain/text "This is secret"

###### application/x-copyq-shortcut

Application or global shortcut which activated the command.

    copyq:
    var shortcut = data("application/x-copyq-shortcut")
    popup("Shortcut Pressed", shortcut)

