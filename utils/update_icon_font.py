#!/usr/bin/env python
'''
Updates icon font and header files for CopyQ repository.

First argument is path to upacked Font Awesome archive (https://fontawesome.com/).
'''
import json
import os
import sys

from shutil import copyfile

from fontTools.ttLib import TTFont

fonts_src_dest = [
    ('fa-solid-900.ttf', 'fontawesome-solid.ttf'),
    ('fa-brands-400.ttf', 'fontawesome-brands.ttf'),
]

solid_style = 'solid'
brands_style = 'brands'


def read_icons(icons_json):
    with open(icons_json, 'r') as icons_file:
        icons_content = icons_file.read()
        return json.loads(icons_content)


def write_header_file_preamble(header_file):
    script = os.path.realpath(__file__)
    script_name = os.path.basename(script)
    comment = (
            f'// This file is generated with "{script_name}"'
            + ' from FontAwesome\'s metadata.\n\n')
    header_file.write(comment)


def write_add_icons_header_file(header_add_icons, icons):
    with open(header_add_icons, 'w') as header_file:
        write_header_file_preamble(header_file)
        header_file.write('// List of method calls for IconSelectDialog.\n')

        for style in [solid_style, brands_style]:
            is_brand = 'true' if style == brands_style else 'false'
            for name, icon in icons.items():
                if style in icon['styles']:
                    code = icon['unicode']
                    search_terms = [icon['label'].lower()] + icon['search']['terms']
                    search_terms_list = (
                            'QStringList()'
                            + ''.join([' << ' + json.dumps(term) for term in search_terms]))
                    header_file.write(f'addIcon(0x{code}, {is_brand}, {search_terms_list});' + '\n')


def write_icons_header_file(header_icons, icons):
    with open(header_icons, 'w') as header_file:
        write_header_file_preamble(header_file)
        header_file.write('#ifndef ICONS_H\n')
        header_file.write('#define ICONS_H\n')
        header_file.write('\n')
        header_file.write('enum IconId {\n')

        for name, icon in icons.items():
            label = name.title().replace('-', '')
            code = icon['unicode']
            header_file.write(f'    Icon{label} = 0x{code},' + '\n')

        header_file.write('};\n')
        header_file.write('\n')
        header_file.write('#endif // ICONS_H\n')


def rename_font_family(path):
    """
    Adds suffix to font family it doesn't conflict with font installed on
    system, which could be in incorrect version.
    """
    font = TTFont(path)
    name_table = font['name']

    name = name_table.getName(nameID=1, platformID=3, platEncID=1, langID=0x409)
    assert name

    name = name.toUnicode()
    assert name.startswith('Font Awesome')

    name = name + ' (CopyQ)'
    name = name_table.setName(name, nameID=1, platformID=3, platEncID=1, langID=0x409)

    font.save(path)


def copy_fonts(font_awesome_src, target_font_dir):
    font_dir = os.path.join(font_awesome_src, 'webfonts')
    for src_name, dest_name in fonts_src_dest:
        src_path = os.path.join(font_dir, src_name)
        dest_path = os.path.join(target_font_dir, dest_name)
        print(f'Copying: {src_path} -> {dest_path}')
        copyfile(src_path, dest_path)
        rename_font_family(dest_path)


def main():
    font_awesome_src = sys.argv[1]

    script = os.path.realpath(__file__)
    utils_dir = os.path.dirname(script)
    src_dir = os.path.join(utils_dir, '..', 'src')

    header_add_icons = os.path.join(src_dir, 'gui', 'add_icons.h')
    header_icons = os.path.join(src_dir, 'gui', 'icons.h')

    target_font_dir = os.path.join(src_dir, 'images')
    copy_fonts(font_awesome_src, target_font_dir)

    icons_json = os.path.join(
            font_awesome_src, 'metadata', 'icons.json')
    icons = read_icons(icons_json)

    write_add_icons_header_file(header_add_icons, icons)
    print(f'Header file "{header_add_icons}" updated.')

    write_icons_header_file(header_icons, icons)
    print(f'Header file "{header_icons}" updated.')


if __name__ == '__main__':
    main()
