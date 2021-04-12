#!/usr/bin/env python
"""
Create new release and upload files to FossHUB.

USAGE: ./fosshub.py <VERSION> <API_KEY>
"""
from requests import post
import sys

version = sys.argv[1],

project_id = '5c1195728c9fe8186f80a14b'
fosshub_new_release_url = f'https://api.fosshub.com/rest/projects/{project_id}/releases/'
files = {
    'copyq-v{version}-setup.exe': 'Windows Installer',
    'copyq-v{version}.zip': 'Windows Portable',
    'CopyQ.dmg.zip': 'macOS'
}
# https://devzone.fosshub.com/dashboard/restApi
data = {
    'version': version,
    'files': [{
        'fileUrl': f'https://github.com/hluk/CopyQ/releases/download/v{version}/{basename}',
        'type': filetype,
        'version': version
    } for basename, filetype in files.items()],
    'publish': True,
}
headers = {
    'X-Auth-Key': sys.argv[2]
}

response = post(fosshub_new_release_url, json=data, headers=headers)
if response.status_code != 200:
    raise RuntimeError('Unexpected response: ', response.text)

print('All OK: ', response.text)
