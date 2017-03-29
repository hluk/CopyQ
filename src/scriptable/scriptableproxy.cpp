/*
    Copyright (c) 2017, Lukas Holecek <hluk@email.cz>

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

#include "scriptableproxy.h"

#include "common/commandstatus.h"
#include "common/command.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/config.h"
#include "common/contenttype.h"
#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/settings.h"
#include "gui/filedialog.h"
#include "gui/iconfactory.h"
#include "gui/mainwindow.h"
#include "gui/tabicons.h"
#include "gui/windowgeometryguard.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"
#include "scriptable/scriptable.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDateTimeEdit>
#include <QDesktopWidget>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSpinBox>
#include <QTextEdit>
#include <QThread>

#include <type_traits>

#define ASSERT_MAIN_THREAD() \
    Q_ASSERT(m_invoked || QThread::currentThread() != qApp->thread()); \

#define BROWSER(call) \
    ClipboardBrowser *c = fetchBrowser(); \
    if (c) \
        (c->call)

#define INVOKE(call) \
    ASSERT_MAIN_THREAD(); \
    if (!m_invoked) { \
        auto callable = createCallable([&]{ return call; }); \
        m_invoked = true; \
        QMetaObject::invokeMethod(m_wnd, "invoke", Qt::BlockingQueuedConnection, Q_ARG(Callable*, &callable)); \
        m_invoked = false; \
        return callable.result(); \
    }

#define INVOKE2(call) \
    ASSERT_MAIN_THREAD(); \
    if (!m_invoked) { \
        auto callable = createCallable([&]{ call; }); \
        m_invoked = true; \
        QMetaObject::invokeMethod(m_wnd, "invoke", Qt::BlockingQueuedConnection, Q_ARG(Callable*, &callable)); \
        m_invoked = false; \
        return; \
    }

Q_DECLARE_METATYPE(QFile*)

namespace {

const char propertyWidgetName[] = "CopyQ_widget_name";
const char propertyWidgetProperty[] = "CopyQ_widget_property";

struct InputDialog {
    QDialog dialog;
    QString defaultChoice; /// Default text for list widgets.
};

template <typename T, typename Return>
class CallableFn : public Callable {
public:
    explicit CallableFn(T &&fn)
        : m_fn(std::forward<T>(fn))
        , m_result()
    {
    }

    void operator()() override { m_result = m_fn(); }
    Return &result() { return m_result; }

private:
    T m_fn;
    Return m_result;
};

template <typename T>
class CallableFn<T, void> : public Callable {
public:
    explicit CallableFn(T &&fn) : m_fn(std::forward<T>(fn)) {}
    void operator()() override { m_fn(); }

private:
    T m_fn;
};

template <typename T, typename Return = typename std::result_of<T()>::type>
CallableFn<T, Return> createCallable(T &&lambda)
{
    return CallableFn<T, Return>(std::forward<T>(lambda));
}

class ScreenshotRectWidget : public QLabel {
public:
    explicit ScreenshotRectWidget(const QPixmap &pixmap)
    {
        setWindowFlags(Qt::Widget | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setCursor(Qt::CrossCursor);

        move(0, 0);
        resize(pixmap.size());
        setPixmap(pixmap);

        show();
    }

    void paintEvent(QPaintEvent *ev) override
    {
        QLabel::paintEvent(ev);
        if (selectionRect.isValid()) {
            QPainter p(this);
            const auto w = pointsToPixels(1);

            p.setPen(QPen(Qt::white, w));
            p.drawRect(selectionRect);

            p.setPen(QPen(Qt::black, w));
            p.drawRect(selectionRect.adjusted(-w, -w, w, w));
        }
    }

    void keyPressEvent(QKeyEvent *ev) override
    {
        QWidget::keyPressEvent(ev);
        hide();
    }

    void mousePressEvent(QMouseEvent *ev) override
    {
        if ( ev->button() == Qt::LeftButton ) {
            m_pos = ev->pos();
            selectionRect.setTopLeft(m_pos);
            update();
        } else {
            hide();
        }
    }

    void mouseReleaseEvent(QMouseEvent *) override
    {
        hide();
    }

    void mouseMoveEvent(QMouseEvent *ev) override
    {
        if ( !ev->buttons().testFlag(Qt::LeftButton) )
            return;

        const auto pos = ev->pos();
        const auto x = std::minmax(pos.x(), m_pos.x());
        const auto y = std::minmax(pos.y(), m_pos.y());
        selectionRect = QRect( QPoint(x.first, y.first), QPoint(x.second, y.second) );
        update();
    }

    QRect selectionRect;

private:
    QPoint m_pos;
};

/// Load icon from icon font, path or theme.
QIcon loadIcon(const QString &idPathOrName)
{
    if (idPathOrName.size() == 1)
        return createPixmap(idPathOrName[0].unicode(), Qt::white, 64);

    if ( QFile::exists(idPathOrName) )
        return QIcon(idPathOrName);

    return QIcon::fromTheme(idPathOrName);
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
    auto w = new Widget(parent);
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

QWidget *createListWidget(const QString &name, const QStringList &items, InputDialog *inputDialog)
{
    QDialog *parent = &inputDialog->dialog;

    const QString currentText = inputDialog->defaultChoice.isNull()
            ? items.value(0)
            : inputDialog->defaultChoice;

    const QString listPrefix = ".list:";
    if ( name.startsWith(listPrefix) ) {
        QListWidget *w = createAndSetWidget<QListWidget>("currentRow", QVariant(), parent);
        w->addItems(items);
        const int i = items.indexOf(currentText);
        if (i != -1)
            w->setCurrentRow(i);
        w->setAlternatingRowColors(true);
        installShortcutToCloseDialog(parent, w, Qt::Key_Enter);
        installShortcutToCloseDialog(parent, w, Qt::Key_Return);
        return label(Qt::Vertical, name.mid(listPrefix.length()), w);
    }

    QComboBox *w = createAndSetWidget<QComboBox>("currentText", QVariant(), parent);
    w->setEditable(true);
    w->addItems(items);
    w->setCurrentIndex(items.indexOf(currentText));
    w->lineEdit()->setText(currentText);
    w->lineEdit()->selectAll();
    w->setMaximumWidth( pointsToPixels(400) );
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

    auto layout = new QHBoxLayout(w);
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

QWidget *createWidget(const QString &name, const QVariant &value, InputDialog *inputDialog)
{
    QDialog *parent = &inputDialog->dialog;

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
    case QVariant::StringList:
        return createListWidget(name, value.toStringList(), inputDialog);
    default:
        QFile *file = value.value<QFile*>();
        if (file)
            return createFileNameEdit(name, *file, parent);

        const QString text = value.toString();
        if (text.contains('\n'))
            return createTextEdit(name, value.toStringList(), parent);

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

QString tabNotFoundError()
{
    return ScriptableProxy::tr("Tab with given name doesn't exist!");
}

QString tabNameEmptyError()
{
    return ScriptableProxy::tr("Tab name cannot be empty!");
}

} // namespace

ScriptableProxy::ScriptableProxy(MainWindow *mainWindow)
    : QObject(nullptr)
    , m_wnd(mainWindow)
    , m_tabName()
    , m_invoked(false)
{
    qRegisterMetaType< QPointer<QWidget> >("QPointer<QWidget>");
    moveToThread(m_wnd->thread());
}

QVariantMap ScriptableProxy::getActionData(int id)
{
    INVOKE(getActionData(id));
    m_actionData = m_wnd->actionData(id);
    return m_actionData;
}

void ScriptableProxy::setActionData(int id, const QVariantMap &data)
{
    INVOKE2(setActionData(id, data));
    m_wnd->setActionData(id, data);
}

void ScriptableProxy::exit()
{
    INVOKE2(exit());
    qApp->quit();
}

void ScriptableProxy::close()
{
    INVOKE2(close());
    m_wnd->close();
}

bool ScriptableProxy::showWindow()
{
    INVOKE(showWindow());
    m_wnd->showWindow();
    return m_wnd->isVisible();
}

bool ScriptableProxy::showWindowAt(const QRect &rect)
{
    INVOKE(showWindowAt(rect));
    setGeometryWithoutSave(m_wnd, rect);
    return showWindow();
}

bool ScriptableProxy::pasteToCurrentWindow()
{
    INVOKE(pasteToCurrentWindow());

    PlatformWindowPtr window = createPlatformNativeInterface()->getCurrentWindow();
    if (!window)
        return false;
    window->pasteClipboard();
    return true;
}

bool ScriptableProxy::copyFromCurrentWindow()
{
    INVOKE(copyFromCurrentWindow());

    PlatformWindowPtr window = createPlatformNativeInterface()->getCurrentWindow();
    if (!window)
        return false;
    window->copy();
    return true;
}

void ScriptableProxy::abortAutomaticCommands()
{
    INVOKE2(abortAutomaticCommands());

    m_wnd->abortAutomaticCommands();
}

bool ScriptableProxy::isMonitoringEnabled()
{
    INVOKE(isMonitoringEnabled());
    return m_wnd->isMonitoringEnabled();
}

bool ScriptableProxy::isMainWindowVisible()
{
    INVOKE(isMainWindowVisible());
    return !m_wnd->isMinimized() && m_wnd->isVisible();
}

bool ScriptableProxy::isMainWindowFocused()
{
    INVOKE(isMainWindowFocused());
    return m_wnd->isActiveWindow();
}

void ScriptableProxy::disableMonitoring(bool arg1)
{
    INVOKE2(disableMonitoring(arg1));
    m_wnd->disableClipboardStoring(arg1);
}

void ScriptableProxy::setClipboard(const QVariantMap &data, QClipboard::Mode mode)
{
    INVOKE2(setClipboard(data, mode));
    m_wnd->setClipboard(data, mode);
}

QString ScriptableProxy::renameTab(const QString &arg1, const QString &arg2)
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

QString ScriptableProxy::removeTab(const QString &arg1)
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

QString ScriptableProxy::tabIcon(const QString &tabName)
{
    INVOKE(tabIcon(tabName));
    return getIconNameForTabName(tabName);
}

void ScriptableProxy::setTabIcon(const QString &tabName, const QString &icon)
{
    INVOKE2(setTabIcon(tabName, icon));
    m_wnd->setTabIcon(tabName, icon);
}

bool ScriptableProxy::showBrowser(const QString &tabName)
{
    INVOKE(showBrowser(tabName));
    ClipboardBrowser *c = fetchBrowser(tabName);
    if (c)
        m_wnd->showBrowser(c);
    return m_wnd->isVisible();
}

bool ScriptableProxy::showBrowserAt(const QString &tabName, const QRect &rect)
{
    INVOKE(showBrowserAt(tabName, rect));
    setGeometryWithoutSave(m_wnd, rect);
    return showBrowser(tabName);
}

bool ScriptableProxy::showBrowser()
{
    INVOKE(showBrowser());
    return showBrowser(m_tabName);
}

void ScriptableProxy::action(const QVariantMap &arg1, const Command &arg2)
{
    INVOKE2(action(arg1, arg2));
    m_wnd->action(arg1, arg2);
}

void ScriptableProxy::showMessage(const QString &title,
        const QString &msg,
        const QString &icon,
        int msec,
        const QString &notificationId,
        const NotificationButtons &buttons)
{
    INVOKE2(showMessage(title, msg, icon, msec, notificationId, buttons));
    m_wnd->showMessage(title, msg, icon, msec, notificationId, buttons);
}

QVariantMap ScriptableProxy::nextItem(int where)
{
    INVOKE(nextItem(where));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return QVariantMap();

    const int row = qMax(0, c->currentIndex().row()) + where;
    const QModelIndex index = c->index(row);

    if (!index.isValid())
        return QVariantMap();

    c->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    return c->copyIndex(index);
}

void ScriptableProxy::browserMoveToClipboard(int arg1)
{
    INVOKE2(browserMoveToClipboard(arg1));
    ClipboardBrowser *c = fetchBrowser();
    if (c)
        c->moveToClipboard(c->index(arg1));
}

void ScriptableProxy::browserSetCurrent(int arg1)
{
    INVOKE2(browserSetCurrent(arg1));
    BROWSER(setCurrent(arg1));
}

QString ScriptableProxy::browserRemoveRows(QList<int> rows)
{
    INVOKE(browserRemoveRows(rows));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return QString("Invalid tab");

    qSort( rows.begin(), rows.end(), qGreater<int>() );

    QModelIndexList indexes;
    indexes.reserve(rows.size());

    for (int row : rows) {
        const QModelIndex indexToRemove = c->index(row);
        if ( indexToRemove.isValid() )
            indexes.append(indexToRemove);
    }

    const QPersistentModelIndex currentIndex = c->currentIndex();

    QString error;
    const int lastRow = c->removeIndexes(indexes, &error);

    if ( !error.isEmpty() )
        return error;

    if ( !currentIndex.isValid() ) {
        const int currentRow = qMin(lastRow, c->length() - 1);
        c->setCurrent(currentRow);
    }

    return QString();
}

void ScriptableProxy::browserEditRow(int arg1)
{
    INVOKE2(browserEditRow(arg1));
    BROWSER(editRow(arg1));
}

void ScriptableProxy::browserEditNew(const QString &arg1, bool changeClipboard)
{
    INVOKE2(browserEditNew(arg1, changeClipboard));
    BROWSER(editNew(arg1, changeClipboard));
}

QStringList ScriptableProxy::tabs()
{
    INVOKE(tabs());
    return m_wnd->tabs();
}

bool ScriptableProxy::toggleVisible()
{
    INVOKE(toggleVisible());
    return m_wnd->toggleVisible();
}

bool ScriptableProxy::toggleMenu(const QString &tabName, int maxItemCount, const QPoint &position)
{
    INVOKE(toggleMenu(tabName, maxItemCount, position));
    return m_wnd->toggleMenu(tabName, maxItemCount, position);
}

bool ScriptableProxy::toggleMenu()
{
    INVOKE(toggleMenu());
    return m_wnd->toggleMenu();
}

int ScriptableProxy::findTabIndex(const QString &arg1)
{
    INVOKE(findTabIndex(arg1));
    return m_wnd->findTabIndex(arg1);
}

void ScriptableProxy::openActionDialog(const QVariantMap &arg1)
{
    INVOKE2(openActionDialog(arg1));
    m_wnd->openActionDialog(arg1);
}

bool ScriptableProxy::loadTab(const QString &arg1)
{
    INVOKE(loadTab(arg1));
    return m_wnd->loadTab(arg1);
}

bool ScriptableProxy::saveTab(const QString &arg1)
{
    INVOKE(saveTab(arg1));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return false;

    const int i = m_wnd->findTabIndex( c->tabName() );
    return m_wnd->saveTab(arg1, i);
}

bool ScriptableProxy::importData(const QString &fileName)
{
    INVOKE(importData(fileName));
    return m_wnd->importData(fileName, ImportOptions::All);
}

bool ScriptableProxy::exportData(const QString &fileName)
{
    INVOKE(exportData(fileName));
    return m_wnd->exportAllData(fileName);
}

QStringList ScriptableProxy::config(const QStringList &nameValue)
{
    INVOKE(config(nameValue));
    return m_wnd->config(nameValue);
}

QByteArray ScriptableProxy::getClipboardData(const QString &mime, QClipboard::Mode mode)
{
    INVOKE(getClipboardData(mime, mode));
    const QMimeData *data = clipboardData(mode);
    if (!data)
        return QByteArray();

    if (mime == "?")
        return data->formats().join("\n").toUtf8() + '\n';

    return cloneData(*data, QStringList(mime)).value(mime).toByteArray();
}

bool ScriptableProxy::hasClipboardFormat(const QString &mime, QClipboard::Mode mode)
{
    INVOKE(hasClipboardFormat(mime, mode));
    const QMimeData *data = clipboardData(mode);
    return data && data->hasFormat(mime);
}

int ScriptableProxy::browserLength()
{
    INVOKE(browserLength());
    ClipboardBrowser *c = fetchBrowser();
    return c ? c->length() : 0;
}

bool ScriptableProxy::browserOpenEditor(const QByteArray &arg1, bool changeClipboard)
{
    INVOKE(browserOpenEditor(arg1, changeClipboard));
    ClipboardBrowser *c = fetchBrowser();
    return c && c->openEditor(arg1, changeClipboard);
}

QString ScriptableProxy::browserAdd(const QStringList &texts)
{
    INVOKE(browserAdd(texts));

    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return "Invalid tab";

    if ( !c->allocateSpaceForNewItems(texts.size()) )
        return "Tab is full (cannot remove any items)";

    for (const auto &text : texts) {
        if ( !c->add(text) )
            return "Failed to new add items";
    }

    return QString();
}

QString ScriptableProxy::browserAdd(const QVariantMap &arg1, int arg2)
{
    INVOKE(browserAdd(arg1, arg2));

    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return "Invalid tab";

    if ( !c->allocateSpaceForNewItems(1) )
        return "Tab is full (cannot remove any items)";

    if ( !c->add(arg1, arg2) )
        return "Failed to new add item";

    return QString();
}

bool ScriptableProxy::browserChange(const QVariantMap &data, int row)
{
    INVOKE(browserChange(data, row));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return false;

    const auto index = c->index(row);
    QVariantMap itemData = c->model()->data(index, contentType::data).toMap();
    for ( const auto &format : data.keys() ) {
        const auto value = data[format];
        if (value.isValid())
            itemData[format] = value;
        else
            itemData.remove(format);
    }

    return c->model()->setData(index, itemData, contentType::data);
}

QByteArray ScriptableProxy::browserItemData(int arg1, const QString &arg2)
{
    INVOKE(browserItemData(arg1, arg2));
    return itemData(arg1, arg2);
}

QVariantMap ScriptableProxy::browserItemData(int arg1)
{
    INVOKE(browserItemData(arg1));
    return itemData(arg1);
}

void ScriptableProxy::setCurrentTab(const QString &tabName)
{
    INVOKE2(setCurrentTab(tabName));
    ClipboardBrowser *c = fetchBrowser(tabName);
    if (c)
        m_wnd->setCurrentTab(c);
}

void ScriptableProxy::setTab(const QString &tabName)
{
    m_tabName = tabName;
}

QString ScriptableProxy::tab()
{
    INVOKE(tab());
    ClipboardBrowser *c = fetchBrowser();
    return c ? c->tabName() : QString();
}

int ScriptableProxy::currentItem()
{
    INVOKE(currentItem());

    const QPersistentModelIndex current =
            m_actionData.value(mimeCurrentItem).value<QPersistentModelIndex>();
    return current.isValid() ? current.row() : -1;
}

bool ScriptableProxy::selectItems(const QList<int> &items)
{
    INVOKE(selectItems(items));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return false;

    c->clearSelection();

    if ( !items.isEmpty() ) {
        c->setCurrent(items.last());

        for (int i : items) {
            const QModelIndex index = c->index(i);
            if (index.isValid())
                c->selectionModel()->select(index, QItemSelectionModel::Select);
        }
    }

    return true;
}

QList<int> ScriptableProxy::selectedItems()
{
    INVOKE(selectedItems());

    QList<int> selectedRows;
    const QList<QPersistentModelIndex> selected = selectedIndexes();
    for (const auto &index : selected) {
        if (index.isValid())
            selectedRows.append(index.row());
    }

    return selectedRows;
}

int ScriptableProxy::selectedItemsDataCount()
{
    INVOKE(selectedItemsDataCount());
    return selectedIndexes().size();
}

QVariantMap ScriptableProxy::selectedItemData(int selectedIndex)
{
    INVOKE(selectedItemData(selectedIndex));

    auto c = currentBrowser();
    if (!c)
        return QVariantMap();

    const auto index = selectedIndexes().value(selectedIndex);
    Q_ASSERT( !index.isValid() || index.model() == c->model() );
    return c->copyIndex(index);
}

bool ScriptableProxy::setSelectedItemData(int selectedIndex, const QVariantMap &data)
{
    INVOKE(setSelectedItemData(selectedIndex, data));

    auto c = currentBrowser();
    if (!c)
        return false;

    const auto index = selectedIndexes().value(selectedIndex);
    if ( !index.isValid() )
        return false;

    Q_ASSERT( index.model() == c->model() );
    return c->model()->setData(index, data, contentType::data);
}

QList<QVariantMap> ScriptableProxy::selectedItemsData()
{
    INVOKE(selectedItemsData());

    auto c = currentBrowser();
    if (!c)
        return QList<QVariantMap>();

    const auto model = c->model();

    QList<QVariantMap> dataList;

    for ( const auto &index : selectedIndexes() ) {
        if ( index.isValid() ) {
            Q_ASSERT( index.model() == model );
            dataList.append( c->copyIndex(index) );
        }
    }

    return dataList;
}

void ScriptableProxy::setSelectedItemsData(const QList<QVariantMap> &dataList)
{
    INVOKE2(setSelectedItemsData(dataList));

    auto c = currentBrowser();
    if (!c)
        return;

    const auto model = c->model();

    const auto indexes = selectedIndexes();
    const auto count = std::min( indexes.size(), dataList.size() );
    for ( int i = 0; i < count; ++i ) {
        const auto &index = indexes[i];
        if ( index.isValid() ) {
            Q_ASSERT( index.model() == model );
            model->setData(index, dataList[i], contentType::data);
        }
    }
}

#ifdef HAS_TESTS
void ScriptableProxy::sendKeys(const QString &keys, int delay)
{
    INVOKE2(sendKeys(keys, delay));
    m_sentKeyClicks = m_wnd->sendKeyClicks(keys, delay);
}

bool ScriptableProxy::keysSent()
{
    INVOKE(keysSent());
    QCoreApplication::processEvents();
    return m_wnd->lastReceivedKeyClicks() >= m_sentKeyClicks;
}

QString ScriptableProxy::testSelected()
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
    for (const auto &index : selectedIndexes)
        selectedRows.append(index.row());
    qSort(selectedRows);

    for (int row : selectedRows)
        result.append(QString::number(row));

    return browser->tabName() + " " + result.join(" ");
}

void ScriptableProxy::resetTestSession(const QString &clipboardTabName)
{
    INVOKE2(resetTestSession(clipboardTabName));
    m_wnd->resetTestSession(clipboardTabName);
}
#endif // HAS_TESTS

QString ScriptableProxy::currentWindowTitle()
{
    INVOKE(currentWindowTitle());
    PlatformWindowPtr window = createPlatformNativeInterface()->getCurrentWindow();
    return window ? window->getTitle() : QString();
}

NamedValueList ScriptableProxy::inputDialog(const NamedValueList &values)
{
    INVOKE(inputDialog(values));

    InputDialog inputDialog;
    QDialog &dialog = inputDialog.dialog;

    QIcon icon;
    QVBoxLayout layout(&dialog);
    QWidgetList widgets;
    widgets.reserve(values.size());

    QString styleSheet;
    QRect geometry(-1, -1, 0, 0);

    for (const auto &value : values) {
        if (value.name == ".title")
            dialog.setWindowTitle( value.value.toString() );
        else if (value.name == ".icon")
            icon = loadIcon(value.value.toString());
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
        else if (value.name == ".defaultChoice")
            inputDialog.defaultChoice = value.value.toString();
        else
            widgets.append( createWidget(value.name, value.value, &inputDialog) );
    }

    dialog.adjustSize();

    if (geometry.height() == 0)
        geometry.setHeight(dialog.height());
    if (geometry.width() == 0)
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

    if (icon.isNull())
        icon = appIcon();
    dialog.setWindowIcon(icon);

    dialog.show();
    dialog.raise();
    dialog.activateWindow();

    if ( !dialog.exec() )
        return NamedValueList();

    NamedValueList result;

    for ( auto w : widgets ) {
        const QString propertyName = w->property(propertyWidgetProperty).toString();
        const QString name = w->property(propertyWidgetName).toString();
        const QVariant value = w->property(propertyName.toUtf8().constData());
        result.append( NamedValue(name, value) );
    }

    return result;
}

void ScriptableProxy::setUserValue(const QString &key, const QVariant &value)
{
    INVOKE2(setUserValue(key, value));
    Settings settings;
    settings.beginGroup("script");
    settings.setValue(key, value);
}

void ScriptableProxy::updateFirstItem(const QVariantMap &data)
{
    INVOKE2(updateFirstItem(data));
    m_wnd->updateFirstItem(data);
}

void ScriptableProxy::updateTitle(const QVariantMap &data)
{
    INVOKE2(updateTitle(data));
    if (m_wnd->canUpdateTitleFromScript())
        m_wnd->updateTitle(data);
}

void ScriptableProxy::setSelectedItemsData(const QString &mime, const QVariant &value)
{
    INVOKE2(setSelectedItemsData(mime, value));
    const QList<QPersistentModelIndex> selected = selectedIndexes();
    for (const auto &index : selected) {
        ClipboardBrowser *c = m_wnd->browserForItem(index);
        if (c) {
            QVariantMap data = c->model()->data(index, contentType::data).toMap();
            if (value.isValid())
                data[mime] = value;
            else
                data.remove(mime);
            c->model()->setData(index, data, contentType::data);
        }
    }
}

void ScriptableProxy::filter(const QString &text)
{
    INVOKE2(filter(text));
    m_wnd->setFilter(text);
}

QList<Command> ScriptableProxy::commands()
{
    INVOKE(commands());
    return loadAllCommands();
}

void ScriptableProxy::setCommands(const QList<Command> &commands)
{
    INVOKE2(setCommands(commands));
    m_wnd->setCommands(commands);
}

void ScriptableProxy::addCommands(const QList<Command> &commands)
{
    INVOKE2(addCommands(commands));
    m_wnd->addCommands(commands);
}

QByteArray ScriptableProxy::screenshot(const QString &format, const QString &screenName, bool select)
{
    INVOKE(screenshot(format, screenName, select));

#if QT_VERSION < 0x050000
    auto pixmap = QPixmap::grabWindow(QApplication::desktop()->winId());
#else
    QScreen *selectedScreen = nullptr;
    if ( screenName.isEmpty() ) {
        selectedScreen = QApplication::primaryScreen();
    } else {
        for ( const auto screen : QApplication::screens() ) {
            if (screen->name() == screenName) {
                selectedScreen = screen;
                break;
            }
        }
    }

    if (!selectedScreen)
        return QByteArray();

    auto pixmap = selectedScreen->grabWindow(0);
#endif

    if (select) {
        ScreenshotRectWidget rectWidget(pixmap);
        while ( !rectWidget.isHidden() )
            QCoreApplication::processEvents();
        const auto rect = rectWidget.selectionRect;
        if ( rect.isValid() )
            pixmap = pixmap.copy(rect);
    }

    QByteArray bytes;
    {
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if ( !pixmap.save(&buffer, format.toUtf8().constData()) )
            return QByteArray();
    }

    return bytes;
}

QString ScriptableProxy::pluginsPath()
{
    INVOKE(pluginsPath());
    return ::pluginsPath();
}

QString ScriptableProxy::themesPath()
{
    INVOKE(themesPath());
    return ::themesPath();
}

QString ScriptableProxy::translationsPath()
{
    INVOKE(translationsPath());
    return ::translationsPath();
}

ClipboardBrowser *ScriptableProxy::fetchBrowser(const QString &tabName)
{
    ASSERT_MAIN_THREAD();

    if (tabName.isEmpty()) {
        const QString defaultTabName = m_actionData.value(mimeCurrentTab).toString();
        if (!defaultTabName.isEmpty())
            return fetchBrowser(defaultTabName);
    }

    return tabName.isEmpty() ? m_wnd->browser(0) : m_wnd->tab(tabName);
}

ClipboardBrowser *ScriptableProxy::fetchBrowser() { return fetchBrowser(m_tabName); }

QVariantMap ScriptableProxy::itemData(int i)
{
    ASSERT_MAIN_THREAD();

    auto c = fetchBrowser();
    return c ? c->copyIndex( c->index(i) ) : QVariantMap();
}

QByteArray ScriptableProxy::itemData(int i, const QString &mime)
{
    ASSERT_MAIN_THREAD();

    const QVariantMap data = itemData(i);
    if ( data.isEmpty() )
        return QByteArray();

    if (mime == "?")
        return QStringList(data.keys()).join("\n").toUtf8() + '\n';

    if (mime == mimeItems)
        return serializeData(data);

    return data.value(mime).toByteArray();
}

ClipboardBrowser *ScriptableProxy::currentBrowser() const
{
    ASSERT_MAIN_THREAD();

    const QString currentTabName = m_actionData.value(mimeCurrentTab).toString();
    if (currentTabName.isEmpty())
        return nullptr;

    const int i = m_wnd->findTabIndex(currentTabName);
    if (i == -1)
        return nullptr;

    auto c = m_wnd->browser(i);
    Q_ASSERT(c->tabName() == currentTabName);
    return c;
}

QList<QPersistentModelIndex> ScriptableProxy::selectedIndexes() const
{
    ASSERT_MAIN_THREAD();

    return m_actionData.value(mimeSelectedItems)
            .value< QList<QPersistentModelIndex> >();
}

QString pluginsPath()
{
    QDir dir;
    if (createPlatformNativeInterface()->findPluginDir(&dir))
        return dir.absolutePath();

    return QString();
}

QString themesPath()
{
    return createPlatformNativeInterface()->themePrefix();
}

QString translationsPath()
{
    return createPlatformNativeInterface()->translationPrefix();
}
