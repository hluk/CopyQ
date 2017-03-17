/*
    Copyright (c) 2014, Lukas Holecek <hluk@email.cz>

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

#include "filewatcher.h"

#include "common/log.h"
#include "common/mimetypes.h"
#include "common/contenttype.h"
#include "gui/iconselectbutton.h"
#include "gui/icons.h"
#include "gui/iconfont.h"
#include "gui/iconwidget.h"

#ifdef HAS_TESTS
#   include "tests/itemsynctests.h"
#endif

#include <QBoxLayout>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QtPlugin>
#include <QUrl>
#include <QVariantMap>

#include <memory>

namespace {

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

const int currentVersion = 1;
const char dataFileHeader[] = "CopyQ_itemsync_tab";

const char configVersion[] = "copyq_itemsync_version";
const char configSyncTabs[] = "sync_tabs";
const char configFormatSettings[] = "format_settings";

const char tabConfigSavedFiles[] = "saved_files";

bool readConfigHeader(QDataStream *stream)
{
    QString header;
    *stream >> header;
    return header == dataFileHeader;
}

bool readConfig(QIODevice *file, QVariantMap *config)
{
    QDataStream stream(file);
    if ( !readConfigHeader(&stream) )
        return false;

    stream >> *config;

    return stream.status() == QDataStream::Ok
            && config->value(configVersion, 0).toInt() == currentVersion;
}

void writeConfiguration(QIODevice *file, const QStringList &savedFiles)
{
    QVariantMap config;
    config.insert(configVersion, currentVersion);
    config.insert(tabConfigSavedFiles, savedFiles);

    QDataStream stream(file);
    stream.setVersion(QDataStream::Qt_4_7);
    stream << QString(dataFileHeader);
    stream << config;
}

void setHeaderSectionResizeMode(QHeaderView *header, int logicalIndex, QHeaderView::ResizeMode mode)
{
#if QT_VERSION < 0x050000
    header->setResizeMode(logicalIndex, mode);
#else
    header->setSectionResizeMode(logicalIndex, mode);
#endif
}

QString iconFromId(int id)
{
    return id != -1 ? QString(QChar(id)) : QString();
}

QPushButton *createBrowseButton()
{
    std::unique_ptr<QPushButton> button(new QPushButton);
    button->setFont( iconFont() );
    button->setText( iconFromId(IconFolderOpen) );
    button->setToolTip( ItemSyncLoader::tr("Browse...",
                                           "Button text for opening file dialog to select synchronization directory") );
    return button.release();
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

QString iconFromUserExtension(const QStringList &fileNames, const QList<FileFormat> &formatSettings)
{
    for ( const auto &format : formatSettings ) {
        if ( format.icon.isEmpty() )
            continue;

        for (const auto &ext : format.extensions) {
            for (const auto &fileName : fileNames) {
                if ( fileName.endsWith(ext) )
                    return format.icon;
            }
        }
    }

    return QString();
}

QString iconFromMime(const QString &format)
{
    return iconFromId(iconFromMimeHelper(format));
}

QString iconForItem(const QModelIndex &index, const QList<FileFormat> &formatSettings)
{
    const QString baseName = FileWatcher::getBaseName(index);
    const QVariantMap dataMap = index.data(contentType::data).toMap();
    const QVariantMap mimeToExtension = dataMap.value(mimeExtensionMap).toMap();

    QStringList fileNames;
    for ( const auto &format : mimeToExtension.keys() ) {
        // Don't change icon for notes.
        if (format != mimeItemNotes)
            fileNames.append( baseName + mimeToExtension[format].toString() );
    }

    // Try to get user icon from file extension.
    const QString icon = iconFromUserExtension(fileNames, formatSettings);
    if ( !icon.isEmpty() )
        return icon;

    // Try to get default icon from MIME type.
    for ( const auto &format : dataMap.keys() ) {
        const auto mimeIcon = iconFromMime(format);
        if ( !mimeIcon.isEmpty() )
            return mimeIcon;
    }

    // Try to get default icon from file extension.
    for (const auto &fileName : fileNames) {
        const int id = iconFromBaseNameExtensionHelper(fileName);
        if (id != -1)
            return iconFromId(id);
    }

    // Return icon for unknown files.
    return iconFromId(IconFile);
}

bool containsItemsWithFiles(const QList<QModelIndex> &indexList)
{
    for (const auto &index : indexList) {
        if ( index.data(contentType::data).toMap().contains(mimeBaseName) )
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
    if ( mimeType->startsWith(COPYQ_MIME_PREFIX_ITEMSYNC) )
        mimeType->clear();
}

void setNormalStretchFixedColumns(QTableWidget *table, int normalColumn, int stretchColumn, int fixedColumn)
{
    QHeaderView *header = table->horizontalHeader();
    setHeaderSectionResizeMode(header, stretchColumn, QHeaderView::Stretch);
    setHeaderSectionResizeMode(header, fixedColumn, QHeaderView::Fixed);
    header->resizeSection(fixedColumn, table->rowHeight(0));
    table->resizeColumnToContents(normalColumn);
}

} // namespace

ItemSync::ItemSync(const QString &label, const QString &icon, ItemWidget *childItem)
    : QWidget( childItem->widget()->parentWidget() )
    , ItemWidget(this)
    , m_label( new QTextEdit(this) )
    , m_icon( new IconWidget(icon, this) )
    , m_childItem(childItem)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->setSizeConstraint(QLayout::SetMinimumSize);

    auto labelLayout = new QHBoxLayout;
    connect(layout, SIGNAL(destroyed()), labelLayout, SLOT(deleteLater()));
    labelLayout->setContentsMargins(0, 0, 0, 0);
    labelLayout->setSpacing(0);

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

    QTextOption option = m_label->document()->defaultTextOption();
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_label->document()->setDefaultTextOption(option);

    m_label->setReadOnly(true);
    m_label->setUndoRedoEnabled(false);

    m_label->setFocusPolicy(Qt::NoFocus);
    m_label->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_label->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_label->setFrameStyle(QFrame::NoFrame);
    m_label->setContextMenuPolicy(Qt::NoContextMenu);

    m_label->viewport()->installEventFilter(this);

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

QObject *ItemSync::createExternalEditor(const QModelIndex &index, QWidget *parent) const
{
    return m_childItem->createExternalEditor(index, parent);
}

void ItemSync::updateSize(const QSize &maximumSize, int idealWidth)
{
    setMaximumSize(maximumSize);

    const int w = idealWidth - m_icon->width() - 8;
    QTextDocument *doc = m_label->document();
    doc->setTextWidth(w);
    m_label->setFixedSize( w, static_cast<int>(doc->size().height()) );

    m_childItem->updateSize(maximumSize, idealWidth);

    adjustSize();
    setFixedSize(sizeHint());
}

bool ItemSync::eventFilter(QObject *, QEvent *event)
{
    return ItemWidget::filterMouseEvents(m_label, event);
}

ItemSyncSaver::ItemSyncSaver(const QString &tabPath)
    : m_tabPath(tabPath)
    , m_watcher(nullptr)
{
}

ItemSyncSaver::ItemSyncSaver(
        QAbstractItemModel *model,
        const QString &tabPath,
        const QString &path,
        const QStringList &files,
        int maxItems,
        const QList<FileFormat> &formatSettings)
    : m_tabPath(tabPath)
    , m_watcher(new FileWatcher(path, files, model, maxItems, formatSettings, this))
{
}

bool ItemSyncSaver::saveItems(const QString &tabName, const QAbstractItemModel &model, QIODevice *file)
{
    // Don't save items if path is empty.
    if (!m_watcher) {
        writeConfiguration(file, QStringList());
        return true;
    }

    const QString path = m_watcher->path();
    QStringList savedFiles;

    if ( !m_watcher->isValid() ) {
        log( tr("Failed to synchronize tab \"%1\" with directory \"%2\"!")
             .arg(tabName)
             .arg(path),
             LogError );
        return false;
    }

    QDir dir(path);

    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        const QVariantMap itemData = index.data(contentType::data).toMap();
        const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();
        const QString baseName = FileWatcher::getBaseName(index);
        const QString filePath = dir.absoluteFilePath(baseName);

        for (const auto &ext : mimeToExtension.values())
            savedFiles.prepend( filePath + ext.toString() );
    }

    writeConfiguration(file, savedFiles);

    return true;
}

bool ItemSyncSaver::canRemoveItems(const QList<QModelIndex> &indexList, QString *error)
{
    if ( !containsItemsWithFiles(indexList) )
        return true;

    if (error) {
        *error = "Removing synchronized items with assigned files from script is not allowed (remove the files instead)";
        return false;
    }

    return QMessageBox::question(
                QApplication::activeWindow(), tr("Remove Items?"),
                tr("Do you really want to <strong>remove items and associated files</strong>?"),
                QMessageBox::No | QMessageBox::Yes,
                QMessageBox::Yes ) == QMessageBox::Yes;
}

bool ItemSyncSaver::canMoveItems(const QList<QModelIndex> &)
{
    // Don't remove items if moved out of list.
    // Items will be automatically removed if underlying files are deleted by the move operation.
    return false;
}

void ItemSyncSaver::itemsRemovedByUser(const QList<QModelIndex> &indexList)
{
    // Remove unneeded files (remaining records in the hash map).
    for (const auto &index : indexList) {
        const QAbstractItemModel *model = index.model();
        if (!model)
            continue;

        if ( m_tabPath.isEmpty() )
            continue;

        const QString baseName = FileWatcher::getBaseName(index);
        if ( baseName.isEmpty() )
            continue;

        // Check if item is still present in list (drag'n'drop).
        bool remove = true;
        for (int i = 0; i < model->rowCount(); ++i) {
            const QModelIndex index2 = model->index(i, 0);
            if ( index2 != index && baseName == FileWatcher::getBaseName(index2) ) {
                remove = false;
                break;
            }
        }
        if (!remove)
            continue;

        const QVariantMap itemData = index.data(contentType::data).toMap();
        const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();
        if ( mimeToExtension.isEmpty() )
            QFile::remove(m_tabPath + '/' + baseName);
        else
            FileWatcher::removeFormatFiles(m_tabPath + '/' + baseName, mimeToExtension);
    }
}

QVariantMap ItemSyncSaver::copyItem(const QAbstractItemModel &, const QVariantMap &itemData)
{
    QVariantMap copiedItemData = itemData;
    copiedItemData.insert(mimeSyncPath, m_tabPath);

    // Add text/plain and text/uri-list if not present.
    bool updateUriData = !copiedItemData.contains(mimeUriList);
    bool updateTextData = !copiedItemData.contains(mimeText);
    if (updateUriData || updateTextData) {
        QByteArray uriData;
        QByteArray textData;

        const QVariantMap mimeToExtension = itemData.value(mimeExtensionMap).toMap();
        const QString basePath = m_tabPath + '/' + itemData.value(mimeBaseName).toString();

        for ( const auto &format : mimeToExtension.keys() ) {
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
                textData.append( filePath.toUtf8()
                                 .replace('\\', "\\\\")
                                 .replace('\n', "\\n")
                                 .replace('\r', "\\r") );
            }
        }

        QVariantMap noSaveData;
        if (updateUriData) {
            noSaveData.insert(mimeUriList, FileWatcher::calculateHash(uriData));
            copiedItemData.insert(mimeUriList, uriData);
        }
        if (updateTextData) {
            noSaveData.insert(mimeText, FileWatcher::calculateHash(textData));
            copiedItemData.insert(mimeText, textData);
        }
        copiedItemData.insert(mimeNoSave, noSaveData);
    }

    return copiedItemData;
}

ItemSyncLoader::ItemSyncLoader()
{
}

ItemSyncLoader::~ItemSyncLoader() = default;

QVariantMap ItemSyncLoader::applySettings()
{
    // Apply settings from tab sync path table.
    QTableWidget *t = ui->tableWidgetSyncTabs;
    QStringList tabPaths;
    m_tabPaths.clear();
    for (int row = 0; row < t->rowCount(); ++row) {
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
    for (const auto &formatSetting : formatSettings) {
        QVariantMap format = formatSetting.toMap();
        FileFormat fileFormat;
        fileFormat.extensions = format.value("formats").toStringList();
        fileFormat.itemMime = format.value("itemMime").toString();
        fileFormat.icon = format.value("icon").toString();
        fixUserExtensions(&fileFormat.extensions);
        fixUserMimeType(&fileFormat.itemMime);
        m_formatSettings.append(fileFormat);
    }
}

QWidget *ItemSyncLoader::createSettingsWidget(QWidget *parent)
{
    ui.reset(new Ui::ItemSyncSettings);
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

        auto button = new IconSelectButton();
        button->setCurrentIcon( format.value("icon").toString() );
        t->setCellWidget(row, formatSettingsTableColumns::icon, button);
    }
    setNormalStretchFixedColumns(t, formatSettingsTableColumns::formats,
                                 formatSettingsTableColumns::itemMime,
                                 formatSettingsTableColumns::icon);

    return w;
}

bool ItemSyncLoader::canLoadItems(QIODevice *file) const
{
    QDataStream stream(file);
    return readConfigHeader(&stream);
}

bool ItemSyncLoader::canSaveItems(const QString &tabName) const
{
    return m_tabPaths.contains(tabName);
}

ItemSaverPtr ItemSyncLoader::loadItems(const QString &tabName, QAbstractItemModel *model, QIODevice *file, int maxItems)
{
    QVariantMap config;
    if ( !readConfig(file, &config) )
        return nullptr;

    const QStringList files = config.value(tabConfigSavedFiles).toStringList();
    return loadItems(tabName, model, files, maxItems);
}

ItemSaverPtr ItemSyncLoader::initializeTab(const QString &tabName, QAbstractItemModel *model, int maxItems)
{
    return loadItems(tabName, model, QStringList(), maxItems);
}

ItemWidget *ItemSyncLoader::transform(ItemWidget *itemWidget, const QModelIndex &index)
{
    static const QRegExp re("copyq_\\d*");
    const QString baseName = FileWatcher::getBaseName(index);
    if ( baseName.isEmpty() || re.exactMatch(baseName) )
        return nullptr;

    itemWidget->setTagged(true);
    return new ItemSync(baseName, iconForItem(index, m_formatSettings), itemWidget);
}

bool ItemSyncLoader::matches(const QModelIndex &index, const QRegExp &re) const
{
    const QVariantMap dataMap = index.data(contentType::data).toMap();
    const QString text = dataMap.value(mimeBaseName).toString();
    return re.indexIn(text) != -1;
}

QObject *ItemSyncLoader::tests(const TestInterfacePtr &test) const
{
#ifdef HAS_TESTS
    QStringList tabPaths;
    for (int i = 0; i < 10; ++i) {
        tabPaths.append(ItemSyncTests::testTab(i));
        tabPaths.append(ItemSyncTests::testDir(i));
    }

    QVariantList formatSettings;
    QVariantMap format;

    format["formats"] = QStringList() << "xxx";
    format["itemMime"] = QString(COPYQ_MIME_PREFIX "test-xxx");
    format["icon"] = QString(iconFromId(IconTrash));
    formatSettings << format;

    format["formats"] = QStringList() << "zzz" << ".yyy";
    format["itemMime"] = QString(COPYQ_MIME_PREFIX "test-zzz");
    format["icon"] = QString();
    formatSettings << format;

    QVariantMap settings;
    settings[configSyncTabs] = tabPaths;
    settings[configFormatSettings] = formatSettings;

    QObject *tests = new ItemSyncTests(test);
    tests->setProperty("CopyQ_test_settings", settings);
    return tests;
#else
    Q_UNUSED(test);
    return nullptr;
#endif
}

void ItemSyncLoader::onBrowseButtonClicked()
{
    QTableWidget *t = ui->tableWidgetSyncTabs;

    QObject *button = sender();
    Q_ASSERT(button != nullptr);

    int row = 0;
    for ( ; row < t->rowCount() && t->cellWidget(row, syncTabsTableColumns::browse) != button; ++row ) {}
    Q_ASSERT(row != t->rowCount());

    QTableWidgetItem *item = t->item(row, syncTabsTableColumns::path);
    const QString path =
            QFileDialog::getExistingDirectory( t, tr("Open Directory for Synchronization"), item->text() );
    if ( !path.isEmpty() )
        item->setText(path);
}

ItemSaverPtr ItemSyncLoader::loadItems(const QString &tabName, QAbstractItemModel *model, const QStringList &files, int maxItems)
{
    const auto tabPath = m_tabPaths.value(tabName);
    const auto path = files.isEmpty() ? tabPath : QFileInfo(files.first()).absolutePath();
    if ( path.isEmpty() )
        return std::make_shared<ItemSyncSaver>(tabPath);

    QDir dir(path);
    if ( !dir.mkpath(".") ) {
        emit error( tr("Failed to create synchronization directory"));
        return nullptr;
    }

    return std::make_shared<ItemSyncSaver>(model, tabPath, dir.path(), files, maxItems, m_formatSettings);
}

Q_EXPORT_PLUGIN2(itemsync, ItemSyncLoader)
