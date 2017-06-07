Customize and Build the Windows Installer
=========================================

Translations
------------

Most of the translations for the installer are taken directly from the
installer generator Inno Setup (http://www.jrsoftware.org/isinfo.php).

You can add translations for CopyQ-specific messages in
``shared/copyq.iss``. Just copy lines starting with ``en.`` from
``[Custom Messages]`` section and change prefix to ``de.`` (for german
translation).

Modify and Test Installation
----------------------------

Normally the installation file is generated automatically by Appveyor
which executes
`appveyor-after-build.bat <https://github.com/hluk/CopyQ/blob/master/utils/appveyor-after-build.bat>`__
to generate portable app folder from build files and runs Inno Setup
(the last line).

So you basically don't have to build the app, you just need:
- the unzipped portable version of the app,
- clone of this repository and
- `Inno Setup <http://www.jrsoftware.org/isinfo.php>`__.

Open
`shared/copyq.iss <https://github.com/hluk/CopyQ/blob/master/shared/copyq.iss>`__
in Inno Setup and add few lines at the beginning of the file.

::

    #define AppVersion 2.8.1-beta
    #define Source C:\path\to\CopyQ-repository-clone
    #define Destination C:\path\to\CopyQ-portable

You should now be able to modify the file in Inno Setup and run it
easily.
