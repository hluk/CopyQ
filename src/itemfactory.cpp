/*
    Copyright (c) 2013, Lukas Holecek <hluk@email.cz>

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

#include "itemfactory.h"

#include "client_server.h"
#include "itemwidget.h"

#include <QCoreApplication>
#include <QDir>
#include <QModelIndex>
#include <QMutex>
#include <QMutexLocker>
#include <QPluginLoader>

namespace {

bool priorityLessThan(const ItemLoaderInterface *lhs, const ItemLoaderInterface *rhs)
{
    return lhs->priority() > rhs->priority();
}

} // namespace

// singleton
ItemFactory* ItemFactory::m_Instance = 0;

ItemFactory *ItemFactory::instance()
{
    static QMutex mutex;

    if ( !hasInstance() ) {
        QMutexLocker lock(&mutex);
        if ( !hasInstance() )
            m_Instance = new ItemFactory();
    }

    return m_Instance;
}

ItemFactory::ItemFactory()
{
    QDir pluginsDir( QCoreApplication::instance()->applicationDirPath() );
#if defined(Q_WS_X11)
    if ( pluginsDir.dirName() == QString("bin") ) {
        pluginsDir.cdUp();
        pluginsDir.cd("lib");
        pluginsDir.cd("copyq");
    }
#elif defined(Q_OS_MAC)
    if (pluginsDir.dirName() == "MacOS") {
        pluginsDir.cdUp();
        pluginsDir.cdUp();
        pluginsDir.cdUp();
    }
#endif

    if ( pluginsDir.cd("plugins") ) {
        foreach (QString fileName, pluginsDir.entryList(QDir::Files)) {
            const QString path = pluginsDir.absoluteFilePath(fileName);
            QPluginLoader pluginLoader(path);
            QObject *plugin = pluginLoader.instance();
            if (plugin == NULL) {
                log( pluginLoader.errorString(), LogError );
            } else {
                log( QObject::tr("Loading plugin: %1").arg(path), LogNote );
                ItemLoaderInterface *loader = qobject_cast<ItemLoaderInterface *>(plugin);
                if (loader != NULL)
                    m_loaders.append(loader);
            }
        }

        qSort(m_loaders.begin(), m_loaders.end(), priorityLessThan);
    }

    if ( m_loaders.isEmpty() )
        log( QObject::tr("No plugins loaded!"), LogWarning );
}

ItemWidget *ItemFactory::createItem(const QModelIndex &index, QWidget *parent) const
{
    foreach (ItemLoaderInterface *loader, m_loaders) {
        ItemWidget *item = loader->create(index, parent);
        if (item != NULL)
            return item;
    }

    return NULL;
}
