# CopyQ Scripting

CopyQ provides scripting capabilities to automatically handle clipboard changes,
organize items, change settings and much more.

In addition to features provided by Qt Script there are following
[functions](#functions), [types](#types), [objects](#objects) and
[useful MIME types](#mime-types).

## Execute Script

The scripts can be executed:

- from commands (in Action or Command dialogs -- <kbd>F5</kbd>, <kbd>F6</kbd> shortcuts) when prefixed with `copyq:`,
- from command line as `copyq eval '<SCRIPT>'`,
- from command line as `cat script.js | copyq eval -`,
- from command line as `copyq <SCRIPT_FUNCTION> <FUNCTION_ARGUMENT_1> <FUNCTION_ARGUMENT_2> ...`.

When run from command line, result of last expression is printed on stdout.

Command exit values are:

- 0 - script finished without error,
- 1 - `fail()` was called,
- 2 - bad syntax,
- 3 - exception was thrown.

## Command Line

If number of arguments that can be passed to function is limited you can use

```bash
copyq <FUNCTION1> <FUNCTION1_ARGUMENT_1> <FUNCTION1_ARGUMENT_2> \
          <FUNCTION2> <FUNCTION2_ARGUMENT> \
              <FUNCTION3> <FUNCTION3_ARGUMENTS> ...
```

where `<FUNCTION1>` and `<FUNCTION2>` are scripts where result of last expression is functions that take two and one arguments respectively.

E.g.

```bash
copyq tab clipboard separator "," read 0 1 2
```

After `eval` no arguments are treated as functions since it can access all arguments.

Arguments recognize escape sequences `\n` (new line), `\t` (tabulator character) and `\\` (backslash).

Argument `-e` is identical to `eval`.

Argument `-` is replaced with data read from stdin.

Argument `--` is skipped and all the remaining arguments are interpreted as they are (escape sequences are ignored and `-e`, `-`, `--` are left unchanged).

### Functions

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

###### menu(tabName, [maxItemCount, [x, y]])

Shows context menu for given tab.

This menu doesn't show clipboard and doesn't have any special actions.

Second argument is optional maximum number of items.
The default value same as for tray (i.e. value of `config('tray_items')`).

Optional arguments x, y are coordinates in pixels on screen where menu should show up.
By default menu shows up under the mouse cursor.

###### exit()

Exits server.

###### disable(), enable()

Disables or enables clipboard content storing.

###### bool monitoring()

Returns true only if clipboard storing is enabled.

###### bool visible()

Returns true only if main window is visible.

###### bool focused()

Returns true only if main window has focus.

###### filter(filterText)

Sets text for filtering items in main window.

###### ignore()

Ignores current clipboard content (used for automatic commands).

This does all of the below.

- Skips any next automatic commands.
- Omits changing window title and tray tool tip.
- Won't store content in clipboard tab.

###### ByteArray clipboard([mimeType])

Returns clipboard data for MIME type (default is text).

Pass argument `"?"` to list available MIME types.

###### ByteArray selection([mimeType])

Same as `clipboard()` for Linux/X11 mouse selection.

###### bool hasClipboardFormat(mimeType)

Returns true only if clipboard contains MIME type.

###### bool hasSelectionFormat(mimeType)

Same as `hasClipboardFormat()` for Linux/X11 mouse selection.

###### bool copy(text)

Sets clipboard plain text.

Same as `copy(mimeText, text)`.

###### bool copy(mimeType, data, [mimeType, data]...)

Sets clipboard data.

This also sets `mimeOwner` format so automatic commands are not run on the new data
and it's not store in clipboard tab.

Exception is thrown if clipboard fails to be set.

Example (set both text and rich text):

```js
copy(mimeText, 'Hello, World!',
     mimeHtml, '<p>Hello, World!</p>')
```

###### bool copy()

Sends `Ctrl+C` to current window.

Exception is thrown if clipboard doesn't change
(clipboard is reset before sending the shortcut).

###### ByteArray copySelection(...)

Same as `copy(...)` for Linux/X11 mouse selection.

###### paste()

Pastes current clipboard.

This is basically only sending `Shift+Insert` shortcut to current window.

Correct functionality depends a lot on target application and window manager.

###### Array tab()

Returns array of with tab names.

###### tab(tabName)

Sets current tab for the script.

E.g. following script selects third item (index is 2) from tab "Notes".

```js
tab('Notes')
select(2)
```

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

Throws an exception if space for the items cannot be allocated.

###### insert(row, text)

Inserts new text items to current tab.

###### remove(row, ...)

Removes items in current tab.

Throws an exception if some items cannot be removed.

###### edit([row|text] ...)

Edits items in current tab.

Opens external editor if set, otherwise opens internal editor.

###### ByteArray read([mimeType]);

Same as `clipboard()`.

###### ByteArray read(mimeType, row, ...);

Returns concatenated data from items.

Pass argument `"?"` to list available MIME types.

###### write(row, mimeType, data, [mimeType, data]...)

Inserts new item to current tab.

Throws an exception if space for the items cannot be allocated.

###### change(row, mimeType, data, [mimeType, data]...)

Changes data in item in current tab.

If data is `undefined` the format is removed from item.

###### String separator()

Returns item separator (used when concatenating item data).

###### separator(separator)

Sets item separator for concatenating item data.

###### action()

Opens action dialog.

###### action(row, ..., command, outputItemSeparator)

Runs command for items in current tab.

###### popup(title, message, [time=8000])

Shows popup message for given time in milliseconds.

If `time` argument is set to -1, the popup is hidden only after mouse click.

###### notification(...)

Shows popup message with icon and buttons.

Each button can have script and data.

If button is clicked the notification is hidden and script is executed with the data passed as stdin.

The function returns immediatelly (doesn't wait on user input).

Special arguments:

- '.title' - notification title
- '.message' - notification message (can contain basic HTML)
- '.icon' - notification icon (path to image or font icon)
- '.id' - notification ID - this replaces notification with same ID
- '.time' - duration of notification in milliseconds (default is -1, i.e. waits for mouse click)
- '.button' - adds button (three arguments: name, script and data)

Example:

```js
notification(
      '.title', 'Example',
      '.message', 'Notification with button',
      '.button', 'Cancel', '', '',
      '.button', 'OK', 'copyq:popup(input())', 'OK Clicked'
      )
```

###### exportTab(fileName)

Exports current tab into file.

###### importTab(fileName)

Imports items from file to a new tab.

###### String config()

Returns help with list of available options.

###### String config(optionName)

Returns value of given option.

Throws an exception if the option is invalid.

###### String config(optionName, value)

Sets option and returns new value.

Throws an exception if the option is invalid.

###### String config(optionName, value, ...)

Sets multiple options and return list with values in format `optionName=newValue`.

Throws an exception if there is an invalid option in which case it won't set any options.

###### String info([pathName])

Returns paths and flags used by the application.

E.g. following command prints path to configuration file.

```bash
copyq info config
```

###### Value eval(script)

Evaluates script and returns result.

###### Value source(fileName)

Evaluates script file and returns result of last expression in the script.

This is useful to move some common code out of commands.

```js
// File: c:/copyq/replace_clipboard_text.js
replaceClipboardText = function(replaceWhat, replaceWith)
{
    var text = str(clipboard())
    var newText = text.replace(replaceWhat, replaceWith)
    if (text != newText)
        copy(newText)
}
```

```js
source('c:/copyq/replace_clipboard_text.js')
replaceClipboardText('secret', '*****')
```

###### String currentPath([path])

Get or set current path.

###### String str(value)

Converts a value to string.

If ByteArray object is the argument, it assumes UTF8 encoding.
To use different encoding, use `toUnicode()`.

###### ByteArray input()

Returns standard input passed to the script.

###### String toUnicode(ByteArray, encodingName)

Returns string for bytes with given encoding.

###### String toUnicode(ByteArray)

Returns string for bytes with encoding detected by checking Byte Order Mark (BOM).

###### ByteArray fromUnicode(String, encodingName)

Returns encoded text.

###### ByteArray data(mimeType)

Returns data for automatic commands or selected items.

If run from menu or using non-global shortcut the data are taken from selected items.

If run for automatic command the data are clipboard content.

###### ByteArray setData(mimeType, data)

Modifies data for `data()` and new clipboard item.

Next automatic command will get updated data.

This is also the data used to create new item from clipboard.

E.g. following automatic command will add creation time data and tag to new items.

    copyq:
    var timeFormat = 'yyyy-MM-dd hh:mm:ss'
    setData('application/x-copyq-user-copy-time', dateString(timeFormat))
    setData(mimeTags, 'copied: ' + time)

E.g. following menu command will add tag to selected items.

    copyq:
    setData('application/x-copyq-tags', 'Important')

###### ByteArray removeData(mimeType)

Removes data for `data()` and new clipboard item.

###### Array dataFormats()

Returns formats available for `data()`.

###### print(value)

Prints value to standard output.

###### abort()

Aborts script evaluation.

###### fail()

Aborts script evaluation with nonzero exit code.

###### setCurrentTab(tabName)

Focus tab without showing main window.

###### selectItems(row, ...)

Selects items in current tab.

###### String selectedTab()

Returns tab that was selected when script was executed.

See [Selected Items](#selected-items).

###### [row, ...] selectedItems()

Returns selected rows in current tab.

See [Selected Items](#selected-items).

###### Item selectedItemData(index)

Returns data for given selected item.

The data can empty if the item was removed during execution of the script.

See [Selected Items](#selected-items).

###### bool setSelectedItemData(index, Item)

Set data for given selected item.

Returns false only if the data cannot be set, usually if item was removed.

See [Selected Items](#selected-items).

###### Item[] selectedItemsData()

Returns data for all selected item.

Some data can empty if the item was removed during execution of the script.

See [Selected Items](#selected-items).

###### void setSelectedItemsData(Item[])

Set data to all selected items.

Some data may not be set if the item was removed during execution of the script.

See [Selected Items](#selected-items).

###### int currentItem(), int index()

Returns current row in current tab.

See [Selected Items](#selected-items).

###### String escapeHtml(text)

Returns text with special HTML characters escaped.

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

If arguments is function it will be called with array of lines read from stdout whenever available.

E.g. create item for each line on stdout:

```js
execute('tail', '-f', 'some_file.log',
        function(lines) { add.apply(this, lines) })
```

Returns object for the finished command or `undefined` on failure.

###### String currentWindowTitle()

Returns window title of currently focused window.

###### Value dialog(...)

Shows messages or asks user for input.

Arguments are names and associated values.

Special arguments:

- '.title' - dialog title
- '.icon' - dialog icon (see below for more info)
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

Editable combo box can be created by passing array.
Current value can be provided using `.defaultChoice` (by default it's the first item).

```js
var text = dialog('.defaultChoice', '', 'Select', ['a', 'b', 'c'])
```

List can be created by prefixing name/label with `.list:` and passing array.

```js
var items = ['a', 'b', 'c']
var selected_index = dialog('.list:Select', items)
if (selected_index)
    print('Selected item: ' + items[selected_index])
```

Icon for custom dialog can be set from icon font, file path or theme.
Icons from icon font can be copied from icon selection dialog in Command dialog or
dialog for setting tab icon (in menu 'Tabs/Change Tab Icon').

```js
var search = dialog(
  '.title', 'Search',
  '.icon', 'search', // Set icon 'search' from theme.
  'Search', ''
  )
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

###### Command[] commands()

Return list of all commands.

###### setCommands(Command[])

Clear previous commands and set new ones.

To add new command:

```js
var cmds = commands()
cmds.unshift({
        name: 'New Command',
        automatic: true,
        input: 'text/plain',
        cmd: 'copyq: popup("Clipboard", input())'
        })
setCommands(cmds)
```

###### Command[] importCommands(String)

Return list of commands from exported commands text.

###### String exportCommands(Command[])

Return exported command text.

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

###### sleep(time)

Wait for given time in milliseconds.

###### ByteArray screenshot(format='png', [screenName])

Returns image data with screenshot.

Example:

```js
copy('image/png', screenshot())
```

###### ByteArray screenshotSelect(format='png', [screenName])

Same as `screenshot()` but allows to select an area on screen.

### Types

###### ByteArray

Wrapper for QByteArray Qt class.

See [QByteArray](http://doc.qt.io/qt-5/qbytearray.html).

`ByteArray` is used to store all item data (image data, HTML and even plain text).

Use `str()` to convert it to string.
Strings are usually more versatile.
For example to concatenate two items, the data need to be converted to strings first.

```js
var text = str(read(0)) + str(read(1))
```

###### File

Wrapper for QFile Qt class.

See [QFile](http://doc.qt.io/qt-5/qfile.html).

Following code reads contents of "README.md" file from current directory.

```js
var f = new File("README.md")
f.open()
var bytes = f.readAll()
```

###### Dir

Wrapper for QDir Qt class.

See [QDir](http://doc.qt.io/qt-5/qdir.html).

###### TemporaryFile

Wrapper for QTemporaryFile Qt class.

See [QTemporaryFile](https://doc.qt.io/qt-5/qtemporaryfile.html).

```js
var f = new TemporaryFile()
f.open()
f.setAutoRemove(false)
popup('New temporary file', f.fileName())
```

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
    mimeText: 'Hello, World!',
    mimeHtml: '<p>Hello, World!</p>'
}
write(mimeItems, pack(item))'
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

###### Command (Object)

Wrapper for a command (from Command dialog).

Properties are same as members of
[Command struct](https://github.com/hluk/CopyQ/blob/master/src/common/command.h).

### MIME Types

Item and clipboard can provide multiple formats for their data.
Type of the data is determined by MIME type.

Here is list of some common and builtin (start with `application/x-copyq-`) MIME types.

These MIME types values are assigned to global variables prefixed with `mime`.

Note: Content for following types is UTF-8 encoded.

###### mimeText (text/plain)

Data contains plain text content.

###### mimeHtml (text/html)

Data contains HTML content.

###### mimeUriList (text/uri-list)

Data contains list of links to files, web pages etc.

###### mimeWindowTitle (application/x-copyq-owner-window-title)

Current window title for copied clipboard.

###### mimeItems (application/x-copyq-item)

Serialized items.

###### mimeItemNotes (application/x-copyq-item-notes)

Data contains notes for item.

###### mimeOwner (application/x-copyq-owner)

If available, the clipboard was set from CopyQ (from script or copied items).

Such clipboard is ignored in CopyQ, i.e. it won't be stored in clipboard tab and
automatic commands won't be executed on it.

###### mimeClipboardMode (application/x-copyq-clipboard-mode)

Contains `selection` if data is from X11 mouse selection.

###### mimeCurrentTab (application/x-copyq-current-tab)

Current tab name when invoking command from main window.

Following command print the tab name when invoked from main window.

    copyq data application/x-copyq-current-tab
    copyq selectedTab

###### mimeSelectedItems (application/x-copyq-selected-items)

Selected items when invoking command from main window.

###### mimeCurrentItem (application/x-copyq-current-item)

Current item when invoking command from main window.

###### mimeHidden (application/x-copyq-hidden)

If set to `1`, the clipboard or item content will be hidden in GUI.

This won't hide notes and tags.

E.g. if you run following, window title and tool tip will be cleared.

    copyq copy application/x-copyq-hidden 1 plain/text "This is secret"

###### mimeShortcut (application/x-copyq-shortcut)

Application or global shortcut which activated the command.

    copyq:
    var shortcut = data(mimeShortcut)
    popup("Shortcut Pressed", shortcut)

###### mimeColor (application/x-copyq-color)

Item color (same as the one used by themes).

Examples:
        #ffff00
        rgba(255,255,0,0.5)
        bg - #000099

###### mimeOutputTab (application/x-copyq-output-tab)

Name of the tab where to store new item.

The clipboard data will be stored in tab with this name after all automatic commands are run.

Clear or remove the format to omit storing the data.

E.g. to omit storing the clipboard data use following in an automatic command.

```js
removeData(mimeOutputTab)
```

Valid only in automatic commands.

###### mimeSyncToClipboard (application/x-copyq-sync-to-selection)

If exists the X11 selection data will be copied to clipboard.

The synchronization will happend after all automatic commands are run.

```js
removeData(mimeSyncToClipboard)
```

Valid only in Linux/X11 in automatic commands.

###### mimeSyncToSelection (application/x-copyq-sync-to-clipboard)

If exists the clipboard data will be copied to X11 selection.

The synchronization will happend after all automatic commands are run.

```js
removeData(mimeSyncToSelection)
```

Valid only in Linux/X11 in automatic commands.

### Selected Items

Functions that get and set data for selected items and current tab are only
available if called from Action dialog or from a command which is in menu.

Selected items are indexed from top to bottom as they appeared in the current
tab at the time the command is executed.

