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
#include "gui/iconselectbutton.h"
#include "gui/icons.h"
#include "gui/iconfont.h"
#include "gui/iconwidget.h"
#include "item/serialize.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScopedPointer>
#include <QTextEdit>
#include <QTimer>
#include <QtPlugin>
#include <QUrl>
#include <QVariantMap>

struct FileFormat {
    bool isValid() const { return !formats.isEmpty(); }
    QStringList formats;
    QString itemMime;
    QString icon;
};

namespace {

const int currentVersion = 1;
const char dataFileHeader[] = "CopyQ_itemsync_tab";

const char configVersion[] = "copyq_itemsync_version";
const char configPath[] = "path";
const char configSavedFiles[] = "saved_files";
const char configSyncTabs[] = "sync_tabs";
const char configFormatSettings[] = "format_settings";

const char dataFileSuffix[] = "_data.dat";

#define MIME_PREFIX_ITEMSYNC MIME_PREFIX "itemsync-"
const char mimeExtensionMap[] = MIME_PREFIX_ITEMSYNC "mime-to-extension-map";
const char mimeBaseName[] = MIME_PREFIX_ITEMSYNC "basename";
const char mimeNoSave[] = MIME_PREFIX_ITEMSYNC "no-save";
const char mimeUnknownData[] = "application/octet-stream";

const char propertyModelDisabled[] = "disabled";
const char propertyModelDirty []= "dirty";

const int updateItemsIntervalMs = 2000; // Interval to update items after a file has changed.

const bool saveUnknownData = false;
const qint64 sizeLimit = 10 << 20;

typedef QByteArray Hash;

namespace syncTabsTableColumns {
enum {
    tabName,
    path,
    browse
};
}

namespace formatSettingsTableColumns {
enum {
    formats,
    itemMime,
    icon
};
}

QDir::Filters itemFileFilter()
{
    return QDir::Files | QDir::Readable | QDir::Writable;
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
    QString header;
    stream >> header;
    if (header != dataFileHeader)
        return false;

    stream >> *config;
    return config->value(configVersion, 0).toInt() == currentVersion;
}

void writeConfiguration(QFile *file, const QStringList &savedFiles)
{
    QVariantMap config;
    config.insert(configVersion, currentVersion);
    config.insert(configSavedFiles, savedFiles);

    QDataStream stream(file);
    stream << QString(dataFileHeader);
    stream << config;
}

FileFormat getFormatSettingsFromFileName(const QString &fileName,
                                         const QList<FileFormat> &formatSettings,
                                         QString *foundExt = NULL)
{
    for ( int i = 0; i < formatSettings.size(); ++i ) {
        const FileFormat &format = formatSettings[i];
        foreach ( const QString &ext, format.formats ) {
            if ( fileName.endsWith(ext) ) {
                if (foundExt)
                    *foundExt = ext;
                return format;
            }
        }
    }
    return FileFormat();
}

QString getBaseName(const QModelIndex &index)
{
    return index.data(contentType::data).toMap().value(mimeBaseName).toString();
}

bool renameToUnique(QString *name, QStringList *usedNames,
                    const QList<FileFormat> &formatSettings)
{
    if ( name->isEmpty() )
        *name = "copyq_0000";
    if ( !usedNames->contains(*name) ) {
        usedNames->append(*name);
        return true;
    }

    QString ext;
    const FileFormat fileFormat = getFormatSettingsFromFileName(*name, formatSettings, &ext);
    if ( !fileFormat.isValid() ) {
        const int i = name->lastIndexOf('.');
        if (i != -1)
            ext = name->mid(i);
    }
    QString baseName = name->left( name->size() - ext.size() );
    if ( baseName.endsWith('.') ) {
        baseName.chop(1);
        ext.prepend('.');
    }

    int i = 0;
    int fieldWidth = 0;

    QRegExp re("\\d+$");
    if ( baseName.indexOf(re) != -1 ) {
        const QString num = re.cap(0);
        i = num.toInt();
        fieldWidth = num.size();
        baseName = baseName.mid( 0, baseName.size() - fieldWidth );
    } else {
        baseName.append('-');
    }

    QString newName;
    do {
        if (i >= 99999)
            return false;
        newName = baseName + QString("%1").arg(++i, fieldWidth, 10, QChar('0')) + ext;
    } while ( usedNames->contains(newName) );

    *name = newName;
    usedNames->append(*name);

    return true;
}

bool renameToUnique(QAbstractItemModel *model, int first, int last,
                    const QList<FileFormat> &formatSettings)
{
    QStringList usedBaseNames;
    for (int i = 0; i < model->rowCount(); ++i) {
        if (i >= first && i <= last)
            continue;
        const QModelIndex index = model->index(i, 0);
        const QString baseName = getBaseName(index);
        Q_ASSERT( !baseName.isEmpty() );
        usedBaseNames.append(baseName);
    }

    for (int i = first; i <= last; ++i) {
        const QModelIndex index = model->index(i, 0);
        const QString oldBaseName = getBaseName(index);
        QString baseName = oldBaseName;
        if ( !renameToUnique(&baseName, &usedBaseNames, formatSettings) )
            return false;
        if (baseName != oldBaseName) {
            QVariantMap dataMap = index.data(contentType::data).toMap();
            dataMap.insert(mimeBaseName, baseName);
            model->setData(index, dataMap, contentType::data);
        }
    }

    return true;
}

QString iconFromId(int id)
{
    return id != -1 ? QString(QChar(id)) : QString();
}

QPushButton *createBrowseButton()
{
    QScopedPointer<QPushButton> button(new QPushButton);
    button->setFont( iconFont() );
    button->setText( iconFromId(IconFolderOpen) );
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

        exts.append( Ext(".bmp", "image/bmp") );
        exts.append( Ext(".gif", "image/gif") );
        exts.append( Ext(".html", mimeHtml) );
        exts.append( Ext("_inkscape.svg", "image/x-inkscape-svg-compressed") );
        exts.append( Ext(".jpg", "image/jpeg") );
        exts.append( Ext(".jpg", "image/jpeg") );
        exts.append( Ext(".png", "image/png") );
        exts.append( Ext(".txt", mimeText) );
        exts.append( Ext(".uri", mimeUriList) );
        exts.append( Ext(".xml", "application/xml") );
        exts.append( Ext("_xml.svg", "image/svg+xml") );
        exts.append( Ext(".xml", "text/xml") );

        exts.append( Ext(dataFileSuffix, "") );
    }

    return exts;
}

Ext findByFormat(const QString &format, const QList<Ext> &exts, const QVariantMap &userExtension)
{
    if ( userExtension.contains(format) )
        return Ext(userExtension[format].toString(), format);

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
                  QMultiMap<Hash, QString> *existingFiles)
{
    const Hash hash = calculateHash(bytes);
    const QStringList fileNames = existingFiles->values(hash);

    if ( fileNames.contains(fileName) ) {
        // already saved
        existingFiles->remove(hash, fileName);
    } else {
        QFile f(fileName);
        if ( !f.open(QIODevice::WriteOnly) || f.write(bytes) == -1 ) {
            log( QString("ItemSync: %1").arg(f.errorString()), LogError );
            return false;
        }
    }

    return true;
}

struct BaseNameExtensions {
    QString baseName;
    QList<Ext> exts;
};

typedef QList<BaseNameExtensions> BaseNameExtensionsList;
BaseNameExtensionsList listFiles(const QStringList &files,
                                 const QList<FileFormat> &formatSettings)
{
    BaseNameExtensionsList fileList;
    QMap<QString, int> fileMap;

    QList<Ext> userExts;
    foreach (const FileFormat &format, formatSettings) {
        if ( !format.itemMime.isEmpty() ) {
            foreach (const QString &ext, format.formats)
                userExts.append( Ext(ext, format.itemMime) );
        }
    }

    QList<Ext> exts = fileExtensionsAndFormats();

    foreach (const QString &fileName, files) {
        Ext ext = findByExtension(fileName, userExts);
        if ( ext.extension.isEmpty() )
            ext = findByExtension(fileName, exts);
        else
            ext.extension.clear();

        const QString baseName = fileName.left( fileName.size() - ext.extension.size() );

        int i = fileMap.value(baseName, -1);
        if (i == -1) {
            i = fileList.size();
            fileList.append( BaseNameExtensions() );
            fileList[i].baseName = baseName;
            fileMap.insert(baseName, i);
        }

        fileList[i].exts.append(ext);
    }

    return fileList;
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
    if ( unsavedUriList.isEmpty() ) {
        dataMap->remove(mimeNoSave);
    } else {
        dataMap->insert(mimeNoSave, "Synchronization disabled.");
        dataMap->insert(mimeUriList, unsavedUriList);
        dataMap->insert(mimeText, unsavedText);
    }
}

/// Load hash of all existing files to map (hash -> filename).
QMultiMap<Hash, QString> listFiles(const QDir &dir)
{
    QMultiMap<Hash, QString> files;

    foreach ( const QString &fileName, dir.entryList(itemFileFilter()) ) {
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

QVariantMap getMimeToExtensionMap(const QModelIndex &index)
{
    QVariantMap mimeToExtension;
    const QVariantMap dataMap = index.data(contentType::data).toMap();
    const QByteArray bytes = dataMap.value(mimeExtensionMap).toByteArray();
    foreach ( const QByteArray &line, bytes.split('\n') ) {
        QList<QByteArray> list = line.split(' ');
        if ( list.size() == 2 )
            mimeToExtension.insert( QString(list[0]), QString(list[1]) );
    }

    return mimeToExtension;
}

int iconFromMimeHelper(const QString &format)
{
    if ( format.startsWith("video/") )
        return IconPlayCircle;
    if ( format.startsWith("audio/") )
        return IconVolumeUp;
    if ( format.startsWith("image/") )
        return IconCamera;
    if ( format.startsWith("text/") )
        return IconFileText;
    return -1;
}

QString iconFromMime(const QString &format)
{
    return iconFromId(iconFromMimeHelper(format));
}

int iconFromBaseNameExtensionHelper(const QString &baseName)
{
    const int i = baseName.lastIndexOf('.');
    if (i != -1)  {
        const QString ext = baseName.mid(i + 1);
        if ( hasVideoExtension(ext) )
            return IconPlayCircle;
        if ( hasAudioExtension(ext) )
            return IconVolumeUp;
        if ( hasImageExtension(ext) )
            return IconCamera;
        if ( hasArchiveExtension(ext) )
            return IconFileText;
        if ( hasTextExtension(ext) )
            return IconFileText;
    }

    return -1;
}

QString iconFromBaseNameExtension(const QString &baseName, const QList<FileFormat> &formatSettings)
{
    const FileFormat fileFormat = getFormatSettingsFromFileName(baseName, formatSettings);
    if ( fileFormat.isValid() )
        return fileFormat.icon;

    return iconFromId(iconFromBaseNameExtensionHelper(baseName));
}

bool containsItemsWithFiles(const QList<QModelIndex> &indexList)
{
    foreach (const QModelIndex &index, indexList) {
        if ( index.data(contentType::data).toMap().contains(mimeBaseName) )
            return true;
    }

    return false;
}

} // namespace

ItemSync::ItemSync(const QString &label, const QString &icon, bool replaceChildItem, ItemWidget *childItem)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_label( new QTextEdit(this) )
    , m_icon( new IconWidget(icon, this) )
    , m_childItem(childItem)
{
    if (replaceChildItem)
        m_childItem.reset();

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

    m_icon->move(4, 2);
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

QWidget *ItemSync::createEditor(QWidget *parent) const
{
    return m_childItem.isNull() ? NULL : m_childItem->createEditor(parent);
}

void ItemSync::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    Q_ASSERT( !m_childItem.isNull() );
    return m_childItem->setEditorData(editor, index);
}

void ItemSync::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    Q_ASSERT( !m_childItem.isNull() );
    return m_childItem->setModelData(editor, model, index);
}

bool ItemSync::hasChanges(QWidget *editor) const
{
    Q_ASSERT( !m_childItem.isNull() );
    return m_childItem->hasChanges(editor);
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

void removeFormatFiles(const QString &path, const QVariantMap &mimeToExtension)
{
    foreach ( const QString &format, mimeToExtension.keys() ) {
        const QString ext = format.isEmpty() ? dataFileSuffix : mimeToExtension[format].toString();
        QFile::remove(path + ext);
    }
}

void ItemSync::onSelectionChanged()
{
    setProperty("copyOnMouseUp", true);
}

class FileWatcher : public QObject {
    Q_OBJECT

public:
    FileWatcher(const QString &path, const QStringList &paths, QAbstractItemModel *model,
                const QList<FileFormat> &formatSettings, QObject *parent)
        : QObject(parent)
        , m_watcher(paths)
        , m_model(model)
        , m_formatSettings(formatSettings)
        , m_path(path)
        , m_valid(false)
    {
        m_watcher.addPath(path);

        m_updateTimer.setInterval(updateItemsIntervalMs);
        m_updateTimer.setSingleShot(true);
        connect( &m_updateTimer, SIGNAL(timeout()),
                 SLOT(updateItems()) );

        connect( &m_watcher, SIGNAL(directoryChanged(QString)),
                 &m_updateTimer, SLOT(start()) );
        connect( &m_watcher, SIGNAL(fileChanged(QString)),
                 &m_updateTimer, SLOT(start()) );

        connectModel();
    }

    void createItemsFromFiles(const QDir &dir, const QStringList &files)
    {
        disconnectModel();

        const int maxItems = m_model->property("maxItems").toInt();

        const BaseNameExtensionsList fileList = listFiles(files, m_formatSettings);
        foreach (const BaseNameExtensions &baseNameWithExts, fileList) {
            QVariantMap dataMap;
            QByteArray mimeToExtension;
            QByteArray unsavedUriList;
            QByteArray unsavedText;

            foreach (const Ext &ext, baseNameWithExts.exts) {
                const QString fileName = baseNameWithExts.baseName + ext.extension;

                QFile f( dir.absoluteFilePath(fileName) );
                if ( !f.open(QIODevice::ReadOnly) )
                    continue;

                if ( fileName.endsWith(dataFileSuffix) && deserializeData(&dataMap, f.readAll()) ) {
                    mimeToExtension.append(" \n");
                } else if ( f.size() > sizeLimit || (ext.format == mimeUnknownData && !saveUnknownData) ) {
                    updateUriList( QFileInfo(f), &unsavedUriList, &unsavedText );
                } else {
                    dataMap.insert(ext.format, f.readAll());
                    mimeToExtension.append(ext.format + " " + ext.extension + "\n");
                }
            }

            addNoSaveData(unsavedUriList, unsavedText, &dataMap);

            if ( !dataMap.isEmpty() ) {
                dataMap.insert( mimeBaseName, QFileInfo(baseNameWithExts.baseName).fileName() );
                if ( !mimeToExtension.isEmpty() )
                    dataMap.insert(mimeExtensionMap, mimeToExtension);

                if ( !setIndexData(dataMap) )
                    return;
                if ( m_model->rowCount() >= maxItems )
                    break;
            }
        }

        connectModel();
    }

    void setPath(const QString &path) { m_path = path; }
    const QString &path() const { return m_path; }

    bool isValid() const { return m_valid; }

public slots:
    /**
     * Check for new files.
     */
    void updateItems()
    {
        if ( m_model.isNull() )
            return;

        disconnectModel();

        m_model->setProperty(propertyModelDisabled, true);

        QDir dir( m_watcher.directories().value(0) );
        QStringList files = dir.entryList(itemFileFilter(), QDir::Time | QDir::Reversed);
        BaseNameExtensionsList fileList = listFiles(files, m_formatSettings);

        for ( int row = 0; row < m_model->rowCount(); ++row ) {
            const QModelIndex index = m_model->index(row, 0);
            const QString baseName = getBaseName(index);

            int i = 0;
            for ( i = 0; i < fileList.size() && fileList[i].baseName != baseName; ++i ) {}

            QVariantMap dataMap;
            QByteArray mimeToExtension;
            QByteArray unsavedUriList;
            QByteArray unsavedText;

            if ( i < fileList.size() ) {
                const QString fileNamePrefix = dir.absoluteFilePath(baseName);
                foreach (const Ext &ext, fileList[i].exts) {
                    QFile f(fileNamePrefix + ext.extension);
                    files.removeOne(baseName + ext.extension);
                    if (f.size() > sizeLimit || (ext.format == mimeUnknownData && !saveUnknownData) ) {
                        updateUriList( QFileInfo(f), &unsavedUriList, &unsavedText );
                    } else if ( f.open(QIODevice::ReadOnly) ) {
                        if ( ext.format.isEmpty() ) {
                            deserializeData(&dataMap, f.readAll());
                            mimeToExtension.append(" \n");
                        } else {
                            dataMap.insert( ext.format, f.readAll() );
                            mimeToExtension.append(ext.format + " " + ext.extension + "\n");
                        }
                    }
                    watchPath( f.fileName() );
                }
                fileList.removeAt(i);
            }

            addNoSaveData(unsavedUriList, unsavedText, &dataMap);

            if ( dataMap.isEmpty() ) {
                m_model->removeRow(row);
                --row;
            } else {
                dataMap.insert(mimeBaseName, baseName);
                if ( !mimeToExtension.isEmpty() )
                    dataMap.insert(mimeExtensionMap, mimeToExtension);
                m_model->setData(index, dataMap, contentType::data);
            }
        }

        createItemsFromFiles(dir, files);

        foreach (const QString &fileName, files)
            watchPath( dir.absoluteFilePath(fileName) );

        m_model->setProperty(propertyModelDisabled, false);

        connectModel();
    }

private slots:
    void onRowsInserted(const QModelIndex &, int first, int last)
    {
        saveItems(first, last);
    }

    void onDataChanged(const QModelIndex &a, const QModelIndex &b)
    {
        saveItems(a.row(), b.row());
    }

private:
    void watchPath(const QString &path)
    {
        if ( !m_watcher.files().contains(path) )
            m_watcher.addPath(path);
    }

    void connectModel()
    {
        connect( m_model.data(), SIGNAL(rowsInserted(QModelIndex, int, int)),
                 this, SLOT(onRowsInserted(QModelIndex, int, int)) );
        connect( m_model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex)),
                 SLOT(onDataChanged(QModelIndex,QModelIndex)) );
        m_valid = true;
    }

    void disconnectModel()
    {
        m_valid = false;
        disconnect( m_model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex)),
                    this, SLOT(onDataChanged(QModelIndex,QModelIndex)) );
        disconnect( m_model.data(), SIGNAL(rowsInserted(QModelIndex, int, int)),
                    this, SLOT(onRowsInserted(QModelIndex, int, int)) );
    }

    bool setIndexData(const QVariantMap &dataMap)
    {
        if ( m_model->insertRow(0) ) {
            const QModelIndex &index = m_model->index(0, 0);
            m_model->setData(index, dataMap, contentType::updateData);
            Q_ASSERT( !getBaseName(index).isEmpty() );
            return true;
        }

        return false;
    }

    void saveItems(int first, int last)
    {
        disconnectModel();

        if ( !renameToUnique(m_model.data(), first, last, m_formatSettings) )
            return;

        if ( m_path.isEmpty() )
            return;

        // Create path if doesn't exist.
        QDir dir(m_path);
        if ( !dir.mkpath(".") ) {
            log( tr("Failed to create synchronization directory \"%1\"!").arg(m_path) );
            return;
        }

        QMultiMap<Hash, QString> existingFiles = listFiles(dir);

        for (int row = first; row <= last; ++row) {
            const QModelIndex index = m_model->index(row, 0);
            const QString baseName = getBaseName(index);
            const QString filePath = dir.absoluteFilePath(baseName);
            QVariantMap oldMimeToExtension = getMimeToExtensionMap(index);
            QVariantMap mimeToExtension;
            QVariantMap dataMap;
            const QList<Ext> exts = fileExtensionsAndFormats();

            const QVariantMap itemData = index.data(contentType::data).toMap();
            bool noSave = itemData.contains(mimeNoSave);

            if ( noSave && itemData.contains(mimeUriList) ) {
                const QByteArray uriList = itemData[mimeUriList].toByteArray();
                foreach (const QByteArray &uri, uriList.split('\n')) {
                    const QString path = QUrl::fromEncoded(uri).toLocalFile();
                    foreach (const QByteArray &key, existingFiles.keys()) {
                        if ( existingFiles.remove(key, path) > 0 )
                            break;
                    }
                }
            }

            foreach ( const QString &format, itemData.keys() ) {
                if ( format.startsWith(MIME_PREFIX_ITEMSYNC) )
                    continue; // skip internal data
                if ( noSave && (format == mimeUriList || format == mimeUriList) )
                    continue;

                const QVariant value = itemData[format];

                if (format == mimeUnknownData) {
                    if ( !saveItemFile(filePath, value.toByteArray(), &existingFiles) )
                        return;
                } else {
                    bool hasFile = oldMimeToExtension.contains(format);
                    const QString ext = findByFormat(format, exts, oldMimeToExtension).extension;

                    if ( !hasFile && ext.isEmpty() ) {
                        dataMap.insert(format, value);
                    } else {
                        mimeToExtension.insert(format, ext);
                        if ( !saveItemFile(filePath + ext, value.toByteArray(), &existingFiles) )
                            return;
                    }
                }
            }

            if ( !dataMap.isEmpty() ) {
                mimeToExtension.insert("", "");
                QByteArray data = serializeData(dataMap);
                if ( !saveItemFile(filePath + dataFileSuffix, data, &existingFiles) )
                    return;
            }

            if ( mimeToExtension != oldMimeToExtension ) {
                QByteArray newMimeToExtension;
                foreach ( const QString &format, mimeToExtension.keys() ) {
                    oldMimeToExtension.remove(format);
                    newMimeToExtension.append(format + " " + mimeToExtension[format].toString() + "\n");
                }

                QVariantMap itemData;
                itemData.insert(mimeExtensionMap, newMimeToExtension);
                m_model->setData(index, itemData, contentType::updateData);

                // Remove files of removed formats.
                removeFormatFiles(filePath, oldMimeToExtension);
            }
        }

        connectModel();
    }

    QFileSystemWatcher m_watcher;
    QPointer<QAbstractItemModel> m_model;
    QTimer m_updateTimer;
    const QList<FileFormat> &m_formatSettings;
    QString m_path;
    bool m_valid;
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

    // Apply settings from tab sync path table.
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
    m_settings.insert(configSyncTabs, tabPaths);

    // Apply settings from file format table.
    t = ui->tableWidgetFormatSettings;
    QVariantList formatSettings;
    m_formatSettings.clear();
    for (int row = 0; row < t->rowCount(); ++row) {
        FileFormat fileFormat;
        fileFormat.formats = t->item(row, formatSettingsTableColumns::formats)->text()
                .split( QRegExp("[,;\\s]"), QString::SkipEmptyParts );
        fileFormat.itemMime = t->item(row, formatSettingsTableColumns::itemMime)->text();
        if ( fileFormat.formats.isEmpty() && fileFormat.itemMime.isEmpty() )
            continue;
        fileFormat.icon = t->cellWidget(row, formatSettingsTableColumns::icon)
                ->property("currentIcon").toString();
        m_formatSettings.append(fileFormat);

        QVariantMap format;
        format["formats"] = fileFormat.formats;
        format["itemMime"] = fileFormat.itemMime;
        format["icon"] = fileFormat.icon;
        formatSettings.append(format);
    }
    m_settings.insert(configFormatSettings, formatSettings);

    // Update data of items with watched files.
    foreach ( const QObject *model, m_watchers.keys() )
        m_watchers[model]->updateItems();

    return m_settings;
}

void ItemSyncLoader::loadSettings(const QVariantMap &settings)
{
    m_settings = settings;

    m_tabPaths.clear();
    const QStringList tabPaths = m_settings.value(configSyncTabs).toStringList();
    for (int i = 0; i < tabPaths.size(); i += 2)
        m_tabPaths.insert( tabPaths[i], tabPaths.value(i + 1) );

    m_formatSettings.clear();
    const QVariantList formatSettings = m_settings.value(configFormatSettings).toList();
    for (int i = 0; i < formatSettings.size(); ++i) {
        QVariantMap format = formatSettings[i].toMap();
        FileFormat fileFormat;
        fileFormat.formats = format.value("formats").toStringList();
        fileFormat.itemMime = format.value("itemMime").toString();
        fileFormat.icon = format.value("icon").toString();
        m_formatSettings.append(fileFormat);
    }
}

void setNormalStretchFixedColumns(QTableWidget *table, int normalColumn, int stretchColumn, int fixedColumn)
{
    QHeaderView *header = table->horizontalHeader();
    setHeaderSectionResizeMode(header, stretchColumn, QHeaderView::Stretch);
    setHeaderSectionResizeMode(header, fixedColumn, QHeaderView::Fixed);
    header->resizeSection(fixedColumn, table->rowHeight(0));
    table->resizeColumnToContents(normalColumn);
}

QWidget *ItemSyncLoader::createSettingsWidget(QWidget *parent)
{
    delete ui;
    ui = new Ui::ItemSyncSettings;
    QWidget *w = new QWidget(parent);
    ui->setupUi(w);

    // Init tab sync path table.
    const QStringList tabPaths = m_settings.value(configSyncTabs).toStringList();
    QTableWidget *t = ui->tableWidgetSyncTabs;
    for (int row = 0, i = 0; i < tabPaths.size() + 20; ++row, i += 2) {
        t->insertRow(row);
        t->setItem( row, syncTabsTableColumns::tabName, new QTableWidgetItem(tabPaths.value(i)) );
        t->setItem( row, syncTabsTableColumns::path, new QTableWidgetItem(tabPaths.value(i + 1)) );

        QPushButton *button = createBrowseButton();
        t->setCellWidget(row, syncTabsTableColumns::browse, button);
        connect( button, SIGNAL(clicked()), SLOT(onBrowseButtonClicked()) );
    }
    setNormalStretchFixedColumns(t, syncTabsTableColumns::tabName, syncTabsTableColumns::path,
                                 syncTabsTableColumns::browse);

    // Init file format table.
    const QVariantList formatSettings = m_settings.value(configFormatSettings).toList();
    t = ui->tableWidgetFormatSettings;
    for (int row = 0; row < formatSettings.size() + 10; ++row) {
        const QVariantMap format = formatSettings.value(row).toMap();
        const QString formats = format.value("formats").toStringList().join(", ");
        t->insertRow(row);
        t->setItem( row, formatSettingsTableColumns::formats, new QTableWidgetItem(formats) );
        t->setItem( row, formatSettingsTableColumns::itemMime, new QTableWidgetItem(format.value("itemMime").toString()) );

        IconSelectButton *button = new IconSelectButton();
        button->setCurrentIcon( format.value("icon").toString() );
        t->setCellWidget(row, formatSettingsTableColumns::icon, button);
    }
    setNormalStretchFixedColumns(t, formatSettingsTableColumns::formats,
                                 formatSettingsTableColumns::itemMime,
                                 formatSettingsTableColumns::icon);

    return w;
}

bool ItemSyncLoader::loadItems(QAbstractItemModel *model, QFile *file)
{
    QVariantMap config;
    bool syncTab = shouldSyncTab(*model);

    if ( !readConfig(file, &config) ) {
        if (syncTab) {
            model->setProperty(propertyModelDirty, true);
            createWatcher( model, tabPath(*model), QStringList() );
        }
        return false;
    }

    model->setProperty(propertyModelDisabled, true);

    QStringList files = config.value(configSavedFiles).toStringList();

    if (syncTab) {
        const QString path = tabPath(*model);
        if ( !path.isEmpty() ) {
            QDir dir(path);
            if ( !dir.mkpath(".") )
                return true;

            foreach ( const QString &fileName, dir.entryList(itemFileFilter(), QDir::Time | QDir::Reversed) ) {
                const QString filePath = dir.absoluteFilePath(fileName);
                if ( !files.contains(filePath) )
                    files.append(filePath);
            }

            // Monitor files in directory.
            FileWatcher *watcher = createWatcher(model, dir.path(), files);

            watcher->createItemsFromFiles(dir, files);
            if ( !watcher->isValid() )
                return true;
        }
    } else {
        // Synchronization was disabled for this tab, save items again.
        model->setProperty(propertyModelDirty, true);
    }

    model->setProperty(propertyModelDisabled, false);

    return true;
}

bool ItemSyncLoader::saveItems(const QAbstractItemModel &model, QFile *file)
{
    if ( !shouldSyncTab(model) )
        return false;

    // If anything fails, just return false so the items are save regularly.

    // Don't save items if path is empty.
    const QString path = tabPath(model);
    if ( path.isEmpty() )
        return true;

    FileWatcher *watcher = m_watchers.value(&model, NULL);
    Q_ASSERT(watcher);
    Q_ASSERT(watcher->path() == path);

    if ( !watcher->isValid() ) {
        log( tr("Failed to synchronize tab \"%1\" with directory \"%2\"!")
             .arg(model.property("tabName").toString())
             .arg(watcher->path()),
             LogError );
        return false;
    }

    QDir dir(path);

    QStringList savedFiles;

    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);

        const QVariantMap mimeToExtension = getMimeToExtensionMap(index);

        QString baseName = getBaseName(index);

        const QString filePath = dir.absoluteFilePath(baseName);

        bool saveDataFile = false;
        const QList<Ext> exts = fileExtensionsAndFormats();

        const QVariantMap itemData = index.data(contentType::data).toMap();
        bool noSave = itemData.contains(mimeNoSave);

        if (noSave)
            savedFiles.prepend(filePath);

        foreach ( const QString &format, itemData.keys() ) {
            if ( format.startsWith(MIME_PREFIX_ITEMSYNC) )
                continue; // skip internal data
            if ( noSave && (format == mimeUriList || format == mimeUriList) )
                continue;

            if (format == mimeUnknownData) {
                savedFiles.prepend(filePath);
            } else {
                bool hasFile = mimeToExtension.contains(format);
                const QString ext = findByFormat(format, exts, mimeToExtension).extension;

                if ( !hasFile && ext.isEmpty() )
                    saveDataFile = true;
                else
                    savedFiles.prepend(filePath + ext);
            }
        }

        if (saveDataFile)
            savedFiles.prepend(filePath + dataFileSuffix);
    }

    writeConfiguration(file, savedFiles);

    return true;
}

bool ItemSyncLoader::createTab(QAbstractItemModel *model, QFile *file)
{
    if ( !shouldSyncTab(*model) )
        return false;

    QDir dir( tabPath(*model) );
    QStringList savedFiles;
    foreach ( const QString &fileName, dir.entryList(itemFileFilter(), QDir::Name | QDir::Reversed) )
        savedFiles.append( dir.absoluteFilePath(fileName) );

    writeConfiguration(file, savedFiles);

    file->seek(0);
    loadItems(model, file);

    return true;
}

ItemWidget *ItemSyncLoader::transform(ItemWidget *itemWidget, const QModelIndex &index)
{
    const QString baseName = getBaseName(index);
    if ( baseName.isEmpty() )
        return NULL;

    const QVariantMap dataMap = index.data(contentType::data).toMap();
    bool noSave = dataMap.contains(mimeNoSave);

    QString icon;
    if (noSave) {
        icon = iconFromBaseNameExtension(baseName, m_formatSettings);
    } else {
        const QVariantMap mimeToExtension = getMimeToExtensionMap(index);
        foreach ( const QString &format, dataMap.keys() ) {
            if ( format.startsWith(MIME_PREFIX) )
                continue; // skip internal data
            icon = mimeToExtension.contains(format)
                    ? iconFromBaseNameExtension(baseName + mimeToExtension[format].toString(), m_formatSettings)
                    : iconFromMime(format);
            if ( !icon.isNull() )
                break;
        }
    }

    if ( icon.isNull() )
        icon = iconFromId(IconFile);

    return new ItemSync(baseName, icon, noSave, itemWidget);
}

bool ItemSyncLoader::canRemoveItems(const QList<QModelIndex> &indexList)
{
    return !containsItemsWithFiles(indexList)
            || QMessageBox::question( NULL, tr("Remove Items?"),
                                      tr("Do you really want to <strong>remove items and associated files</strong>?"),
                                      QMessageBox::No | QMessageBox::Yes,
                                      QMessageBox::Yes ) == QMessageBox::Yes;
}

void ItemSyncLoader::itemsRemovedByUser(const QList<QModelIndex> &indexList)
{
    // Remove unneeded files (remaining records in the hash map).
    foreach (const QModelIndex &index, indexList) {
        const QAbstractItemModel *model = index.model();
        if (!model)
            continue;

        const QString path = tabPath(*index.model());
        if ( path.isEmpty() )
            continue;

        const QString baseName = getBaseName(index);
        if ( baseName.isEmpty() )
            continue;

        // Check if item is still present in list (drag'n'drop).
        bool remove = true;
        for (int i = 0; i < model->rowCount(); ++i) {
            const QModelIndex index2 = model->index(i, 0);
            if ( index2 != index && baseName == getBaseName(index2) ) {
                remove = false;
                break;
            }
        }
        if (!remove)
            continue;

        const QVariantMap mimeToExtension = getMimeToExtensionMap(index);
        if ( mimeToExtension.isEmpty() )
            QFile::remove(path + '/' + baseName);
        else
            removeFormatFiles(path + '/' + baseName, mimeToExtension);
    }
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
    for ( ; row < t->rowCount() && t->cellWidget(row, syncTabsTableColumns::browse) != button; ++row ) {}
    Q_ASSERT(row != t->rowCount());


    QTableWidgetItem *item = t->item(row, syncTabsTableColumns::path);
    const QString path =
            QFileDialog::getExistingDirectory( t, tr("Open Directory for Synchronization"), item->text() );
    if ( !path.isEmpty() )
        item->setText(path);
}

bool ItemSyncLoader::shouldSyncTab(const QAbstractItemModel &model) const
{
    return m_tabPaths.contains(model.property("tabName").toString());
}

QString ItemSyncLoader::tabPath(const QAbstractItemModel &model) const
{
    const QString tabName = model.property("tabName").toString();
    return m_tabPaths.value(tabName);
}

FileWatcher *ItemSyncLoader::createWatcher(QAbstractItemModel *model, const QString &tabPath,
                                           const QStringList &paths)
{
    FileWatcher *watcher = new FileWatcher(tabPath, paths, model, m_formatSettings, this);
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
