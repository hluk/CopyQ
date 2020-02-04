#!/usr/bin/env python3
"""
One time script to fix window title translation strings in CopyQ.

Removes unneeded application from window titles from translation files.

The application name is added to the title by using
QGuiApplication::setApplicationDisplayName().
"""
from glob import glob
import re

import xml.etree.ElementTree as ET


def main():
    prefix = re.compile('^CopyQ\\s*')
    suffix = re.compile('\\s*CopyQ$')
    sources = {
        "CopyQ About",
        "CopyQ Action Dialog",
        "CopyQ Add Commands",
        "CopyQ Clipboard Content",
        "CopyQ Commands",
        "CopyQ Configuration",
        "CopyQ Export Error",
        "CopyQ Import Error",
        "CopyQ Item Content",
        "CopyQ Log",
        "CopyQ New Tab",
        "CopyQ New Shortcut",
        "CopyQ Options for Export",
        "CopyQ Options for Import",
        "CopyQ Process Manager",
        "CopyQ Rename Tab",
        "CopyQ Rename Tab Group",
        "CopyQ Select Icon",
    }

    for fn in glob('translations/*.ts'):
        tree = ET.parse(fn)
        root = tree.getroot()
        for context in root.iter('context'):
            for message in context.iter('message'):
                source = message.find('source')
                if source.text in sources:
                    tr = message.find('translation')
                    if not tr.text:
                        continue

                    source.text = re.sub(prefix, '', source.text)
                    tr.text = re.sub(prefix, '', tr.text)
                    tr.text = re.sub(suffix, '', tr.text)
                    if tr.text[0].islower():
                        tr.text = tr.text[0].upper() + tr.text[1:]
                    tr.set('type', 'unfinished')

        tree.write(fn)


if __name__ == "__main__":
    main()
