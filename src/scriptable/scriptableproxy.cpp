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

#include "scriptable.h"

#include "common/commandstatus.h"
#include "common/common.h"
#include "common/contenttype.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/settings.h"
#include "gui/filedialog.h"
#include "gui/mainwindow.h"
#include "gui/tabicons.h"
#include "gui/windowgeometryguard.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QTextEdit>

#define INVOKE(call) \
    if (isValueUnset()) \
      return setValue(v, (call))

#define BROWSER(call) \
    ClipboardBrowser *c = fetchBrowser(); \
    if (c) \
        (c->call)

#define BROWSER_INVOKE(call, defaultValue) \
    if (!isValueUnset()) \
        Q_ASSERT(false); \
    ClipboardBrowser *c = fetchBrowser(); \
    return setValue(v, c ? (c->call) : (defaultValue))

Q_DECLARE_METATYPE(QFile*)

namespace {

const char propertyWidgetName[] = "CopyQ_widget_name";
const char propertyWidgetProperty[] = "CopyQ_widget_property";

template <typename T>
T setValue(QVariant &v, const T &value)
{
    v = QVariant::fromValue(value);
    return value;
}

QByteArray serializeWindow(WId winId)
{
    QByteArray data;
    return createPlatformNativeInterface()->serialize(winId, &data) ? data : QByteArray();
}

QWidget *label(Qt::Orientation orientation, const QString &name, QWidget *w)
{
    QWidget *parent = w->parentWidget();

    if ( !name.isEmpty() ) {
        QBoxLayout *layout;
        if (orientation == Qt::Horizontal)
            layout = new QHBoxLayout;
        else
            layout = new QVBoxLayout;

        parent->layout()->addItem(layout);

        QLabel *label = new QLabel(name + ":", parent);
        label->setBuddy(w);
        layout->addWidget(label);
        layout->addWidget(w, 1);
    }

    w->setProperty(propertyWidgetName, name);

    return w;
}

QWidget *label(const QString &name, QWidget *w)
{
    w->setProperty("text", name);
    w->setProperty(propertyWidgetName, name);
    return w;
}

template <typename Widget>
Widget *createAndSetWidget(const char *propertyName, const QVariant &value, QWidget *parent)
{
    Widget *w = new Widget(parent);
    w->setProperty(propertyName, value);
    w->setProperty(propertyWidgetProperty, propertyName);
    parent->layout()->addWidget(w);
    return w;
}

QWidget *createDateTimeEdit(
        const QString &name, const char *propertyName, const QVariant &value, QWidget *parent)
{
    QDateTimeEdit *w = createAndSetWidget<QDateTimeEdit>(propertyName, value, parent);
    w->setCalendarPopup(true);
    return label(Qt::Horizontal, name, w);
}

void installShortcutToCloseDialog(QWidget *dialog, QWidget *shortcutParent, int shortcut)
{
    QShortcut *s = new QShortcut(QKeySequence(shortcut), shortcutParent);
    QObject::connect(s, SIGNAL(activated()), dialog, SLOT(accept()));
}

QWidget *createListWidget(const QString &name, const QStringList &itemsWithCurrent, QWidget *parent)
{
    const QString currentText = itemsWithCurrent.value(0);
    const QStringList items = itemsWithCurrent.mid(1);
    QComboBox *w = createAndSetWidget<QComboBox>("currentText", QVariant(), parent);
    w->setEditable(true);
    w->addItems(items);
    w->setCurrentIndex(items.indexOf(currentText));
    w->lineEdit()->setText(currentText);
    w->lineEdit()->selectAll();
    installShortcutToCloseDialog(parent, w, Qt::Key_Enter);
    installShortcutToCloseDialog(parent, w, Qt::Key_Return);
    return label(Qt::Horizontal, name, w);
}

QWidget *createSpinBox(const QString &name, const QVariant &value, QWidget *parent)
{
    QSpinBox *w = createAndSetWidget<QSpinBox>("value", value, parent);
    w->setRange(-1e9, 1e9);
    return label(Qt::Horizontal, name, w);
}

QWidget *createLineEdit(const QVariant &value, QWidget *parent)
{
    QLineEdit *lineEdit = createAndSetWidget<QLineEdit>("text", value, parent);
    lineEdit->selectAll();
    return lineEdit;
}

QWidget *createFileNameEdit(const QString &name, const QFile &file, QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    parent->layout()->addWidget(w);

    QHBoxLayout *layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    QWidget *lineEdit = createLineEdit(file.fileName(), w);
    lineEdit->setProperty(propertyWidgetName, name);

    QPushButton *browseButton = new QPushButton("...");

    FileDialog *dialog = new FileDialog(w, name, file.fileName());
    QObject::connect( browseButton, SIGNAL(clicked()),
                      dialog, SLOT(exec()) );
    QObject::connect( dialog, SIGNAL(fileSelected(QString)),
                      lineEdit, SLOT(setText(QString)) );

    layout->addWidget(lineEdit);
    layout->addWidget(browseButton);

    label(Qt::Vertical, name, w);

    return lineEdit;
}

QWidget *createTextEdit(const QString &name, const QVariant &value, QWidget *parent)
{
    QTextEdit *w = createAndSetWidget<QTextEdit>("plainText", value, parent);
    w->setTabChangesFocus(true);
    return label(Qt::Vertical, name, w);
}

QWidget *createWidget(const QString &name, const QVariant &value, QWidget *parent)
{
    switch ( value.type() ) {
    case QVariant::Bool:
        return label(name, createAndSetWidget<QCheckBox>("checked", value, parent));
    case QVariant::Int:
        return createSpinBox("value", value, parent);
    case QVariant::Date:
        return createDateTimeEdit(name, "date", value, parent);
    case QVariant::Time:
        return createDateTimeEdit(name, "time", value, parent);
    case QVariant::DateTime:
        return createDateTimeEdit(name, "dateTime", value, parent);
    case QVariant::List:
        return createListWidget(name, value.toStringList(), parent);
    default:
        QFile *file = value.value<QFile*>();
        if (file) {
            return createFileNameEdit(name, *file, parent);
        } else {
            const QString text = value.toString();
            if (text.contains('\n'))
                return createTextEdit(name, value.toStringList(), parent);
        }

        return label(Qt::Horizontal, name, createLineEdit(value, parent));
    }
}

void setGeometryWithoutSave(QWidget *window, const QRect &geometry)
{
    WindowGeometryGuard::blockUntilHidden(window);

    int x = pointsToPixels(geometry.x());
    int y = pointsToPixels(geometry.y());
    if (x < 0 || y < 0) {
        const QPoint mousePos = QCursor::pos();
        if (geometry.x() < 0)
            x = mousePos.x();
        if (geometry.y() < 0)
            y = mousePos.y();
    }

    const int w = pointsToPixels(geometry.width());
    const int h = pointsToPixels(geometry.height());
    if (w > 0 && h > 0)
        window->resize(w, h);

    moveWindowOnScreen(window, QPoint(x, y));
}

} // namespace

namespace detail {

ScriptableProxyHelper::ScriptableProxyHelper(MainWindow *mainWindow, const QVariantMap &actionData)
    : QObject(NULL)
    , m_wnd(mainWindow)
    , m_tabName()
    , m_lock()
    , m_actionData(actionData)
{
    qRegisterMetaType< QPointer<QWidget> >("QPointer<QWidget>");
    moveToThread(m_wnd->thread());
}

const QVariant &ScriptableProxyHelper::value() const
{
    return v;
}

void ScriptableProxyHelper::unsetValue()
{
    v.clear();
    m_valueUnset = true;
}

bool ScriptableProxyHelper::isValueUnset()
{
    if (!m_valueUnset)
        return false;
    m_valueUnset = false;
    return true;
}

QString ScriptableProxyHelper::tabNotFoundError()
{
    return ScriptableProxyHelper::tr("Tab with given name doesn't exist!");
}

QString ScriptableProxyHelper::tabNameEmptyError()
{
    return ScriptableProxyHelper::tr("Tab name cannot be empty!");
}

void ScriptableProxyHelper::close()
{
    m_wnd->close();
}

void ScriptableProxyHelper::showWindow()
{
    m_wnd->showWindow();
}

void ScriptableProxyHelper::showWindowAt(const QRect &rect)
{
    setGeometryWithoutSave(m_wnd, rect);
    showWindow();
}

bool ScriptableProxyHelper::pasteToCurrentWindow()
{
    INVOKE(pasteToCurrentWindow());

    PlatformWindowPtr window = createPlatformNativeInterface()->getCurrentWindow();
    if (!window)
        return false;
    window->pasteClipboard();
    return true;
}

bool ScriptableProxyHelper::copyFromCurrentWindow()
{
    INVOKE(copyFromCurrentWindow());

    PlatformWindowPtr window = createPlatformNativeInterface()->getCurrentWindow();
    if (!window)
        return false;
    window->copy();
    return true;
}

void ScriptableProxyHelper::abortAutomaticCommands()
{
    m_wnd->abortAutomaticCommands();
}

bool ScriptableProxyHelper::isMonitoringEnabled()
{
    INVOKE(isMonitoringEnabled());
    return m_wnd->isMonitoringEnabled();
}

bool ScriptableProxyHelper::isMainWindowVisible()
{
    INVOKE(isMainWindowVisible());
    return !m_wnd->isMinimized() && m_wnd->isVisible();
}

bool ScriptableProxyHelper::isMainWindowFocused()
{
    INVOKE(isMainWindowFocused());
    return m_wnd->isActiveWindow();
}

void ScriptableProxyHelper::disableMonitoring(bool arg1)
{
    m_wnd->disableClipboardStoring(arg1);
}

void ScriptableProxyHelper::setClipboard(const QVariantMap &data, QClipboard::Mode mode)
{
    m_wnd->setClipboard(data, mode);
}

QString ScriptableProxyHelper::renameTab(const QString &arg1, const QString &arg2)
{
    INVOKE(renameTab(arg1, arg2));

    if ( arg1.isEmpty() || arg2.isEmpty() )
        return tabNameEmptyError();

    const int i = m_wnd->findTabIndex(arg2);
    if (i == -1)
        return tabNotFoundError();

    if ( m_wnd->findTabIndex(arg1) != -1 )
        return tr("Tab with given name already exists!");

    m_wnd->renameTab(arg1, i);

    return QString();
}

QString ScriptableProxyHelper::removeTab(const QString &arg1)
{
    INVOKE(removeTab(arg1));

    if ( arg1.isEmpty() )
        return tabNameEmptyError();

    const int i = m_wnd->findTabIndex(arg1);
    if (i == -1)
        return tabNotFoundError();

    m_wnd->removeTab(false, i);
    return QString();
}

QString ScriptableProxyHelper::tabIcon(const QString &tabName)
{
    INVOKE(tabIcon(tabName));
    return getIconNameForTabName(tabName);
}

void ScriptableProxyHelper::setTabIcon(const QString &tabName, const QString &icon)
{
    m_wnd->setTabIcon(tabName, icon);
}

void ScriptableProxyHelper::showBrowser(const QString &tabName)
{
    ClipboardBrowser *c = fetchBrowser(tabName);
    if (c)
        m_wnd->showBrowser(c);
}

void ScriptableProxyHelper::showBrowserAt(const QString &tabName, const QRect &rect)
{
    setGeometryWithoutSave(m_wnd, rect);
    showBrowser(tabName);
    QCoreApplication::processEvents();
}

void ScriptableProxyHelper::showBrowser()
{
    showBrowser(m_tabName);
}

void ScriptableProxyHelper::action(const QVariantMap &arg1, const Command &arg2)
{
    m_wnd->action(arg1, arg2);
}

void ScriptableProxyHelper::showMessage(const QString &arg1, const QString &arg2, QSystemTrayIcon::MessageIcon arg3, int arg4)
{
    m_wnd->showMessage(arg1, arg2, arg3, arg4);
}

void ScriptableProxyHelper::browserLock()
{
    Q_ASSERT(m_lock.isNull());
    ClipboardBrowser *c = fetchBrowser();
    if (c)
        m_lock.reset( new ClipboardBrowser::Lock(c) );
}

void ScriptableProxyHelper::browserUnlock()
{
    Q_ASSERT(!m_lock.isNull());
    m_lock.reset();
}

QVariantMap ScriptableProxyHelper::nextItem(int where)
{
    INVOKE(nextItem(where));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return QVariantMap();

    const int row = qMax(0, c->currentIndex().row()) + where;
    const QModelIndex index = c->index(row);

    if (!index.isValid())
        return QVariantMap();

    c->setCurrentIndex(index);
    return ::itemData(index);
}

void ScriptableProxyHelper::browserMoveToClipboard(int arg1)
{
    ClipboardBrowser *c = fetchBrowser();
    if (c)
        c->moveToClipboard(c->index(arg1));
}

void ScriptableProxyHelper::browserSetCurrent(int arg1)
{
    BROWSER(setCurrent(arg1));
}

void ScriptableProxyHelper::browserRemoveRows(QList<int> rows)
{
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return;

    qSort( rows.begin(), rows.end(), qGreater<int>() );

    ClipboardBrowser::Lock lock(c);
    foreach (int row, rows)
        c->removeRow(row);
}

void ScriptableProxyHelper::browserEditRow(int arg1)
{
    BROWSER(editRow(arg1));
}

void ScriptableProxyHelper::browserEditNew(const QString &arg1, bool changeClipboard)
{
    BROWSER(editNew(arg1, changeClipboard));
}

QStringList ScriptableProxyHelper::tabs()
{
    INVOKE(tabs());
    return m_wnd->tabs();
}

bool ScriptableProxyHelper::toggleVisible()
{
    INVOKE(toggleVisible());
    return m_wnd->toggleVisible();
}

bool ScriptableProxyHelper::toggleMenu(const QString &tabName)
{
    INVOKE(toggleMenu(tabName));
    ClipboardBrowser *c = fetchBrowser(tabName);
    return c && m_wnd->toggleMenu(c);
}

bool ScriptableProxyHelper::toggleMenu()
{
    INVOKE(toggleMenu());
    return m_wnd->toggleMenu();
}

QByteArray ScriptableProxyHelper::mainWinId()
{
    INVOKE(mainWinId());
    return serializeWindow(m_wnd->winId());
}

QByteArray ScriptableProxyHelper::trayMenuWinId()
{
    INVOKE(trayMenuWinId());
    return serializeWindow(m_wnd->trayMenu()->winId());
}

int ScriptableProxyHelper::findTabIndex(const QString &arg1)
{
    INVOKE(findTabIndex(arg1));
    return m_wnd->findTabIndex(arg1);
}

QByteArray ScriptableProxyHelper::openActionDialog(const QVariantMap &arg1)
{
    INVOKE(openActionDialog(arg1));
    return serializeWindow(m_wnd->openActionDialog(arg1));
}

bool ScriptableProxyHelper::loadTab(const QString &arg1)
{
    INVOKE(loadTab(arg1));
    return m_wnd->loadTab(arg1);
}

bool ScriptableProxyHelper::saveTab(const QString &arg1)
{
    INVOKE(saveTab(arg1));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return false;

    const int i = m_wnd->findTabIndex( c->tabName() );
    return m_wnd->saveTab(arg1, i);
}

QVariant ScriptableProxyHelper::config(const QString &name, const QString &value)
{
    INVOKE(config(name, value));

    if ( name.isNull() )
        return m_wnd->getUserOptionsDescription();

    if ( m_wnd->hasUserOption(name) ) {
        if ( value.isNull() )
            return m_wnd->getUserOptionValue(name);

        m_wnd->setUserOptionValue(name, value);
        return QString();
    }

    return QVariant();
}

QByteArray ScriptableProxyHelper::getClipboardData(const QString &mime, QClipboard::Mode mode)
{
    INVOKE(getClipboardData(mime, mode));
    const QMimeData *data = clipboardData(mode);
    if (!data)
        return QByteArray();

    if (mime == "?")
        return data->formats().join("\n").toUtf8() + '\n';

    return cloneData(*data, QStringList(mime)).value(mime).toByteArray();
}

int ScriptableProxyHelper::browserLength()
{
    BROWSER_INVOKE(length(), 0);
}

bool ScriptableProxyHelper::browserOpenEditor(const QByteArray &arg1, bool changeClipboard)
{
    BROWSER_INVOKE(openEditor(arg1, changeClipboard), false);
}

bool ScriptableProxyHelper::browserAdd(const QString &arg1)
{
    BROWSER_INVOKE(add(arg1), false);
}

bool ScriptableProxyHelper::browserAdd(const QStringList &texts)
{
    INVOKE(browserAdd(texts));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return false;

    ClipboardBrowser::Lock lock(c);
    foreach (const QString &text, texts) {
        if ( !c->add(text) )
            return false;
    }

    return true;
}

bool ScriptableProxyHelper::browserAdd(const QVariantMap &arg1, int arg2)
{
    BROWSER_INVOKE(add(arg1, arg2), false);
}

bool ScriptableProxyHelper::browserChange(const QVariantMap &data, int row)
{
    INVOKE(browserChange(data, row));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return false;

    const QModelIndex index = c->index(row);
    QVariantMap itemData = index.data(contentType::data).toMap();
    foreach (const QString &mime, data.keys())
        itemData[mime] = data[mime];

    return c->model()->setData(index, itemData, contentType::data);
}

QByteArray ScriptableProxyHelper::browserItemData(int arg1, const QString &arg2)
{
    INVOKE(browserItemData(arg1, arg2));
    return itemData(arg1, arg2);
}

QVariantMap ScriptableProxyHelper::browserItemData(int arg1)
{
    INVOKE(browserItemData(arg1));
    return itemData(arg1);
}

void ScriptableProxyHelper::setCurrentTab(const QString &tabName)
{
    ClipboardBrowser *c = fetchBrowser(tabName);
    if (c)
        m_wnd->setCurrentTab(c);
}

void ScriptableProxyHelper::setTab(const QString &tabName)
{
    m_tabName = tabName;
}

QString ScriptableProxyHelper::tab()
{
    BROWSER_INVOKE(tabName(), QString());
}

int ScriptableProxyHelper::currentItem()
{
    INVOKE(currentItem());
    if (!canUseSelectedItems())
        return -1;

    const QPersistentModelIndex current =
            m_actionData.value(mimeCurrentItem).value<QPersistentModelIndex>();
    return current.isValid() ? current.row() : -1;
}

bool ScriptableProxyHelper::selectItems(const QList<int> &items)
{
    INVOKE(selectItems(items));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return false;

    c->clearSelection();

    if ( !items.isEmpty() ) {
        c->setCurrent(items.last());

        foreach (int i, items) {
            const QModelIndex index = c->index(i);
            if (index.isValid())
                c->selectionModel()->select(index, QItemSelectionModel::Select);
        }
    }

    return true;
}

QList<int> ScriptableProxyHelper::selectedItems()
{
    INVOKE(selectedItems());

    if (!canUseSelectedItems())
        return QList<int>();

    QList<int> selectedRows;
    const QList<QPersistentModelIndex> selected = selectedIndexes();
    foreach (const QPersistentModelIndex &index, selected) {
        if (index.isValid())
            selectedRows.append(index.row());
    }

    return selectedRows;
}

#ifdef HAS_TESTS
QString ScriptableProxyHelper::sendKeys(const QString &keys)
{
    INVOKE(sendKeys(keys));

    if (keys == "FLUSH_KEYS")
        return QString();

    QWidget *w = m_wnd->trayMenu();

    if ( !w->isVisible() ) {
        w = QApplication::focusWidget();
        if (!w) {
            COPYQ_LOG("No focused widget -> using main window");
            w = m_wnd;
        }
    }

    if (keys.startsWith(":")) {
        const QString widgetName = QString("%1 in %2")
                .arg(w->metaObject()->className())
                .arg(w->window()->metaObject()->className());

        COPYQ_LOG( QString("Sending keys \"%1\" to \"%2\".")
                   .arg(keys.mid(1))
                   .arg(widgetName) );

        QTest::keyClicks(w, keys.mid(1), Qt::NoModifier, 50);
    } else {
        const QKeySequence shortcut(keys, QKeySequence::PortableText);

        if ( shortcut.isEmpty() )
            return QString("Cannot parse key \"%1\"!").arg(keys);

        // Don't stop when modal window is open.
        QMetaObject::invokeMethod( this, "keyClick", Qt::QueuedConnection,
                                   Q_ARG(const QKeySequence &, shortcut),
                                   Q_ARG(const QPointer<QWidget> &, w)
                                   );
    }

    return QString();
}

QString ScriptableProxyHelper::testSelected()
{
    INVOKE(testSelected());

    ClipboardBrowser *browser = m_wnd->browser();
    if (!browser)
        return QString();

    if (browser->length() == 0)
        return browser->tabName();

    QModelIndexList selectedIndexes = browser->selectionModel()->selectedIndexes();

    QStringList result;
    result.reserve( selectedIndexes.size() + 1 );

    const QModelIndex currentIndex = browser->currentIndex();
    result.append(currentIndex.isValid() ? QString::number(currentIndex.row()) : "_");

    QList<int> selectedRows;
    selectedRows.reserve( selectedIndexes.size() );
    foreach (const QModelIndex &index, selectedIndexes)
        selectedRows.append(index.row());
    qSort(selectedRows);

    foreach (int row, selectedRows)
        result.append(QString::number(row));

    return browser->tabName() + " " + result.join(" ");
}

void ScriptableProxyHelper::keyClick(const QKeySequence &shortcut, const QPointer<QWidget> &widget)
{
    const QString keys = shortcut.toString();

    if (widget.isNull()) {
        COPYQ_LOG( QString("Failed to send key \"%1\".").arg(keys) );
        return;
    }

    const QString widgetName = QString("%1 in %2")
            .arg(widget->metaObject()->className())
            .arg(widget->window()->metaObject()->className());

    m_wnd->showMessage( widgetName, shortcut.toString(),
                        QSystemTrayIcon::Information, 4000 );

    COPYQ_LOG( QString("Sending key \"%1\" to \"%2\".")
               .arg(keys)
               .arg(widgetName) );

    QTest::keyClick( widget.data(),
                     Qt::Key(shortcut[0] & ~Qt::KeyboardModifierMask),
            Qt::KeyboardModifiers(shortcut[0] & Qt::KeyboardModifierMask), 0 );

    COPYQ_LOG( QString("Key \"%1\" sent to \"%2\".")
               .arg(keys)
               .arg(widgetName) );
}
#else // HAS_TESTS
#define INVOKE_NO_TESTS(value) \
    Q_ASSERT(isValueUnset()); \
    return setValue(v, (value))

QString ScriptableProxyHelper::sendKeys(const QString &)
{
    INVOKE_NO_TESTS(QString());
}

QString ScriptableProxyHelper::testSelected()
{
    INVOKE_NO_TESTS(QString());
}

void ScriptableProxyHelper::keyClick(const QKeySequence &, const QPointer<QWidget> &)
{
}
#endif // HAS_TESTS

QString ScriptableProxyHelper::currentWindowTitle()
{
    INVOKE(currentWindowTitle());
    PlatformWindowPtr window = createPlatformNativeInterface()->getCurrentWindow();
    return window ? window->getTitle() : QString();
}

NamedValueList ScriptableProxyHelper::inputDialog(const NamedValueList &values)
{
    INVOKE(inputDialog(values));
    QDialog dialog;
    QVBoxLayout layout(&dialog);
    QWidgetList widgets;
    widgets.reserve(values.size());

    QString styleSheet;
    QRect geometry(-1, -1, 0, 0);

    foreach (const NamedValue &value, values) {
        if (value.name == ".title")
            dialog.setWindowTitle( value.value.toString() );
        else if (value.name == ".icon")
            dialog.setWindowIcon( QIcon(value.value.toString()) );
        else if (value.name == ".style")
            styleSheet = value.value.toString();
        else if (value.name == ".height")
            geometry.setHeight( pointsToPixels(value.value.toInt()) );
        else if (value.name == ".width")
            geometry.setWidth( pointsToPixels(value.value.toInt()) );
        else if (value.name == ".x")
            geometry.setX(value.value.toInt());
        else if (value.name == ".y")
            geometry.setY(value.value.toInt());
        else if (value.name == ".label")
            createAndSetWidget<QLabel>("text", value.value, &dialog);
        else
            widgets.append( createWidget(value.name, value.value, &dialog) );
    }

    dialog.adjustSize();

    if (geometry.height() <= 0)
        geometry.setHeight(dialog.height());
    if (geometry.width() <= 0)
        geometry.setWidth(dialog.width());
    if (geometry.isValid())
        dialog.resize(geometry.size());
    if (geometry.x() >= 0 && geometry.y() >= 0)
        dialog.move(geometry.topLeft());

    if ( !styleSheet.isEmpty() )
        dialog.setStyleSheet(styleSheet);

    QDialogButtonBox buttons(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    connect( &buttons, SIGNAL(accepted()), &dialog, SLOT(accept()) );
    connect( &buttons, SIGNAL(rejected()), &dialog, SLOT(reject()) );
    layout.addWidget(&buttons);

    dialog.show();
    dialog.raise();
    dialog.activateWindow();

    const QByteArray wid = serializeWindow( dialog.winId() );
    if ( !wid.isEmpty() )
        emit sendMessage(wid, CommandActivateWindow);

    if ( !dialog.exec() )
        return NamedValueList();

    NamedValueList result;

    foreach ( QWidget *w, widgets ) {
        const QString propertyName = w->property(propertyWidgetProperty).toString();
        const QString name = w->property(propertyWidgetName).toString();
        const QVariant value = w->property(propertyName.toUtf8().constData());
        result.append( NamedValue(name, value) );
    }

    return result;
}

void ScriptableProxyHelper::setUserValue(const QString &key, const QVariant &value)
{
    Settings settings;
    settings.beginGroup("script");
    settings.setValue(key, value);
}

void ScriptableProxyHelper::updateFirstItem(const QVariantMap &data)
{
    m_wnd->updateFirstItem(data);
}

void ScriptableProxyHelper::updateTitle(const QVariantMap &data)
{
    if (m_wnd->canUpdateTitleFromScript())
        m_wnd->updateTitle(data);
}

void ScriptableProxyHelper::setSelectedItemsData(const QString &mime, const QVariant &value)
{
    const QList<QPersistentModelIndex> selected = selectedIndexes();
    foreach (const QPersistentModelIndex &index, selected) {
        ClipboardBrowser *c = m_wnd->browserForItem(index);
        if (c) {
            QVariantMap data = c->model()->data(index, contentType::data).toMap();
            data[mime] = value;
            c->model()->setData(index, data, contentType::data);
        }
    }
}

void ScriptableProxyHelper::filter(const QString &text)
{
    m_wnd->setFilter(text);
}

ClipboardBrowser *detail::ScriptableProxyHelper::fetchBrowser(const QString &tabName)
{
    if (tabName.isEmpty()) {
        const QString defaultTabName = m_actionData.value(mimeCurrentTab).toString();
        if (!defaultTabName.isEmpty())
            return fetchBrowser(defaultTabName);
    }

    ClipboardBrowser *c = tabName.isEmpty() ? m_wnd->browser(0) : m_wnd->tab(tabName);
    if (!c)
        return NULL;

    c->loadItems();
    return c->isLoaded() ? c : NULL;
}

ClipboardBrowser *ScriptableProxyHelper::fetchBrowser() { return fetchBrowser(m_tabName); }

QVariantMap ScriptableProxyHelper::itemData(int i)
{
    ClipboardBrowser *c = fetchBrowser();
    return c ? ::itemData(c->index(i)) : QVariantMap();
}

QByteArray ScriptableProxyHelper::itemData(int i, const QString &mime)
{
    const QVariantMap data = itemData(i);
    if ( data.isEmpty() )
        return QByteArray();

    if (mime == "?")
        return QStringList(data.keys()).join("\n").toUtf8() + '\n';

    if (mime == mimeItems)
        return serializeData(data);

    return data.value(mime).toByteArray();
}

bool ScriptableProxyHelper::canUseSelectedItems() const
{
    return m_tabName.isEmpty()
            || m_tabName == m_actionData.value(mimeCurrentTab).toString();
}

QList<QPersistentModelIndex> ScriptableProxyHelper::selectedIndexes() const
{
    return m_actionData.value(mimeSelectedItems)
            .value< QList<QPersistentModelIndex> >();
}

} // namespace detail

ScriptableProxy::ScriptableProxy(MainWindow *mainWindow, const QVariantMap &actionData)
    : m_helper(new detail::ScriptableProxyHelper(mainWindow, actionData))
{
    qRegisterMetaType<QSystemTrayIcon::MessageIcon>("SystemTrayIcon::MessageIcon");
}

ScriptableProxy::~ScriptableProxy() { delete m_helper; }
