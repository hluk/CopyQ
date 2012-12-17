/*
    Copyright (c) 2012, Lukas Holecek <hluk@email.cz>

    This file is part of CopyQ.

    CopyQ is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CopyQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CopyQ.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "app.h"

#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

App::App(int &argc, char **argv)
    : m_app(argc, argv)
    , m_exitCode(0)
    , m_closed(false)
{
    QCoreApplication::setOrganizationName("copyq");
    QCoreApplication::setApplicationName("copyq");

    const QString locale = QLocale::system().name();
    QTranslator *translator = new QTranslator(this);
    translator->load("qt_" + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    QCoreApplication::installTranslator(translator);

    translator = new QTranslator(this);
    translator->load("copyq_" + locale, ":/translations");
    QCoreApplication::installTranslator(translator);
}

int App::exec()
{
    if (m_closed) {
        m_app.processEvents();
        return m_exitCode;
    } else {
        return m_app.exec();
    }
}

void App::exit(int exitCode)
{
    QCoreApplication::exit(exitCode);
    m_exitCode = exitCode;
    m_closed = true;
}
