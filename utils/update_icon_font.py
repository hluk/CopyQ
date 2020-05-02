#!/usr/bin/env python
'''
Updates icon font and header files for CopyQ repository.

First argument is path to upacked Font Awesome archive (https://fontawesome.com/).
'''
import json
import os
import sys

from shutil import copyfile
from textwrap import dedent

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

def write_icon_list_header_file(header_icon_list, icons):
    with open(header_icon_list, 'w') as header_file:
        items = []
        for style in [solid_style, brands_style]:
            is_brand = 'true' if style == brands_style else 'false'
            for name, icon in icons.items():
                if style in icon['styles']:
                    code = icon['unicode']
                    search_terms = [icon['label'].lower()] + (icon['search']['terms'] or [])
                    search_terms_list = '|'.join(search_terms)
                    items.append('{0x%s, %s, "%s"}' % (code, is_brand, search_terms_list))

        item_list_content = ',\n'.join(items)
        content = dedent('''\
            struct Icon {
                unsigned int unicode;
                bool isBrand;
                const char *searchTerms;
            };

            constexpr Icon iconList[] = {
            %s
            };
        ''') % item_list_content

        write_header_file_preamble(header_file)
        header_file.write(content)


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

    See: https://github.com/fonttools/fonttools/blob/master/Snippets/rename-fonts.py
    """

    font = TTFont(path)
    name_table = font['name']

    FAMILY_RELATED_IDS = dict(
        LEGACY_FAMILY=1,
        TRUETYPE_UNIQUE_ID=3,
        FULL_NAME=4,
        POSTSCRIPT_NAME=6,
        PREFERRED_FAMILY=16,
        WWS_FAMILY=21,
    )

    for rec in name_table.names:
        if rec.nameID not in FAMILY_RELATED_IDS.values():
            continue

        name = rec.toUnicode()
        if name.startswith('Font Awesome'):
            rec.string = name + ' (CopyQ)'
        elif name.startswith('FontAwesome'):
            rec.string = name + '(CopyQ)'

        assert rec.toUnicode().endswith('(CopyQ)')

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
    project_dir = os.path.dirname(utils_dir)
    src_dir = os.path.join(project_dir, 'src')

    header_icon_list = os.path.join(src_dir, 'gui', 'icon_list.h')
    header_icons = os.path.join(src_dir, 'gui', 'icons.h')

    target_font_dir = os.path.join(src_dir, 'images')
    copy_fonts(font_awesome_src, target_font_dir)

    icons_json = os.path.join(
            font_awesome_src, 'metadata', 'icons.json')
    icons = read_icons(icons_json)

    write_icon_list_header_file(header_icon_list, icons)
    print(f'Header file "{header_icon_list}" updated.')

    write_icons_header_file(header_icons, icons)
    print(f'Header file "{header_icons}" updated.')


if __name__ == '__main__':
    main()
