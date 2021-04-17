Scripting
=========

If you need to process items in some non-trivial way you can take
advantage of the scripting interface the application provides. This is
accessible on command line as ``copyq eval SCRIPT`` or
``copyq -e SCRIPT`` where ``SCRIPT`` is string containing commands
written in JavaScript-similar scripting language (see :ref:`scripting-api`).

Every command line option is available as function in the scripting
interface. Command ``copyq help tab`` can be written as
``copyq eval 'print(help("tab"))'`` (note: ``print`` is needed to print
the return value of ``help("tab")`` function call).

Searching Items
---------------

You can print each item with ``copyq read N`` where N is item number
from 0 to ``copyq size`` (i.e. number of items in the first tab) and put
item to clipboard with ``copyq select N``. With these commands it's
possible to search items and copy the right one with a script. E.g.
having file ``script.js`` containing

::

    var match = "MATCH-THIS";
    var i = 0;
    while (i < size() && str(read(i)).indexOf(match) === -1)
        ++i;
    select(i);

and passing it to CopyQ using ``cat script.js | copyq eval -`` will put
first item containing "MATCH-THIS" string to clipboard.

Working with Tabs
-----------------

By default commands and functions work with items in the first tab.
Calling ``read(0, 1, 2)`` will read first three items from the first
tab. To access items in other tab you need to switch the current tab
with ``tab("TAB_NAME")`` (or ``copyq tab TAB_NAME`` on command line)
where ``TAB_NAME`` is name of the tab.

For example to search for an item as in the previous script but in all
tabs you'll have to run:

::

    var match = "MATCH-THIS";
    var tabs = tab();
    for (var i in tabs) {
        tab(tabs[i]);
        var j = 0;
        while (j < size() && str(read(j)).indexOf(match) === -1)
            ++j;
        if (j < size())
            print("Match in tab \"" + tabs[i] + "\" item number " + j + ".\n");
    }

Scripting Functions
-------------------

As mentioned above, all command line options are also available for
scripting e.g.: ``show()``, ``hide()``, ``toggle()``, ``copy()``,
``paste()``.

Reference for available scripting functions and language features can be found
at :ref:`scripting-api`.
