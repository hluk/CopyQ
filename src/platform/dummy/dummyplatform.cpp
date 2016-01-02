/*
    Copyright (c) 2016, Lukas Holecek <hluk@email.cz>

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

#include "dummyplatform.h"

#include "dummyclipboard.h"

#include <QApplication>
#include <QCoreApplication>

PlatformPtr createPlatformNativeInterface()
{
    return PlatformPtr(new DummyPlatform);
}

QApplication *DummyPlatform::createServerApplication(int &argc, char **argv)
{
    return new QApplication(argc, argv);
}

QApplication *DummyPlatform::createMonitorApplication(int &argc, char **argv)
{
    return new QApplication(argc, argv);
}

QCoreApplication *DummyPlatform::createClientApplication(int &argc, char **argv)
{
    return new QCoreApplication(argc, argv);
}

PlatformClipboardPtr DummyPlatform::clipboard()
{
    return PlatformClipboardPtr(new DummyClipboard());
}
