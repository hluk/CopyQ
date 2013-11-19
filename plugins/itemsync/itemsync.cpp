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

#include <QBoxLayout>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QScopedPointer>
#include <QTextEdit>
#include <QTimer>
#include <QtPlugin>
#include <QUrl>
#include <QVariantMap>

struct FileFormat {
    bool isValid() const { return !extensions.isEmpty(); }
    QStringList extensions;
    QString itemMime;
    QString icon;
};

namespace {

const int currentVersion = 1;
const char dataFileHeader[] = "CopyQ_itemsync_tab";

const char configVersion[] = "copyq_itemsync_version";
const char configPath[] = "path";
const char configSyncTabs[] = "sync_tabs";
const char configFormatSettings[] = "format_settings";

const char tabConfigSavedFiles[] = "saved_files";

const char dataFileSuffix[] = "_copyq.dat";

#define MIME_PREFIX_ITEMSYNC MIME_PREFIX "itemsync-"
const char mimeExtensionMap[] = MIME_PREFIX_ITEMSYNC "mime-to-extension-map";
const char mimeBaseName[] = MIME_PREFIX_ITEMSYNC "basename";
const char mimeNoSave[] = MIME_PREFIX_ITEMSYNC "no-save";
const char mimeSyncPath[] = MIME_PREFIX_ITEMSYNC "sync-path";
const char mimeNoFormat[] = MIME_PREFIX_ITEMSYNC "no-format";
const char mimeUnknownFormats[] = MIME_PREFIX_ITEMSYNC "unknown-formats";

const char propertyModelDisabled[] = "disabled";
const char propertyModelDirty []= "dirty";

const int updateItemsIntervalMs = 2000; // Interval to update items after a file has changed.

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

bool readConfigHeader(QDataStream *stream)
{
    QString header;
    *stream >> header;
    return header == dataFileHeader;
}

bool readConfig(QFile *file, QVariantMap *config)
{
    QDataStream stream(file);
    if ( !readConfigHeader(&stream) )
        return false;
    stream >> *config;
    return config->value(configVersion, 0).toInt() == currentVersion;
}

void writeConfiguration(QFile *file, const QStringList &savedFiles)
{
    QVariantMap config;
    config.insert(configVersion, currentVersion);
    config.insert(tabConfigSavedFiles, savedFiles);

    QDataStream stream(file);
    stream.setVersion(QDataStream::Qt_4_7);
    stream << QString(dataFileHeader);
    stream << config;
}

FileFormat getFormatSettingsFromFileName(const QString &fileName,
                                         const QList<FileFormat> &formatSettings,
                                         QString *foundExt = NULL)
{
    for ( int i = 0; i < formatSettings.size(); ++i ) {
        const FileFormat &format = formatSettings[i];
        foreach ( const QString &ext, format.extensions ) {
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

void getBaseNameAndExtension(const QString &fileName, QString *baseName, QString *ext,
                             const QList<FileFormat> &formatSettings)
{
    const FileFormat fileFormat = getFormatSettingsFromFileName(fileName, formatSettings, ext);

    ext->clear();

    if ( !fileFormat.isValid() ) {
        const int i = fileName.lastIndexOf('.');
        if (i != -1)
            *ext = fileName.mid(i);
    }

    *baseName = fileName.left( fileName.size() - ext->size() );

    if ( baseName->endsWith('.') ) {
        baseName->chop(1);
        ext->prepend('.');
    }
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
        exts.append( Ext(".svg", "image/svg+xml") );
        exts.append( Ext(".xml", "text/xml") );
    }

    return exts;
}

QString findByFormat(const QString &format, const QList<FileFormat> &formatSettings)
{
    // Find in default extensions.
    const QList<Ext> &exts = fileExtensionsAndFormats();

    for (int i = 0; i < exts.size(); ++i) {
        const Ext &ext = exts[i];
        if (ext.format == format)
            return ext.extension;
    }

    // Find in user defined extensions.
    foreach (const FileFormat &fileFormat, formatSettings) {
        if ( !fileFormat.extensions.isEmpty() && fileFormat.itemMime != "-"
             && format == fileFormat.itemMime )
        {
            return fileFormat.extensions.first();
        }
    }

    return QString();
}

Ext findByExtension(const QString &fileName, const QList<FileFormat> &formatSettings)
{
    // Is internal data format?
    if ( fileName.endsWith(dataFileSuffix) )
        return Ext(dataFileSuffix, mimeUnknownFormats);

    // Find in user defined formats.
    bool hasUserFormat = false;
    foreach (const FileFormat &format, formatSettings) {
        foreach (const QString &ext, format.extensions) {
            if ( fileName.endsWith(ext) ) {
                if ( format.itemMime.isEmpty() )
                    hasUserFormat = true;
                else
                    return Ext( QString(), format.itemMime );
            }
        }
    }

    // Find in default formats.
    const QList<Ext> &exts = fileExtensionsAndFormats();

    for (int i = 0; i < exts.size(); ++i) {
        const Ext &ext = exts[i];
        if ( fileName.endsWith(ext.extension) )
            return ext;
    }

    return hasUserFormat ? Ext(QString(), mimeNoFormat) : Ext();
}

Hash calculateHash(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1);
}

bool saveItemFile(const QString &filePath, const QByteArray &bytes,
                  QStringList *existingFiles, bool hashChanged = true)
{
    if ( !existingFiles->removeOne(filePath) || hashChanged ) {
        QFile f(filePath);
        if ( !f.open(QIODevice::WriteOnly) || f.write(bytes) == -1 ) {
            log( QString("ItemSync: %1").arg(f.errorString()), LogError );
            return false;
        }
    }

    return true;
}

struct BaseNameExtensions {
    explicit BaseNameExtensions(const QString &baseName = QString(),
                                const QList<Ext> &exts = QList<Ext>())
        : baseName(baseName)
        , exts(exts) {}
    QString baseName;
    QList<Ext> exts;
};

bool canUseFile(QFileInfo &info)
{
    return !info.isHidden() && !info.fileName().startsWith('.') && info.isReadable();
}

bool getBaseNameExtension(const QString &filePath, const QList<FileFormat> &formatSettings,
                          QString *baseName, Ext *ext)
{
    QFileInfo info(filePath);
    if ( !canUseFile(info) )
        return false;

    *ext = findByExtension(filePath, formatSettings);
    if ( ext->format.isEmpty() || ext->format == "-" )
        return false;

    const QString fileName = info.fileName();
    *baseName = fileName.left( fileName.size() - ext->extension.size() );

    return true;
}

typedef QList<BaseNameExtensions> BaseNameExtensionsList;
BaseNameExtensionsList listFiles(const QStringList &files,
                                 const QList<FileFormat> &formatSettings)
{
    BaseNameExtensionsList fileList;
    QMap<QString, int> fileMap;

    foreach (const QString &filePath, files) {
        QString baseName;
        Ext ext;
        if ( getBaseNameExtension(filePath, formatSettings, &baseName, &ext) ) {
            int i = fileMap.value(baseName, -1);
            if (i == -1) {
                i = fileList.size();
                fileList.append( BaseNameExtensions(baseName) );
                fileMap.insert(baseName, i);
            }

            fileList[i].exts.append(ext);
        }
    }

    return fileList;
}

/// Load hash of all existing files to map (hash -> filename).
QStringList listFiles(const QDir &dir)
{
    QStringList files;

    foreach ( const QString &fileName, dir.entryList(itemFileFilter()) ) {
        const QString path = dir.absoluteFilePath(fileName);
        QFileInfo info(path);
        if ( canUseFile(info) )
            files.append(path);
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
    if ( !fileFormat.icon.isEmpty() )
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

bool containsUserData(const QVariantMap &dataMap)
{
    foreach ( const QString &format, dataMap.keys() ) {
        if ( !format.startsWith(MIME_PREFIX) )
            return true;
    }

    return false;
}

void fixUserExtensions(QStringList *exts)
{
    for (int i = 0; i < exts->size(); ++i) {
        QString &ext = (*exts)[i];
        if ( !ext.startsWith('.') )
            ext.prepend('.');
        // Use "_user.dat" instead of "*.dat" to avoid collisions with extension "_copy.dat"
        // internally used to store data of unknown MIME type.
        if ( ext.toLower().endsWith(".dat") )
            ext.insert(ext.size() - 4, "_user");
        // Remove invalid extensions containing path separator.
        if ( ext.contains('/') )
            exts->removeAt(i--);
    }
}

void fixUserMimeType(QString *mimeType)
{
    // Don't allow user to override internal formats.
    if ( mimeType->startsWith(MIME_PREFIX_ITEMSYNC) )
        mimeType->clear();
}

} // namespace

ItemSync::ItemSync(const QString &label, const QString &icon, ItemWidget *childItem)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_label( new QTextEdit(this) )
    , m_icon( new IconWidget(icon, this) )
    , m_childItem(childItem)
    , m_copyOnMouseUp(false)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    layout->setSpacing(0);

    QHBoxLayout *labelLayout = new QHBoxLayout;
    labelLayout->setMargin(0);

    labelLayout->addWidget(m_icon);
    labelLayout->addWidget(m_label);
    labelLayout->addStretch();

    layout->addLayout(labelLayout);

    QWidget *w = m_childItem->widget();
    layout->addWidget(w);
    w->setObjectName("item_child");
    w->setParent(this);

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
}

void ItemSync::highlight(const QRegExp &re, const QFont &highlightFont, const QPalette &highlightPalette)
{
    m_childItem->setHighlight(re, highlightFont, highlightPalette);

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

    update();
}

QWidget *ItemSync::createEditor(QWidget *parent) const
{
    return m_childItem->createEditor(parent);
}

void ItemSync::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    return m_childItem->setEditorData(editor, index);
}

void ItemSync::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    return m_childItem->setModelData(editor, model, index);
}

bool ItemSync::hasChanges(QWidget *editor) const
{
    return m_childItem->hasChanges(editor);
}

void ItemSync::updateSize(const QSize &maximumSize)
{
    setMaximumSize(maximumSize);

    const int w = maximumSize.width() - m_icon->width() - 8;
    QTextDocument *doc = m_label->document();
    doc->setTextWidth(w);
    m_label->setFixedSize( doc->idealWidth() + 16, doc->size().height() );

    m_childItem->updateSize(maximumSize);

    adjustSize();
    setFixedSize(sizeHint());
}

void ItemSync::mousePressEvent(QMouseEvent *e)
{
    const QPoint pos = m_label->viewport()->mapFrom(this, e->pos());
    m_label->setTextCursor( m_label->cursorForPosition(pos) );
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
    if (m_copyOnMouseUp) {
        m_copyOnMouseUp = false;
        if ( m_label->textCursor().hasSelection() )
            m_label->copy();
    } else {
        QWidget::mouseReleaseEvent(e);
    }
}

void removeFormatFiles(const QString &path, const QVariantMap &mimeToExtension)
{
    foreach ( const QVariant &extValue, mimeToExtension.values() )
        QFile::remove(path + extValue.toString());
}

void moveFormatFiles(const QString &oldPath, const QString &newPath,
                     const QVariantMap &mimeToExtension)
{
    foreach ( const QVariant &extValue, mimeToExtension.values() ) {
        const QString ext = extValue.toString();
        QFile::rename(oldPath + ext, newPath + ext);
    }
}

void copyFormatFiles(const QString &oldPath, const QString &newPath,
                     const QVariantMap &mimeToExtension)
{
    foreach ( const QVariant &extValue, mimeToExtension.values() ) {
        const QString ext = extValue.toString();
        QFile::copy(oldPath + ext, newPath + ext);
    }
}

void ItemSync::onSelectionChanged()
{
    m_copyOnMouseUp = true;
}

class FileWatcher : public QObject {
    Q_OBJECT

public:
    FileWatcher(const QString &path, const QStringList &paths, QAbstractItemModel *model,
                const QList<FileFormat> &formatSettings, QObject *parent)
        : QObject(parent)
        , m_watcher()
        , m_model(model)
        , m_formatSettings(formatSettings)
        , m_path(path)
        , m_valid(false)
        , m_indexData()
        , m_usedBaseNames()
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

        createItemsFromFiles( QDir(path), listFiles(paths, m_formatSettings) );
    }

    void createItemsFromFiles(const QDir &dir, const BaseNameExtensionsList &fileList)
    {
        disconnectModel();

        const int maxItems = m_model->property("maxItems").toInt();

        foreach (const BaseNameExtensions &baseNameWithExts, fileList) {
            if ( !createItemFromFiles(dir, baseNameWithExts, 0) )
                return;
            if ( m_model->rowCount() >= maxItems )
                break;
        }

        connectModel();
    }

    bool createItemFromFiles(const QDir &dir, const BaseNameExtensions &baseNameWithExts, int targetRow)
    {
        QVariantMap dataMap;
        QVariantMap mimeToExtension;

        updateDataAndWatchFile(dir, baseNameWithExts, &dataMap, &mimeToExtension);

        if ( !mimeToExtension.isEmpty() ) {
            dataMap.insert( mimeBaseName, QFileInfo(baseNameWithExts.baseName).fileName() );
            dataMap.insert(mimeExtensionMap, mimeToExtension);

            if ( !createItem(dataMap, targetRow) )
                return false;
        }

        return true;
    }

    const QString &path() const { return m_path; }

    bool isValid() const { return m_valid; }

    QAbstractItemModel *model() const { return m_model; }

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
        QStringList files;
        foreach ( const QString &fileName, dir.entryList(itemFileFilter(), QDir::Time | QDir::Reversed) )
            files.append( dir.absoluteFilePath(fileName) );
        BaseNameExtensionsList fileList = listFiles(files, m_formatSettings);

        for ( int row = 0; row < m_model->rowCount(); ++row ) {
            const QModelIndex index = m_model->index(row, 0);
            const QString baseName = getBaseName(index);

            int i = 0;
            for ( i = 0; i < fileList.size() && fileList[i].baseName != baseName; ++i ) {}

            QVariantMap dataMap;
            QVariantMap mimeToExtension;

            if ( i < fileList.size() ) {
                updateDataAndWatchFile(dir, fileList[i], &dataMap, &mimeToExtension);
                fileList.removeAt(i);
            }

            if ( mimeToExtension.isEmpty() ) {
                m_model->removeRow(row--);
            } else {
                dataMap.insert(mimeBaseName, baseName);
                dataMap.insert(mimeExtensionMap, mimeToExtension);
                updateIndexData(index, dataMap);
            }
        }

        createItemsFromFiles(dir, fileList);

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

    void onRowsRemoved(const QModelIndex &, int first, int last)
    {
        foreach ( const QPersistentModelIndex &index, indexList(first, last) ) {
            QMap<QPersistentModelIndex, IndexData>::iterator it = m_indexData.find(index);
            Q_ASSERT( it != m_indexData.end() );
            Q_ASSERT( m_usedBaseNames.contains(it.value().baseName) );
            m_usedBaseNames.remove(it.value().baseName);
            m_indexData.erase(it);
        }
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
                 this, SLOT(onRowsInserted(QModelIndex, int, int)), Qt::UniqueConnection );
        connect( m_model.data(), SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)),
                 this, SLOT(onRowsRemoved(QModelIndex, int, int)), Qt::UniqueConnection );
        connect( m_model.data(), SIGNAL(dataChanged(QModelIndex,QModelIndex)),
                 SLOT(onDataChanged(QModelIndex,QModelIndex)), Qt::UniqueConnection );
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

    bool createItem(const QVariantMap &dataMap, int targetRow)
    {
        const int row = qMax( 0, qMin(targetRow, m_model->rowCount()) );
        if ( m_model->insertRow(row) ) {
            const QModelIndex &index = m_model->index(row, 0);
            updateIndexData(index, dataMap);
            return true;
        }

        return false;
    }

    void updateIndexData(const QPersistentModelIndex &index, const QVariantMap &itemData)
    {
        m_model->setData(index, itemData, contentType::data);

        // Item base name is non-empty.
        const QString baseName = getBaseName(index);
        Q_ASSERT( !baseName.isEmpty() );

        const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();

        IndexData &data = m_indexData[index];

        data.baseName = baseName;
        m_usedBaseNames.insert(baseName);

        QMap<QString, Hash> &formatData = data.formatHash;
        formatData.clear();

        foreach ( const QString &format, mimeToExtension.keys() ) {
            if ( !format.startsWith(MIME_PREFIX_ITEMSYNC) )
                formatData.insert(format, calculateHash(itemData.value(format).toByteArray()) );
        }
    }

    QList<QPersistentModelIndex> indexList(int first, int last)
    {
        QList<QPersistentModelIndex> indexList;
        for (int i = first; i <= last; ++i)
            indexList.append( m_model->index(i, 0) );
        return indexList;
    }

    void saveItems(int first, int last)
    {
        disconnectModel();

        const QList<QPersistentModelIndex> indexList = this->indexList(first, last);

        if ( !renameMoveCopy(indexList) )
            return;

        if ( m_path.isEmpty() )
            return;

        // Create path if doesn't exist.
        QDir dir(m_path);
        if ( !dir.mkpath(".") ) {
            log( tr("Failed to create synchronization directory \"%1\"!").arg(m_path) );
            return;
        }

        QStringList existingFiles = listFiles(dir);

        foreach (const QPersistentModelIndex &index, indexList) {
            if ( !index.isValid() )
                continue;

            const QString baseName = getBaseName(index);
            const QString filePath = dir.absoluteFilePath(baseName);
            QVariantMap itemData = index.data(contentType::data).toMap();
            QVariantMap oldMimeToExtension = itemData.value(mimeExtensionMap).toMap();
            QVariantMap mimeToExtension;
            QVariantMap dataMapUnknown;

            const QVariantMap noSaveData = itemData.value(mimeNoSave).toMap();

            foreach ( const QString &format, itemData.keys() ) {
                if ( format.startsWith(MIME_PREFIX_ITEMSYNC) )
                    continue; // skip internal data

                const QByteArray bytes = itemData[format].toByteArray();
                const Hash hash = calculateHash(bytes);

                if ( noSaveData.contains(format) && noSaveData[format].toByteArray() == hash ) {
                    itemData.remove(format);
                    continue;
                }

                bool hasFile = oldMimeToExtension.contains(format);
                const QString ext = hasFile ? oldMimeToExtension[format].toString()
                                            : findByFormat(format, m_formatSettings);

                if ( !hasFile && ext.isEmpty() ) {
                    dataMapUnknown.insert(format, bytes);
                } else {
                    mimeToExtension.insert(format, ext);
                    Q_ASSERT( m_indexData.contains(index) );
                    const Hash oldHash = m_indexData[index].formatHash.value(format);
                    if ( !saveItemFile(filePath + ext, bytes, &existingFiles, hash != oldHash) )
                        return;
                }
            }

            for ( QVariantMap::const_iterator it = oldMimeToExtension.begin();
                  it != oldMimeToExtension.end(); ++it )
            {
                if ( it.key().startsWith(mimeNoFormat) )
                    mimeToExtension.insert( it.key(), it.value() );
            }

            if ( mimeToExtension.isEmpty() || !dataMapUnknown.isEmpty() ) {
                mimeToExtension.insert(mimeUnknownFormats, dataFileSuffix);
                QByteArray data = serializeData(dataMapUnknown);
                if ( !saveItemFile(filePath + dataFileSuffix, data, &existingFiles) )
                    return;
            }

            if ( !noSaveData.isEmpty() || mimeToExtension != oldMimeToExtension ) {
                itemData.remove(mimeNoSave);

                foreach ( const QString &format, mimeToExtension.keys() )
                    oldMimeToExtension.remove(format);

                itemData.insert(mimeExtensionMap, mimeToExtension);
                updateIndexData(index, itemData);

                // Remove files of removed formats.
                removeFormatFiles(filePath, oldMimeToExtension);
            }
        }

        connectModel();
    }

    bool renameToUnique(QString *name, bool newFile)
    {
        if ( name->isEmpty() ) {
            *name = "copyq_0000";
        } else {
            // Replace/remove unsafe characters.
            name->replace( QRegExp("/|\\\\|^\\."), QString("_") );
            name->remove( QRegExp("\\n|\\r") );
        }

        if ( !m_usedBaseNames.contains(*name) ) {
            m_usedBaseNames.insert(*name);
            return true;
        }

        QString ext;
        QString baseName;
        getBaseNameAndExtension(*name, &baseName, &ext, m_formatSettings);

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
        } while ( m_usedBaseNames.contains(newName) || (newFile && QFile::exists(m_path + '/' + newName)) );

        *name = newName;
        m_usedBaseNames.insert(*name);

        return true;
    }

    bool renameMoveCopy(const QList<QPersistentModelIndex> &indexList)
    {
        foreach (const QPersistentModelIndex &index, indexList) {
            if ( !index.isValid() )
                continue;

            const QString olderBaseName = m_indexData.value(index).baseName;
            const QString oldBaseName = getBaseName(index);
            QString baseName = oldBaseName;

            bool newItem = olderBaseName.isEmpty();
            bool itemRenamed = olderBaseName != baseName;
            if ( newItem || itemRenamed ) {
                if ( !renameToUnique(&baseName, oldBaseName.isEmpty()) )
                    return false;
                itemRenamed = olderBaseName != baseName;
            }

            QVariantMap itemData = index.data(contentType::data).toMap();
            const QString syncPath = itemData.value(mimeSyncPath).toString();
            bool copyFilesFromOtherTab = !syncPath.isEmpty() && syncPath != m_path;

            if (copyFilesFromOtherTab || itemRenamed) {
                const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();
                const QString newBasePath = m_path + '/' + baseName;

                if ( !syncPath.isEmpty() ) {
                    copyFormatFiles(syncPath + '/' + oldBaseName, newBasePath, mimeToExtension);
                } else {
                    // Move files.
                    if ( !olderBaseName.isEmpty() )
                        moveFormatFiles(m_path + '/' + olderBaseName, newBasePath, mimeToExtension);
                }

                itemData.remove(mimeSyncPath);
                itemData.insert(mimeBaseName, baseName);
                updateIndexData(index, itemData);

                if ( oldBaseName.isEmpty() && itemData.contains(mimeUriList) ) {
                    if ( copyFilesFromUriList(itemData[mimeUriList].toByteArray(), index.row()) )
                         m_model->removeRow(index.row());
                }
            }
        }

        return true;
    }

    void updateDataAndWatchFile(const QDir &dir, const BaseNameExtensions &baseNameWithExts,
                                QVariantMap *dataMap, QVariantMap *mimeToExtension)
    {
        const QString basePath = dir.absoluteFilePath(baseNameWithExts.baseName);

        foreach (const Ext &ext, baseNameWithExts.exts) {
            Q_ASSERT( !ext.format.isEmpty() );

            const QString fileName = basePath + ext.extension;

            QFile f( dir.absoluteFilePath(fileName) );
            if ( !f.open(QIODevice::ReadOnly) )
                continue;

            if ( ext.extension == dataFileSuffix && deserializeData(dataMap, f.readAll()) ) {
                mimeToExtension->insert(mimeUnknownFormats, dataFileSuffix);
            } else if ( f.size() > sizeLimit || ext.format.startsWith(mimeNoFormat)
                        || dataMap->contains(ext.format) )
            {
                mimeToExtension->insert(mimeNoFormat + ext.extension, ext.extension);
            } else {
                dataMap->insert(ext.format, f.readAll());
                mimeToExtension->insert(ext.format, ext.extension);
            }

            watchPath(fileName);
        }
    }

    bool copyFilesFromUriList(const QByteArray &uriData, int targetRow)
    {
        QMimeData tmpData;
        tmpData.setData(mimeUriList, uriData);

        bool copied = false;

        foreach ( const QUrl &url, tmpData.urls() ) {
            if ( url.isLocalFile() ) {
                QFile f(url.toLocalFile());

                if (f.exists()) {
                    QString ext;
                    QString baseName;
                    getBaseNameAndExtension( QFileInfo(f).fileName(), &baseName, &ext,
                                             m_formatSettings );

                    if ( renameToUnique(&baseName, true) ) {
                        const QString targetFilePath = m_path + '/' + baseName + ext;
                        f.copy(targetFilePath);
                        if ( m_model->rowCount() < m_model->property("maxItems").toInt() ) {
                            QString baseName;
                            Ext ext;
                            if ( getBaseNameExtension(targetFilePath, m_formatSettings, &baseName, &ext) ) {
                                BaseNameExtensions baseNameExts(baseName, QList<Ext>() << ext);
                                createItemFromFiles( QDir(m_path), baseNameExts, targetRow );
                                copied = true;
                            }
                        }
                    }
                }
            }
        }

        return copied;
    }

    struct IndexData {
        QString baseName;
        QMap<QString, Hash> formatHash;
    };

    QFileSystemWatcher m_watcher;
    QPointer<QAbstractItemModel> m_model;
    QTimer m_updateTimer;
    const QList<FileFormat> &m_formatSettings;
    QString m_path;
    bool m_valid;
    QMap<QPersistentModelIndex, IndexData> m_indexData;
    QSet<QString> m_usedBaseNames;
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
        fileFormat.extensions = t->item(row, formatSettingsTableColumns::formats)->text()
                .split( QRegExp("[,;\\s]"), QString::SkipEmptyParts );
        fileFormat.itemMime = t->item(row, formatSettingsTableColumns::itemMime)->text();
        if ( fileFormat.extensions.isEmpty() && fileFormat.itemMime.isEmpty() )
            continue;
        fileFormat.icon = t->cellWidget(row, formatSettingsTableColumns::icon)
                ->property("currentIcon").toString();

        QVariantMap format;
        format["formats"] = fileFormat.extensions;
        format["itemMime"] = fileFormat.itemMime;
        format["icon"] = fileFormat.icon;
        formatSettings.append(format);

        fixUserExtensions(&fileFormat.extensions);
        fixUserMimeType(&fileFormat.itemMime);
        m_formatSettings.append(fileFormat);
    }
    m_settings.insert(configFormatSettings, formatSettings);

    // Update data of items with watched files.
    // Path of watched tab changes: save in old path, unload, reload with the new path sometime later
    // Path of unwatched tab is set: save items in the new directory (sometime later)
    // Path of watched tab is unset: save, unload, empty configuration file sometime later
    foreach ( FileWatcher *watcher, m_watchers.values() ) {
        QAbstractItemModel *m = watcher->model();
        if ( watcher->path() == tabPath(*m) )
            watcher->updateItems();
    }

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
        fileFormat.extensions = format.value("formats").toStringList();
        fileFormat.itemMime = format.value("itemMime").toString();
        fileFormat.icon = format.value("icon").toString();
        fixUserExtensions(&fileFormat.extensions);
        fixUserMimeType(&fileFormat.itemMime);
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

    if ( !readConfig(file, &config) )
        return false;

    if ( shouldSyncTab(*model) ) {
        createWatcherAndLoadItems(model, config);
    } else {
        QStringList files = config.value(tabConfigSavedFiles).toStringList();
        if ( !files.isEmpty() ) {
            const QString oldTabPath = QDir::cleanPath(files[0] + "/..");
            QDir dir(oldTabPath);
            createWatcher(model, dir.path(), files);
        }
    }

    return true;
}

bool ItemSyncLoader::saveItems(const QAbstractItemModel &model, QFile *file)
{
    // If anything fails, just return false so the items are save regularly.
    FileWatcher *watcher = m_watchers.value(&model, NULL);
    if (!watcher)
        return false;

    const QString path = watcher->path();
    QStringList savedFiles;

    // Don't save items if path is empty.
    if ( !path.isEmpty() ) {
        if ( !watcher->isValid() ) {
            log( tr("Failed to synchronize tab \"%1\" with directory \"%2\"!")
                 .arg(model.property("tabName").toString())
                 .arg(path),
                 LogError );
            return false;
        }

        QDir dir(path);

        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row, 0);
            const QVariantMap itemData = index.data(contentType::data).toMap();
            const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();
            const QString baseName = getBaseName(index);
            const QString filePath = dir.absoluteFilePath(baseName);

            foreach (const QVariant &ext, mimeToExtension.values())
                savedFiles.prepend( filePath + ext.toString() );
        }
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

void ItemSyncLoader::itemsLoaded(QAbstractItemModel *model, QFile *file)
{
    QDataStream stream(file);
    bool tabSynced = readConfigHeader(&stream);
    bool syncTab = shouldSyncTab(*model);

    if (syncTab != tabSynced) {
        model->setProperty(propertyModelDirty, true);
        if (syncTab) {
            createWatcherAndLoadItems(model);
        } else {
            delete m_watchers.take(model);

            // Remove empty items.
            for (int i = 0; i < model->rowCount(); ++i) {
                const QModelIndex index = model->index(i, 0);
                QVariantMap dataMap = index.data(contentType::data).toMap();

                if ( containsUserData(dataMap) )
                    model->setData(index, dataMap, contentType::data);
                else
                    model->removeRow(i--);
            }
        }
    }
}

ItemWidget *ItemSyncLoader::transform(ItemWidget *itemWidget, const QModelIndex &index)
{
    const QString baseName = getBaseName(index);
    if ( baseName.isEmpty() )
        return NULL;

    const QVariantMap dataMap = index.data(contentType::data).toMap();
    const QVariantMap mimeToExtension = dataMap.value(mimeExtensionMap).toMap();

    QString icon;
    foreach ( const QString &format, dataMap.keys() ) {
        if ( format.startsWith(MIME_PREFIX_ITEMSYNC) )
            continue; // skip internal data

        icon = mimeToExtension.contains(format)
                ? iconFromBaseNameExtension(baseName + mimeToExtension[format].toString(), m_formatSettings)
                : iconFromMime(format);

        if ( !icon.isNull() )
            break;
    }

    if ( icon.isNull() ) {
        icon = iconFromBaseNameExtension(baseName, m_formatSettings);
        if ( icon.isNull() )
            icon = iconFromId(IconFile);
    }

    return new ItemSync(baseName, icon, itemWidget);
}

bool ItemSyncLoader::canRemoveItems(const QList<QModelIndex> &indexList)
{
    return !containsItemsWithFiles(indexList)
            || QMessageBox::question( QApplication::activeWindow(), tr("Remove Items?"),
                                      tr("Do you really want to <strong>remove items and associated files</strong>?"),
                                      QMessageBox::No | QMessageBox::Yes,
                                      QMessageBox::Yes ) == QMessageBox::Yes;
}

bool ItemSyncLoader::canMoveItems(const QList<QModelIndex> &)
{
    // Don't remove items if moved out of list.
    // Items will be automatically removed if underlying files are deleted by the move operation.
    return false;
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

        const QVariantMap itemData = index.data(contentType::data).toMap();
        const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();
        if ( mimeToExtension.isEmpty() )
            QFile::remove(path + '/' + baseName);
        else
            removeFormatFiles(path + '/' + baseName, mimeToExtension);
    }
}

QVariantMap ItemSyncLoader::copyItem(const QAbstractItemModel &model, const QVariantMap &itemData)
{
    QVariantMap copiedItemData = itemData;
    const QString syncPath = tabPath(model);
    copiedItemData.insert(mimeSyncPath, syncPath);

    // Add text/plain and text/uri-list if not present.
    bool updateUriData = !copiedItemData.contains(mimeUriList);
    bool updateTextData = !copiedItemData.contains(mimeText);
    if (updateUriData || updateTextData) {
        QByteArray uriData;
        QByteArray textData;

        const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();
        const QString basePath = syncPath + '/' + itemData.value(mimeBaseName).toString();

        foreach ( const QString &format, mimeToExtension.keys() ) {
            const QString ext = mimeToExtension[format].toString();
            const QString filePath = basePath + ext;

            if (updateUriData) {
                if ( !uriData.isEmpty() )
                    uriData.append("\n");
                uriData.append( QUrl::fromLocalFile(filePath).toEncoded() );
            }

            if (updateTextData) {
                if ( !textData.isEmpty() )
                    textData.append("\n");
                textData.append( QString(filePath)
                                 .replace('\\', "\\\\")
                                 .replace('\n', "\\n")
                                 .replace('\r', "\\r") );
            }
        }

        QVariantMap noSaveData;
        if (updateUriData) {
            noSaveData.insert(mimeUriList, calculateHash(uriData));
            copiedItemData.insert(mimeUriList, uriData);
        }
        if (updateTextData) {
            noSaveData.insert(mimeText, calculateHash(textData));
            copiedItemData.insert(mimeText, textData);
        }
        copiedItemData.insert(mimeNoSave, noSaveData);
    }

    return copiedItemData;
}

bool ItemSyncLoader::matches(const QModelIndex &index, const QRegExp &re) const
{
    const QVariantMap dataMap = index.data(contentType::data).toMap();
    const QString text = dataMap.value(mimeBaseName).toString();
    return re.indexIn(text) != -1;
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

void ItemSyncLoader::createWatcherAndLoadItems(QAbstractItemModel *model, const QVariantMap &config)
{
    model->setProperty(propertyModelDisabled, true);

    const QString path = tabPath(*model);
    if ( !path.isEmpty() ) {
        QDir dir(path);
        if ( !dir.mkpath(".") )
            return;

        QStringList files = config.value(tabConfigSavedFiles).toStringList();

        foreach ( const QString &fileName, dir.entryList(itemFileFilter(), QDir::Time | QDir::Reversed) ) {
            const QString filePath = dir.absoluteFilePath(fileName);
            if ( !files.contains(filePath) )
                files.append(filePath);
        }

        // Monitor files in directory.
        FileWatcher *watcher = createWatcher(model, dir.path(), files);
        if ( !watcher->isValid() )
            return;
    }

    model->setProperty(propertyModelDisabled, false);
}

Q_EXPORT_PLUGIN2(itemsync, ItemSyncLoader)

#include "itemsync.moc"
