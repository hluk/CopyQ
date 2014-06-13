#!/usr/bin/env python
# encoding: utf-8

"""
Settings file for dmgbuild.

Use like this: dmgbuild -Dapp_path=/path/to/copqy.app -s /path/to/dmg_settings.py "CopyQ 2.2" copyq22.dmg

http://dmgbuild.readthedocs.org/en/latest/settings.html
"""

from __future__ import unicode_literals


import os

def get_size(start_path = '.'):
    total_size = 0
    for dirpath, dirnames, filenames in os.walk(start_path):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            total_size += os.path.getsize(fp)
    return total_size

app_path = defines['app_path']
app_name = os.path.basename(app_path)

# Volume format (see hdiutil create -help)
format = "UDBZ"

# Volume size (must be large enough for your files)
_copyq_size_mb = get_size(app_path) / 1024 / 1024
size = "%sm" % (_copyq_size_mb + 30)

# Files and symlinks to include
files = [app_path]
symlinks = { 'Applications': '/Applications' }

# Icon
badge_icon = "%s/Contents/Resources/icon.icns" % app_path

# Display settings

icon_locations = {
    app_name: (140, 120),
    'Applications': (500, 120)
}

# Window position in ((x, y), (w, h)) format
window_rect = ((100, 100), (640, 280))

background = 'builtin-arrow'
