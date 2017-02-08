#!/usr/bin/python
'''
Parses API for C++ from scriptable README file.
'''

import re

readme = 'src/scriptable/README.md'

def main():
    with open(readme) as f:
        api = ''
        for line in f.readlines():
            line = line.strip()
            if line.startswith('###### '):
                api = line[7:]
            elif api and line:
                m = re.match(r'.*?(\w+)\s*(\(|$)', api)
                if m:
                    name = m.group(1)
                    if api.startswith(name + ' '):
                        api = api[len(name) + 1:]
                    print('addDocumentation("{}", "{}", "{}");'
                            .format(name, api, line))
                api = ''

if __name__ == "__main__":
    main()
