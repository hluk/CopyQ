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

#include "common/contenttype.h"
#include "common/client_server.h"
#include "itemwidget.h"

#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QModelIndex>
#include <QMutex>
#include <QMutexLocker>
#include <QPluginLoader>

namespace {

const int dummyItemMaxChars = 4096;

bool priorityLessThan(const ItemLoaderInterface *lhs, const ItemLoaderInterface *rhs)
{
    return lhs->priority() > rhs->priority();
}

class DummyItem : public QLabel, public ItemWidget {
public:
    DummyItem(const QModelIndex &index, QWidget *parent)
        : QLabel(parent)
        , ItemWidget(this)
    {
        setMargin(4);
        setWordWrap(true);
        setTextFormat(Qt::PlainText);
        setText( index.data(contentType::text).toString().left(dummyItemMaxChars) );
        updateSize();
    }

protected:
    virtual void updateSize()
    {
        setMinimumWidth(maximumWidth());
        adjustSize();
    }
};

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
    : m_loaders()
    , m_loaderChildren()
{ 
    loadPlugins();

    if ( m_loaders.isEmpty() )
        log( QObject::tr("No plugins loaded"), LogNote );
}

ItemWidget *ItemFactory::createItem(ItemLoaderInterface *loader,
                                    const QModelIndex &index, QWidget *parent)
{
    if (loader == NULL || loader->isEnabled()) {
        ItemWidget *item = (loader == NULL) ? new DummyItem(index, parent)
                                           : loader->create(index, parent);
        if (item != NULL) {
            QWidget *w = item->widget();
            QString notes = index.data(contentType::notes).toString();
            if (!notes.isEmpty())
                w->setToolTip(notes);

            m_loaderChildren[w] = loader;
            connect(w, SIGNAL(destroyed(QObject*)), SLOT(loaderChildDestroyed(QObject*)));
            return item;
        }
    }

    return NULL;
}

ItemWidget *ItemFactory::createItem(const QModelIndex &index, QWidget *parent)
{
    foreach (ItemLoaderInterface *loader, m_loaders) {
        ItemWidget *item = createItem(loader, index, parent);
        if (item != NULL)
            return item;
    }

    return createItem(NULL, index, parent);
}

ItemWidget *ItemFactory::nextItemLoader(const QModelIndex &index, ItemWidget *current)
{
    return otherItemLoader(index, current, 1);
}

ItemWidget *ItemFactory::previousItemLoader(const QModelIndex &index, ItemWidget *current)
{
    return otherItemLoader(index, current, -1);
}

QStringList ItemFactory::formatsToSave() const
{
    QStringList formats;

    foreach (const ItemLoaderInterface *loader, m_loaders) {
        if (loader->isEnabled()) {
            foreach ( const QString &format, loader->formatsToSave() ) {
                if ( !formats.contains(format) )
                    formats.append(format);
            }
        }
    }

    if ( !formats.contains("text/plain") )
        formats.prepend("text/plain");

    return formats;
}

void ItemFactory::setPluginPriority(const QStringList &pluginNames)
{
    int a = -1;
    int b = -1;
    int j = -1;
    for (int i = 0; i < m_loaders.size(); ++i) {
        b = pluginNames.indexOf( m_loaders[i]->name() );
        if (b != -1) {
            if (a > b) {
                // swap and restart
                qSwap(m_loaders[j], m_loaders[i]);
                i = 0;
                a = b = -1;
            } else {
                a = b;
                j = i;
            }
        }
    }
}

void ItemFactory::loaderChildDestroyed(QObject *obj)
{
    m_loaderChildren.remove(obj);
}

ItemWidget *ItemFactory::otherItemLoader(const QModelIndex &index, ItemWidget *current, int dir)
{
    Q_ASSERT(dir == -1 || dir == 1);
    Q_ASSERT(current->widget() != NULL);

    QWidget *w = current->widget();
    ItemLoaderInterface *currentLoader = m_loaderChildren[w];
    if (currentLoader == NULL)
        return NULL;

    const int currentIndex = m_loaders.indexOf(currentLoader);
    Q_ASSERT(currentIndex != -1);

    const int size = m_loaders.size();
    for (int i = currentIndex + dir; i != currentIndex; i = i + dir) {
        if (i >= size)
            i = i % size;
        else if (i < 0)
            i = size - 1;

        ItemWidget *item = createItem(m_loaders[i], index, w->parentWidget());
        if (item != NULL)
            return item;
    }

    return NULL;
}

bool ItemFactory::loadPlugins()
{
#if defined(COPYQ_WS_X11)
#   ifdef COPYQ_PLUGIN_PREFIX
    QDir pluginsDir(COPYQ_PLUGIN_PREFIX);
#   else
    QDir pluginsDir( QCoreApplication::instance()->applicationDirPath() );
    if ( pluginsDir.dirName() == QString("bin")
         && (!pluginsDir.cdUp() || !pluginsDir.cd("lib") || !pluginsDir.cd("copyq")) )
    {
         return false;
    }
#   endif
#elif defined(Q_OS_MAC)
    QDir pluginsDir( QCoreApplication::instance()->applicationDirPath() );
    if ( pluginsDir.dirName() == "MacOS"
         && (!pluginsDir.cdUp() || !pluginsDir.cdUp() || !pluginsDir.cdUp()) )
    {
        return false;
    }
#else
    QDir pluginsDir( QCoreApplication::instance()->applicationDirPath() );
    if ( !pluginsDir.cd("plugins") )
        return false;
#endif

    if ( !pluginsDir.isReadable() )
        return false;

    foreach (QString fileName, pluginsDir.entryList(QDir::Files)) {
        if ( QLibrary::isLibrary(fileName) ) {
            const QString path = pluginsDir.absoluteFilePath(fileName);
            QPluginLoader pluginLoader(path);
            QObject *plugin = pluginLoader.instance();
            log( QObject::tr("Loading plugin: %1").arg(path), LogNote );
            if (plugin == NULL) {
                log( pluginLoader.errorString(), LogError );
            } else {
                ItemLoaderInterface *loader = qobject_cast<ItemLoaderInterface *>(plugin);
                if (loader == NULL)
                    pluginLoader.unload();
                else
                    m_loaders.append(loader);
            }
        }
    }

    qSort(m_loaders.begin(), m_loaders.end(), priorityLessThan);

    return true;
}
