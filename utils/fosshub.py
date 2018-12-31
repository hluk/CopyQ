#!/usr/bin/env python
"""
Create new release and upload files to FossHUB.

USAGE: ./fosshub.py <VERSION> <API_KEY>
"""
import requests
import sys

project_id = '5c1195728c9fe8186f80a14b'
fosshub_new_release_url = 'https://api.fosshub.com/rest/projects/{project_id}/releases/'.format(project_id=project_id)
github_release_url = 'https://github.com/hluk/CopyQ/releases/download/v{version}/{basename}'
files = {
    'copyq-v{version}-setup.exe': 'Windows Installer',
    'copyq-v{version}.zip': 'Windows Portable',
    'CopyQ.dmg': 'macOS',
}

version = sys.argv[1]
api_key = sys.argv[2]

# https://devzone.fosshub.com/dashboard/restApi
data = {
    'version': version,
    'files': [{
        'fileUrl': github_release_url.format(version=version, basename=basename.format(version=version)),
        'type': filetype,
        'version': version
    } for basename, filetype in files.items()],
    'publish': True,
}
headers = {
    'X-Auth-Key': api_key
}

response = requests.post(fosshub_new_release_url, json=data, headers=headers)
if response.status_code != 200:
    raise RuntimeError('Unexpected response: ' + response.text)

print('All OK: ' + response.text)
