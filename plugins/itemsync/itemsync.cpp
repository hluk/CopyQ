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

#include "itemsync.h"
#include "ui_itemsyncsettings.h"

#include "common/common.h"
#include "common/contenttype.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QFileDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScopedPointer>
#include <QTextEdit>
#include <QTimer>
#include <QtPlugin>
#include <QUrl>
#include <QVariantMap>

namespace {

const int currentVersion = 1;

const char configVersion[] = "copyq_itemsync_version";
const char configPath[] = "path";
const char configSavedFiles[] = "saved_files";

const char dataFileSuffix[] = "_data.dat";

const char mimeBaseName[] = "application/x-copyq-itemsync-basename";
const char mimeUnknownData[] = "application/octet-stream";
const char mimeNoSave[] = "application/x-copyq-itemsync-no-save";

const char propertyModelDisabled[] = "disabled";
const char propertyModelDirty []= "dirty";

const QDir::Filters itemFileFilter = QDir::Files | QDir::Readable | QDir::Writable;

const int updateItemsIntervalMs = 2000; // Interval to update items after a file has changed.

const bool saveUnknownData = false;
const qint64 sizeLimit = 10 << 20;

typedef QByteArray Hash;

namespace syncTabsTableColumns {
enum {
    tabName,
    path,
    button
};
}

void setHeaderSectionResizeMode(QHeaderView *header, int logicalIndex, QHeaderView::ResizeMode mode)
{
#if QT_VERSION < 0x050000
    header->setResizeMode(logicalIndex, mode);
#else
    header->setSectionResizeMode(logicalIndex, mode);
#endif
}

bool readConfig(QFile *file, QVariantMap *config)
{
    QDataStream stream(file);
    stream >> *config;
    return config->value(configVersion, 0).toInt() == currentVersion;
}

void writeConfiguration(QFile *file, const QStringList &savedFiles)
{
    QVariantMap config;
    config.insert(configVersion, currentVersion);
    config.insert(configSavedFiles, savedFiles);

    QDataStream stream(file);
    stream << config;
}

bool renameToUnique(QString *name, const QStringList &usedNames)
{
    if ( !usedNames.contains(*name) )
        return true;

    int i = 0;
    QString newName;
    do {
        if (i >= 99999)
            return false;
        newName = *name + '-' + QString::number(++i);
    } while ( usedNames.contains(newName) );

    *name = newName;
    return true;
}

bool renameToUnique(const QString &fileName, QMultiMap<Hash, QString> *existingFiles = NULL)
{
    if ( !QFile::exists(fileName) )
        return true;

    int i = 0;
    const QString ext = QFileInfo(fileName).completeSuffix();
    const QString filePath = fileName.left( fileName.size() - ext.size() - (ext.isEmpty() ? 0 : 1) );
    QString newFileName;
    do {
        if (i >= 9999)
            return false;
        newFileName = filePath + '-' + QString::number(++i) + '.' + ext;
    } while ( QFile::exists(newFileName) );

    if (existingFiles) {
        foreach ( const Hash key, existingFiles->uniqueKeys() ) {
            if ( existingFiles->values(key).contains(fileName) ) {
                existingFiles->remove(key, fileName);
                existingFiles->insert(key, newFileName);
                break;
            }
        }
    }

    return QFile::rename(fileName, newFileName);
}

QPushButton *createBrowseButton()
{
    QScopedPointer<QPushButton> button(new QPushButton);
    QFont font("FontAwesome");
    font.setPixelSize(14);
    button->setFont(font);
    button->setText( QString(QChar(IconFolderOpen)) );
    button->setToolTip( ItemSyncLoader::tr("Browse...") );
    return button.take();
}

struct Ext {
    Ext() : extension(), format() {}

    Ext(const QString &extension, const QString &format)
        : extension(extension)
        , format(format)
    {}

    QString extension;
    QString format;
};

QList<Ext> fileExtensionsAndFormats()
{
    static QList<Ext> exts;

    if ( exts.isEmpty() ) {
        exts.append( Ext("_note.txt", mimeItemNotes) );
        exts.append( Ext("_wnd.txt", mimeWindowTitle) );

        exts.append( Ext(".bmp", "image/bmp") );
        exts.append( Ext(".gif", "image/gif") );
        exts.append( Ext(".html", "text/html") );
        exts.append( Ext("_inkscape.svg", "image/x-inkscape-svg-compressed") );
        exts.append( Ext(".jpg", "image/jpeg") );
        exts.append( Ext(".jpg", "image/jpeg") );
        exts.append( Ext(".png", "image/png") );
        exts.append( Ext(".txt", "text/plain") );
        exts.append( Ext(".uri", "text/uri-list") );
        exts.append( Ext(".xml", "application/xml") );
        exts.append( Ext("_xml.svg", "image/svg+xml") );
        exts.append( Ext(".xml", "text/xml") );
    }

    return exts;
}

Ext findByFormat(const QString &format, const QList<Ext> &exts)
{
    for (int i = 0; i < exts.size(); ++i) {
        const Ext &ext = exts[i];
        if (ext.format == format)
            return ext;
    }

    return Ext();
}

Ext findByExtension(const QString &fileName, const QList<Ext> &exts)
{
    for (int i = 0; i < exts.size(); ++i) {
        const Ext &ext = exts[i];
        if ( fileName.endsWith(ext.extension) )
            return ext;
    }

    return Ext("", mimeUnknownData);
}

Hash calculateHash(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1);
}

Hash calculateHash(QFile *file)
{
    if (file->size() > sizeLimit)
        return QByteArray();
    file->seek(0);
    return QCryptographicHash::hash( file->readAll(), QCryptographicHash::Sha1 );
}

bool saveItemFile(const QString &fileName, const QByteArray &bytes,
                  QMultiMap<Hash, QString> *existingFiles, QStringList *savedFiles)
{
    const Hash hash = calculateHash(bytes);
    const QList<QString> fileNames = existingFiles->values(hash);

    if ( !fileNames.isEmpty() ) {
        if ( fileNames.contains(fileName) ) {
            existingFiles->remove(hash, fileName);
        } else {
            const QString &existingFileName = fileNames.first();
            if ( !renameToUnique(fileName, existingFiles)
                 || !QFile::rename(existingFileName, fileName) )
            {
                return false;
            }
            existingFiles->remove(hash, existingFileName);
        }
    } else {
        if ( !renameToUnique(fileName, existingFiles) )
            return false;
        QFile f(fileName);
        if ( !f.open(QIODevice::WriteOnly) || f.write(bytes) == -1 ) {
            log( QString("ItemSync: %1").arg(f.errorString()), LogError );
            return false;
        }
    }

    savedFiles->prepend(fileName);

    return true;
}

struct BaseNameExtentions {
    QString baseName;
    QList<Ext> exts;
};

typedef QList<BaseNameExtentions> BaseNameExtentionsList;
BaseNameExtentionsList listFiles(const QStringList &files)
{
    BaseNameExtentionsList fileList;
    QMap<QString, int> fileMap;
    const QList<Ext> exts = fileExtensionsAndFormats();
    foreach (const QString &fileName, files) {
        const Ext ext = findByExtension(fileName, exts);
        const QString baseName = fileName.left( fileName.size() - ext.extension.size() );

        int i = fileMap.value(baseName, -1);
        if (i == -1) {
            i = fileList.size();
            fileList.append( BaseNameExtentions() );
            fileList[i].baseName = baseName;
            fileMap.insert(baseName, i);
        }

        fileList[i].exts.append(ext);
    }

    return fileList;
}

QString getBaseName(const QModelIndex &index, const QStringList &formats)
{
    const int baseNameIndex = formats.indexOf(mimeBaseName);
    return (baseNameIndex != -1) ? index.data(contentType::firstFormat + baseNameIndex).toString()
                                 : QString();
}

void updateUriList(const QFileInfo &info, QByteArray *unsavedUriList, QByteArray *unsavedText)
{
    if ( !unsavedUriList->isEmpty() ) {
        unsavedUriList->append("\n");
        unsavedText->append("\n");
    }

    const QString path = info.absoluteFilePath();
    unsavedUriList->append( QUrl::fromLocalFile(path).toEncoded() );
    unsavedText->append( QString(path)
                        .replace('\\', "\\\\")
                        .replace('\n', "\\n")
                        .replace('\r', "\\r") );
}

void addNoSaveData(const QByteArray &unsavedUriList, const QByteArray &unsavedText, QVariantMap *dataMap)
{
    if ( !unsavedUriList.isEmpty() ) {
        dataMap->insert(mimeNoSave, "Synchronization disabled.");
        dataMap->insert("text/uri-list", unsavedUriList);
        dataMap->insert("text/plain", unsavedText);
    }
}

/// Load hash of all existing files to map (hash -> filename).
QMultiMap<Hash, QString> listFiles(const QDir &dir, QSet<int> *usedBaseNameIndexes)
{
    QMultiMap<Hash, QString> files;

    QRegExp re("^copyq_(\\d{4})");

    foreach ( const QString &fileName, dir.entryList(itemFileFilter) ) {
        if ( re.indexIn(fileName) == 0 )
            usedBaseNameIndexes->insert(re.cap(1).toInt());
        const QString path = dir.absoluteFilePath(fileName);
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            const Hash hash = calculateHash(&f);
            files.insert(hash, path);
        }
    }

    return files;
}

bool hasVideoExtension(const QString &ext)
{
    return ext == "avi"
            || ext == "mkv"
            || ext == "mp4"
            || ext == "mpg"
            || ext == "mpeg"
            || ext == "ogv"
            || ext == "flv";
}

bool hasAudioExtension(const QString &ext)
{
    return ext == "mp3"
            || ext == "wav"
            || ext == "ogg"
            || ext == "m4a";
}

bool hasImageExtension(const QString &ext)
{
    return ext == "png"
            || ext == "jpg"
            || ext == "gif"
            || ext == "bmp"
            || ext == "svg"
            || ext == "tga"
            || ext == "tiff"
            || ext == "psd"
            || ext == "xcf"
            || ext == "ico"
            || ext == "pbm"
            || ext == "ppm"
            || ext == "eps"
            || ext == "pcx"
            || ext == "jpx"
            || ext == "jp2";
}

bool hasArchiveExtension(const QString &ext)
{
    return ext == "zip"
            || ext == "7z"
            || ext == "tar"
            || ext == "rar"
            || QRegExp("r\\d\\d").exactMatch(ext)
            || ext == "arj";
}

bool hasTextExtension(const QString &ext)
{
    return ext == "txt"
            || ext == "log"
            || ext == "xml"
            || ext == "html"
            || ext == "htm"
            || ext == "pdf"
            || ext == "doc"
            || ext == "docx"
            || ext == "odt"
            || ext == "xls"
            || ext == "rtf"
            || ext == "csv"
            || ext == "ppt";
}

} // namespace

ItemSync::ItemSync(const QString &label, IconId icon, bool replaceChildItem, ItemWidget *childItem)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_label( new QTextEdit(this) )
    , m_icon( new QLabel(this) )
    , m_childItem(childItem)
{
    if (replaceChildItem)
        m_childItem.reset();

    // icon
    m_icon->setObjectName("item_child");
    m_icon->setTextFormat(Qt::PlainText);
    QFont iconFont("FontAwesome");
    m_icon->setFont(iconFont);
    m_icon->setStyleSheet("*{font-size:14px}");
    m_icon->setText(QString(QChar(icon)));
    m_icon->adjustSize();

    if ( !m_childItem.isNull() ) {
        m_childItem->widget()->setObjectName("item_child");
        m_childItem->widget()->setParent(this);
        m_childItem->widget()->move( m_icon->width() + 6, 0 );
    }

    m_label->setObjectName("item_child");

    m_label->document()->setDefaultFont(font());

    m_label->setReadOnly(true);
    m_label->setUndoRedoEnabled(false);

    m_label->setFocusPolicy(Qt::NoFocus);
    m_label->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_label->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_label->setFrameStyle(QFrame::NoFrame);

    // Selecting text copies it to clipboard.
    connect( m_label, SIGNAL(selectionChanged()), SLOT(onSelectionChanged()) );

    m_label->setPlainText(label);

    m_label->viewport()->installEventFilter(this);

    m_icon->move(4, 4);
    m_label->move( m_icon->geometry().right() + 4, 0 );
}

void ItemSync::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    if ( !m_childItem.isNull() )
        m_childItem->setHighlight(re, highlightFont, highlightPalette);

    if (m_label != NULL) {
        QList<QTextEdit::ExtraSelection> selections;

        if ( !re.isEmpty() ) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground( highlightPalette.base() );
            selection.format.setForeground( highlightPalette.text() );
            selection.format.setFont(highlightFont);

            QTextCursor cur = m_label->document()->find(re);
            int a = cur.position();
            while ( !cur.isNull() ) {
                if ( cur.hasSelection() ) {
                    selection.cursor = cur;
                    selections.append(selection);
                } else {
                    cur.movePosition(QTextCursor::NextCharacter);
                }
                cur = m_label->document()->find(re, cur);
                int b = cur.position();
                if (a == b) {
                    cur.movePosition(QTextCursor::NextCharacter);
                    cur = m_label->document()->find(re, cur);
                    b = cur.position();
                    if (a == b) break;
                }
                a = b;
            }
        }

        m_label->setExtraSelections(selections);
    }

    update();
}

void ItemSync::updateSize()
{
    const int w = maximumWidth() - ( m_icon != NULL ? m_icon->width() : 0 );

    if ( !m_childItem.isNull() ) {
        m_childItem->widget()->setMaximumWidth(w);
        m_childItem->updateSize();
    }

    const int h = m_childItem.isNull() ? 0 : m_childItem->widget()->height();

    if (m_label != NULL) {
        m_label->setMaximumWidth(w - m_label->x());
        m_label->document()->setTextWidth(w - m_label->x());
        m_label->resize( m_label->document()->idealWidth() + 16, m_label->document()->size().height() );

        if ( !m_childItem.isNull() )
            m_childItem->widget()->move( 0, m_label->height() );

        resize( w, h + m_label->height() );
    } else {
        resize(w, h);
    }
}

void ItemSync::mousePressEvent(QMouseEvent *e)
{
    m_label->setTextCursor( m_label->cursorForPosition(e->pos()) );
    QWidget::mousePressEvent(e);
    e->ignore();
}

void ItemSync::mouseDoubleClickEvent(QMouseEvent *e)
{
    if ( e->modifiers().testFlag(Qt::ShiftModifier) )
        QWidget::mouseDoubleClickEvent(e);
    else
        e->ignore();
}

void ItemSync::contextMenuEvent(QContextMenuEvent *e)
{
    e->ignore();
}

void ItemSync::mouseReleaseEvent(QMouseEvent *e)
{
    if ( property("copyOnMouseUp").toBool() ) {
        setProperty("copyOnMouseUp", false);
        if ( m_label->textCursor().hasSelection() )
            m_label->copy();
    } else {
        QWidget::mouseReleaseEvent(e);
    }
}

bool ItemSync::eventFilter(QObject *obj, QEvent *event)
{
    if ( m_label != NULL && obj == m_label->viewport() ) {
        if ( event->type() == QEvent::MouseButtonPress ) {
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);
            mousePressEvent(ev);
        }
    }

    return QWidget::eventFilter(obj, event);
}

void ItemSync::onSelectionChanged()
{
    setProperty("copyOnMouseUp", true);
}

class FileWatcher : public QObject {
    Q_OBJECT
public:
    FileWatcher(const QStringList &paths, QAbstractItemModel *model, QObject *parent)
        : QObject(parent)
        , m_watcher(paths)
        , m_model(model)
    {
        m_updateTimer.setInterval(updateItemsIntervalMs);
        m_updateTimer.setSingleShot(true);
        connect( &m_updateTimer, SIGNAL(timeout()),
                 SLOT(updateItems()) );

        connect( &m_watcher, SIGNAL(directoryChanged(QString)),
                 &m_updateTimer, SLOT(start()) );
        connect( &m_watcher, SIGNAL(fileChanged(QString)),
                 &m_updateTimer, SLOT(start()) );
    }

    void clearIndexBaseNames()
    {
        m_itemFiles.clear();
    }

    void setIndexBaseName(const QModelIndex &index, const QString &baseName)
    {
        m_itemFiles.insert(index, baseName);
    }

    bool createItemsFromFiles(const QDir &dir, const QStringList &files,
                              const QStringList &removedBaseNames = QStringList())
    {
        const int maxItems = m_model->property("maxItems").toInt();

        const BaseNameExtentionsList fileList = listFiles(files);
        foreach (const BaseNameExtentions &baseNameWithExts, fileList) {
            bool removed = removedBaseNames.contains(baseNameWithExts.baseName);

            QVariantMap dataMap;
            QByteArray unsavedUriList;
            QByteArray unsavedText;

            foreach (const Ext &ext, baseNameWithExts.exts) {
                const QString fileName = baseNameWithExts.baseName + ext.extension;

                QFile f( dir.absoluteFilePath(fileName) );
                if ( (removed && QFileInfo(f).lastModified().secsTo(QDateTime::currentDateTime()) > (updateItemsIntervalMs/1000))
                     || !f.open(QIODevice::ReadOnly))
                {
                    continue;
                }

                if ( fileName.endsWith(dataFileSuffix) ) {
                    QDataStream stream(&f);
                    QVariantMap dataMap2;
                    stream >> dataMap2;
                    if ( stream.status() == QDataStream::Ok )
                        dataMap.unite(dataMap2);
                } else if (f.size() > sizeLimit || (ext.format == mimeUnknownData && !saveUnknownData) ) {
                    updateUriList( QFileInfo(f), &unsavedUriList, &unsavedText );
                } else {
                    dataMap.insert(ext.format, f.readAll());
                }
            }

            addNoSaveData(unsavedUriList, unsavedText, &dataMap);

            if ( !dataMap.isEmpty() ) {
                dataMap.insert( mimeBaseName, QFileInfo(baseNameWithExts.baseName).fileName() );

                if ( !setIndexData(dataMap) )
                    return false;
                if ( m_model->rowCount() >= maxItems )
                    break;
            }
        }

        return true;
    }

private slots:
    /**
     * Check for new files.
     */
    void updateItems()
    {
        if ( m_model.isNull() )
            return;

        m_model->setProperty(propertyModelDisabled, true);

        QDir dir( m_watcher.directories().value(0) );
        QStringList files = dir.entryList(itemFileFilter, QDir::Time | QDir::Reversed);
        BaseNameExtentionsList fileList = listFiles(files);
        QStringList removedBaseNames;

        for ( QMap<QPersistentModelIndex, QString>::iterator it = m_itemFiles.begin();
              it != m_itemFiles.end(); ++it )
        {
            const QPersistentModelIndex &index = it.key();
            const QString &baseName = it.value();

            if ( it.key().isValid() ) {
                int i = 0;
                for ( i = 0; i < fileList.size() && fileList[i].baseName != baseName; ++i ) {}

                QVariantMap dataMap;
                QByteArray unsavedUriList;
                QByteArray unsavedText;

                if ( i < fileList.size() ) {
                    const QString fileNamePrefix = dir.absoluteFilePath(baseName);
                    foreach (const Ext &ext, fileList[i].exts) {
                        QFile f(fileNamePrefix + ext.extension);
                        files.removeOne(baseName + ext.extension);
                        if (f.size() > sizeLimit || (ext.format == mimeUnknownData && !saveUnknownData) )
                            updateUriList( QFileInfo(f), &unsavedUriList, &unsavedText );
                        else if ( f.open(QIODevice::ReadOnly) )
                            dataMap.insert( ext.format, f.readAll() );
                        m_watcher.addPath( f.fileName() );
                    }
                    fileList.removeAt(i);
                }

                addNoSaveData(unsavedUriList, unsavedText, &dataMap);

                if ( dataMap.isEmpty() ) {
                    m_model->removeRow( index.row() );
                } else {
                    dataMap.insert(mimeBaseName, baseName);
                    m_model->setData(index, QVariant(), contentType::notes);
                    m_model->setData(index, dataMap, contentType::data);
                }
            } else {
                // If an item was removed, ignore its file.
                removedBaseNames.append(baseName);
            }
        }

        createItemsFromFiles(dir, files, removedBaseNames);

        foreach (const QString &fileName, files)
            m_watcher.addPath( dir.absoluteFilePath(fileName) );

        m_model->setProperty(propertyModelDisabled, false);
    }

private:
    bool setIndexData(const QVariantMap &dataMap)
    {
        if ( m_model->insertRow(0) ) {
            const QModelIndex &index = m_model->index(0, 0);
            m_model->setData(index, dataMap, contentType::data);
            Q_ASSERT( dataMap.contains(mimeBaseName) );
            setIndexBaseName( index, dataMap.value(mimeBaseName).toString() );
            return true;
        }

        return false;
    }

    QFileSystemWatcher m_watcher;
    QPointer<QAbstractItemModel> m_model;
    QTimer m_updateTimer;
    QMap<QPersistentModelIndex, QString> m_itemFiles;
};

ItemSyncLoader::ItemSyncLoader()
    : ui(NULL)
    , m_settings()
{
}

ItemSyncLoader::~ItemSyncLoader()
{
}

QVariantMap ItemSyncLoader::applySettings()
{
    Q_ASSERT(ui);

    QTableWidget *t = ui->tableWidgetSyncTabs;
    QStringList tabPaths;
    m_tabPaths.clear();
    for (int row = 0, i = 0; i < t->rowCount(); ++row, i += 2) {
        const QString tabName = t->item(row, syncTabsTableColumns::tabName)->text();
        if ( !tabName.isEmpty() ) {
            const QString tabPath = t->item(row, syncTabsTableColumns::path)->text();
            tabPaths << tabName << tabPath;
            m_tabPaths.insert(tabName, tabPath);
        }
    }
    m_settings.insert("sync_tabs", tabPaths);

    return m_settings;
}

void ItemSyncLoader::loadSettings(const QVariantMap &settings)
{
    m_settings = settings;

    m_tabPaths.clear();
    const QStringList tabPaths = m_settings.value("sync_tabs").toStringList();
    for (int i = 0; i < tabPaths.size(); i += 2)
        m_tabPaths.insert( tabPaths[i], tabPaths.value(i + 1) );
}

QWidget *ItemSyncLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemSyncSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    const QStringList tabPaths = m_settings.value("sync_tabs").toStringList();
    QTableWidget *t = ui->tableWidgetSyncTabs;
    for (int row = 0, i = 0; i < tabPaths.size() + 20; ++row, i += 2) {
        t->insertRow(row);
        t->setItem( row, syncTabsTableColumns::tabName, new QTableWidgetItem(tabPaths.value(i)) );
        t->setItem( row, syncTabsTableColumns::path, new QTableWidgetItem(tabPaths.value(i + 1)) );

        QPushButton *button = createBrowseButton();
        connect( button, SIGNAL(clicked()), SLOT(onBrowseButtonClicked()) );
        t->setCellWidget( row, syncTabsTableColumns::button, button);
    }

    QHeaderView *header = t->horizontalHeader();
    setHeaderSectionResizeMode(header, syncTabsTableColumns::path, QHeaderView::Stretch);
    setHeaderSectionResizeMode(header, syncTabsTableColumns::button, QHeaderView::Fixed);
    header->resizeSection(syncTabsTableColumns::button, t->rowHeight(0));
    t->resizeColumnToContents(syncTabsTableColumns::tabName);

    return w;
}

bool ItemSyncLoader::loadItems(const QString &tabName, QAbstractItemModel *model, QFile *file)
{
    QVariantMap config;
    if ( !readConfig(file, &config) ) {
        if ( shouldSyncTab(tabName) ) {
            model->setProperty(propertyModelDirty, true);
            createWatcher( model, QStringList(tabPath(tabName)) );
        }
        return false;
    }

    model->setProperty(propertyModelDisabled, true);

    QStringList files = config.value(configSavedFiles).toStringList();

    if ( shouldSyncTab(tabName) ) {
        const QString path = tabPath(tabName);
        if ( !path.isEmpty() ) {
            QDir dir( tabPath(tabName) );
            if ( !dir.mkpath(".") )
                return true;

            foreach ( const QString &fileName, dir.entryList(itemFileFilter, QDir::Time | QDir::Reversed) ) {
                const QString filePath = dir.absoluteFilePath(fileName);
                if ( !files.contains(filePath) )
                    files.append(filePath);
            }

            // Monitor files in directory.
            files.append( dir.path() );
            FileWatcher *watcher = createWatcher(model, files);

            if ( !watcher->createItemsFromFiles(dir, files) )
                return true;
        }
    } else {
        // Synchronization was disabled for this tab, save items again.
        model->setProperty(propertyModelDirty, true);
    }

    model->setProperty(propertyModelDisabled, false);

    return true;
}

bool ItemSyncLoader::saveItems(const QString &tabName, const QAbstractItemModel &model, QFile *file)
{
    if ( !shouldSyncTab(tabName) )
        return false;

    // If anything fails, just return false so the items are save regularly.

    // Don't save items if path is empty.
    const QString path = tabPath(tabName);
    if ( path.isEmpty() )
        return true;

    // Create path if doesn't exist.
    QDir dir(path);
    if ( !dir.mkpath(".") )
        return false;

    QSet<int> usedBaseNameIndexes;
    int baseNameIndex = -1;
    QMultiMap<Hash, QString> existingFiles = listFiles(dir, &usedBaseNameIndexes);

    QStringList savedFiles;
    QStringList usedBaseNames;

    FileWatcher *watcher = m_watchers.value(&model, NULL);
    Q_ASSERT(watcher);
    watcher->clearIndexBaseNames();

    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        const QStringList formats = index.data(contentType::formats).toStringList();

        QString baseName = getBaseName(index, formats);
        if ( baseName.isEmpty() ) {
            while ( usedBaseNameIndexes.contains(++baseNameIndex) ) {}
            baseName = QString("copyq_%1").arg( baseNameIndex, 4, 10, QChar('0') );
        }
        else if (!renameToUnique(&baseName, usedBaseNames))
            return false;
        usedBaseNames.append(baseName);
        watcher->setIndexBaseName(index, baseName);

        const QString filePath = dir.absoluteFilePath(baseName);

        QVariantMap dataMap;
        const QList<Ext> exts = fileExtensionsAndFormats();

        bool noSave = formats.contains(mimeNoSave);

        if (noSave) {
            savedFiles.prepend(filePath);
            const int urisIndex = formats.indexOf("text/uri-list");
            if (urisIndex != -1) {
                const QByteArray uriList = index.data(contentType::firstFormat + urisIndex).toByteArray();
                foreach (const QByteArray &uri, uriList.split('\n')) {
                    const QString path = QUrl::fromEncoded(uri).toLocalFile();
                    foreach (const QByteArray &key, existingFiles.keys()) {
                        if ( existingFiles.remove(key, path) > 0 )
                            break;
                    }
                }
            }
        }

        for (int formatIndex = 0; formatIndex < formats.size(); ++formatIndex) {
            const QString format = formats[formatIndex];
            if (format == mimeBaseName)
                continue;
            if ( noSave && (format == mimeNoSave || format == "text/plain" || format == "text/uri-list") )
                continue;

            const QVariant value = index.data(contentType::firstFormat + formatIndex);

            if (format == mimeUnknownData) {
                if ( !saveItemFile(filePath, value.toByteArray(), &existingFiles, &savedFiles) )
                    return false;
            } else {
                const QString ext = findByFormat(format, exts).extension;
                if ( ext.isEmpty() )
                    dataMap.insert(format, value);
                else if ( !saveItemFile(filePath + ext, value.toByteArray(), &existingFiles, &savedFiles) )
                    return false;
            }
        }

        if ( !dataMap.isEmpty() ) {
            QByteArray data;
            {
                QDataStream stream(&data, QIODevice::WriteOnly);
                stream << dataMap;
            }
            if ( !saveItemFile(filePath + dataFileSuffix, data, &existingFiles, &savedFiles) )
                return false;
        }
    }

    // Remove unneeded files (remaining records in the hash map).
    foreach ( const Hash key, existingFiles.uniqueKeys() ) {
        foreach ( const QString &fileName, existingFiles.values(key) )
            QFile::remove(fileName);
    }

    writeConfiguration(file, savedFiles);

    return true;
}

bool ItemSyncLoader::createTab(const QString &tabName, QAbstractItemModel *model, QFile *file)
{
    if ( !shouldSyncTab(tabName) )
        return false;

    QDir dir( tabPath(tabName) );
    QStringList savedFiles;
    foreach ( const QString &fileName, dir.entryList(itemFileFilter, QDir::Name | QDir::Reversed) )
        savedFiles.append( dir.absoluteFilePath(fileName) );

    writeConfiguration(file, savedFiles);

    file->seek(0);
    loadItems(tabName, model, file);

    return true;
}

ItemWidget *ItemSyncLoader::transform(ItemWidget *itemWidget, const QModelIndex &index)
{
    const QStringList formats = index.data(contentType::formats).toStringList();
    const int indexOfBaseName = formats.indexOf(mimeBaseName);
    if (indexOfBaseName == -1)
        return NULL;

    const QString baseName = index.data(contentType::firstFormat + indexOfBaseName).toString();

    IconId icon = IconFile;
    if ( formats.contains(mimeNoSave) ) {
        // icon from file name extension
        const int i = baseName.lastIndexOf('.');
        if (i != -1) {
            const QString ext = baseName.mid(i + 1).toLower();
            if ( hasVideoExtension(ext) )
                icon = IconPlayCircle;
            else if ( hasAudioExtension(ext) )
                icon = IconVolumeUp;
            else if ( hasImageExtension(ext) )
                icon = IconCamera;
            else if ( hasArchiveExtension(ext) )
                icon = IconFileText;
            else if ( hasTextExtension(ext) )
                icon = IconFileText;
        }
    } else {
        // icon from MIME type
        if ( formats.indexOf(QRegExp("video/.*")) != -1 )
            icon = IconPlayCircle;
        else if ( formats.indexOf(QRegExp("audio/.*")) != -1 )
            icon = IconVolumeUp;
        else if ( formats.indexOf(QRegExp("image/.*")) != -1 )
            icon = IconCamera;
        else if ( formats.indexOf(QRegExp("text/.*")) != -1 )
            icon = IconFileText;
    }

    bool replaceChildItem = formats.indexOf(mimeNoSave) != -1;
    return new ItemSync(baseName, icon, replaceChildItem, itemWidget);
}

void ItemSyncLoader::removeWatcher(QObject *watcher)
{
    foreach ( const QObject *model, m_watchers.keys() ) {
        if (m_watchers[model] == watcher) {
            m_watchers.remove(model);
            return;
        }
    }
}

void ItemSyncLoader::removeModel()
{
    const QObject *model = sender();
    Q_ASSERT(model);
    delete m_watchers.take(model);
}

void ItemSyncLoader::onBrowseButtonClicked()
{
    QTableWidget *t = ui->tableWidgetSyncTabs;

    QObject *button = sender();
    Q_ASSERT(button != NULL);

    int row = 0;
    for ( ; row < t->rowCount() && t->cellWidget(row, syncTabsTableColumns::button) != button; ++row ) {}
    Q_ASSERT(row != t->rowCount());


    QTableWidgetItem *item = t->item(row, syncTabsTableColumns::path);
    const QString path =
            QFileDialog::getExistingDirectory( t, tr("Open Directory for Synchronization"), item->text() );
    if ( !path.isEmpty() )
        item->setText(path);
}

bool ItemSyncLoader::shouldSyncTab(const QString &tabName) const
{
    return m_tabPaths.contains(tabName);
}

QString ItemSyncLoader::tabPath(const QString &tabName) const
{
    Q_ASSERT(shouldSyncTab(tabName));
    return m_tabPaths[tabName];
}

FileWatcher *ItemSyncLoader::createWatcher(QAbstractItemModel *model, const QStringList &paths)
{
    FileWatcher *watcher = new FileWatcher(paths, model, this);
    m_watchers.insert(model, watcher);

    connect( model, SIGNAL(unloaded()),
             SLOT(removeModel()) );
    connect( model, SIGNAL(destroyed()),
             SLOT(removeModel()) );
    connect( watcher, SIGNAL(destroyed(QObject*)),
             SLOT(removeWatcher(QObject*)) );

    return watcher;
}

Q_EXPORT_PLUGIN2(itemsync, ItemSyncLoader)

#include "itemsync.moc"
