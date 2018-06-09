/*
    Copyright (c) 2018, Lukas Holecek <hluk@email.cz>

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

#include "common/action.h"
#include "common/appconfig.h"
#include "common/command.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/config.h"
#include "common/contenttype.h"
#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/settings.h"
#include "common/textdata.h"
#include "gui/clipboardbrowser.h"
#include "gui/filedialog.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/mainwindow.h"
#include "gui/notification.h"
#include "gui/tabicons.h"
#include "gui/windowgeometryguard.h"
#include "item/serialize.h"
#include "platform/platformnativeinterface.h"
#include "platform/platformwindow.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QApplication>
#include <QBuffer>
#include <QDataStream>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaMethod>
#include <QMetaType>
#include <QMimeData>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSpinBox>
#include <QTextEdit>
#include <QUrl>

#include <type_traits>

const quint32 serializedFunctionCallMagicNumber = 0x58746908;
const quint32 serializedFunctionCallVersion = 1;

#define BROWSER(call) \
    ClipboardBrowser *c = fetchBrowser(); \
    if (c) \
        (c->call)

#define STR(str) str

#define INVOKE_(function, arguments) \
    static auto f = FunctionCallSerializer(STR(#function)).withSlotArguments arguments; \
    f.setTabName(m_tabName); \
    f.setArguments arguments; \
    emit sendFunctionCall(f.serializeAndClear()) \

#define INVOKE(function, arguments) \
    if (!m_wnd) { \
        using Result = decltype(function arguments); \
        INVOKE_(function, arguments); \
        return m_returnValue.value<Result>(); \
    }

#define INVOKE2(function, arguments) \
    if (!m_wnd) { \
        INVOKE_(function, arguments); \
        return; \
    }

Q_DECLARE_METATYPE(QFile*)

QDataStream &operator<<(QDataStream &out, const NotificationButton &button)
{
    out << button.name
        << button.script
        << button.data;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, NotificationButton &button)
{
    in >> button.name
       >> button.script
       >> button.data;
    Q_ASSERT(in.status() == QDataStream::Ok);
    return in;
}

QDataStream &operator<<(QDataStream &out, const NamedValueList &list)
{
    out << list.size();
    for (const auto &item : list)
        out << item.name << item.value;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, NamedValueList &list)
{
    int size;
    in >> size;
    for (int i = 0; i < size; ++i) {
        NamedValue item;
        in >> item.name >> item.value;
        list.append(item);
    }
    Q_ASSERT(in.status() == QDataStream::Ok);
    return in;
}

QDataStream &operator<<(QDataStream &out, const Command &command)
{
    out << command.name
        << command.re
        << command.wndre
        << command.matchCmd
        << command.cmd
        << command.sep
        << command.input
        << command.output
        << command.wait
        << command.automatic
        << command.display
        << command.inMenu
        << command.isGlobalShortcut
        << command.isScript
        << command.transform
        << command.remove
        << command.hideWindow
        << command.enable
        << command.icon
        << command.shortcuts
        << command.globalShortcuts
        << command.tab
        << command.outputTab;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, Command &command)
{
    in >> command.name
       >> command.re
       >> command.wndre
       >> command.matchCmd
       >> command.cmd
       >> command.sep
       >> command.input
       >> command.output
       >> command.wait
       >> command.automatic
       >> command.display
       >> command.inMenu
       >> command.isGlobalShortcut
       >> command.isScript
       >> command.transform
       >> command.remove
       >> command.hideWindow
       >> command.enable
       >> command.icon
       >> command.shortcuts
       >> command.globalShortcuts
       >> command.tab
       >> command.outputTab;
    Q_ASSERT(in.status() == QDataStream::Ok);
    return in;
}

QDataStream &operator<<(QDataStream &out, ClipboardMode mode)
{
    const int modeId = static_cast<int>(mode);
    out << modeId;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, ClipboardMode &mode)
{
    int modeId;
    in >> modeId;
    Q_ASSERT(in.status() == QDataStream::Ok);
    mode = static_cast<ClipboardMode>(modeId);
    return in;
}

QDataStream &operator<<(QDataStream &out, const ScriptablePath &path)
{
    return out << path.path;
}

QDataStream &operator>>(QDataStream &in, ScriptablePath &path)
{
    return in >> path.path;
}

QDataStream &operator<<(QDataStream &out, Qt::KeyboardModifiers value)
{
    return out << static_cast<int>(value);
}

QDataStream &operator>>(QDataStream &in, Qt::KeyboardModifiers &value)
{
    int valueInt;
    in >> valueInt;
    Q_ASSERT(in.status() == QDataStream::Ok);
    value = static_cast<Qt::KeyboardModifiers>(valueInt);
    return in;
}

namespace {

const char propertyWidgetName[] = "CopyQ_widget_name";
const char propertyWidgetProperty[] = "CopyQ_widget_property";

struct InputDialog {
    QDialog dialog;
    QString defaultChoice; /// Default text for list widgets.

    explicit InputDialog(QWidget *parentWindow)
        : dialog(parentWindow)
    {
    }
};

template<typename ...Ts>
class SlotArguments;

template<typename T, typename ...Ts>
class SlotArguments<T, Ts...> {
public:
    static QByteArray arguments()
    {
        if ( std::is_same<QVariant, T>::value )
            return "QVariant," + SlotArguments<Ts...>::arguments();

        return QByteArray(QMetaType::typeName(qMetaTypeId<T>()))
                + "," + SlotArguments<Ts...>::arguments();
    }
};

template<>
class SlotArguments<> {
public:
    static QByteArray arguments() { return QByteArray(); }
};

class FunctionCallSerializer {
public:
    explicit FunctionCallSerializer(const char *functionName)
    {
        m_args << QVariant() << QByteArray(functionName);
    }

    template<typename ...Ts>
    FunctionCallSerializer &withSlotArguments(Ts...)
    {
        QByteArray args = SlotArguments<Ts...>::arguments();
        args.chop(1);
        setSlotArgumentTypes(args);
        return *this;
    }

    QByteArray serializeAndClear()
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_0);
        stream << serializedFunctionCallMagicNumber << serializedFunctionCallVersion << m_args;
        m_args.resize(2);
        return bytes;
    }

    void setArguments() {}

    template<typename T, typename ...Ts>
    void setArguments(const T &head, Ts... args)
    {
        m_args << QVariant::fromValue(head);
        setArguments(args...);
    }

    void setTabName(const QString &tabName)
    {
        m_args[0] = tabName;
    }

private:
    void setSlotArgumentTypes(const QByteArray &args)
    {
        const auto slotName = m_args[1].toByteArray() + "(" + args + ")";
        const int slotIndex = ScriptableProxy::staticMetaObject.indexOfSlot(slotName);
        if (slotIndex == -1) {
            log("Failed to find scriptable proxy slot: " + slotName, LogError);
            Q_ASSERT(false);
        }
        m_args[1] = slotName;
    }

    QVector<QVariant> m_args;
};

class ScreenshotRectWidget : public QLabel {
public:
    explicit ScreenshotRectWidget(const QPixmap &pixmap)
    {
        setWindowFlags(Qt::Widget | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setCursor(Qt::CrossCursor);
        setPixmap(pixmap);
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
        // Types need to be explicitly specified because minmax() returns pair of references.
        const std::pair<int,int> x = std::minmax(pos.x(), m_pos.x());
        const std::pair<int,int> y = std::minmax(pos.y(), m_pos.y());
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
    QObject::connect(s, SIGNAL(activatedAmbiguously()), dialog, SLOT(accept()));
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

QWidget *createFileNameEdit(const QString &name, const QString &path, QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    parent->layout()->addWidget(w);

    auto layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    QWidget *lineEdit = createLineEdit(path, w);
    lineEdit->setProperty(propertyWidgetName, name);

    QPushButton *browseButton = new QPushButton("...");

    FileDialog *dialog = new FileDialog(w, name, path);
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
        if ( value.userType() == qMetaTypeId<ScriptablePath>() ) {
            const auto path = value.value<ScriptablePath>();
            return createFileNameEdit(name, path.path, parent);
        }

        const QString text = value.toString();
        if (text.contains('\n'))
            return createTextEdit(name, value.toStringList(), parent);

        return label(Qt::Horizontal, name, createLineEdit(value, parent));
    }
}

void setGeometryWithoutSave(QWidget *window, QRect geometry)
{
    setGeometryGuardBlockedUntilHidden(window);

    const auto pos = (geometry.x() == -1 && geometry.y() == -1)
            ? QCursor::pos()
            : geometry.topLeft();

    const int w = pointsToPixels(geometry.width());
    const int h = pointsToPixels(geometry.height());
    if (w > 0 && h > 0)
        window->resize(w, h);

    moveWindowOnScreen(window, pos);
}

QString tabNotFoundError()
{
    return ScriptableProxy::tr("Tab with given name doesn't exist!");
}

QString tabNameEmptyError()
{
    return ScriptableProxy::tr("Tab name cannot be empty!");
}

void raiseWindow(QWidget *window)
{
    window->raise();
    window->activateWindow();
    QApplication::setActiveWindow(window);
    QApplication::processEvents();
    const auto wid = window->winId();
    const auto platformWindow = createPlatformNativeInterface()->getWindow(wid);
    if (platformWindow)
        platformWindow->raise();
}

} // namespace

ScriptableProxy::ScriptableProxy(MainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_wnd(mainWindow)
    , m_tabName()
{
    qRegisterMetaType< QPointer<QWidget> >("QPointer<QWidget>");
    qRegisterMetaTypeStreamOperators<ClipboardMode>("ClipboardMode");
    qRegisterMetaTypeStreamOperators<Command>("Command");
    qRegisterMetaTypeStreamOperators<NamedValueList>("NamedValueList");
    qRegisterMetaTypeStreamOperators<NotificationButtons>("NotificationButtons");
    qRegisterMetaTypeStreamOperators<ScriptablePath>("ScriptablePath");
    qRegisterMetaTypeStreamOperators<QVector<int>>("QVector<int>");
    qRegisterMetaTypeStreamOperators<QVector<Command>>("QVector<Command>");
    qRegisterMetaTypeStreamOperators<QVector<QVariantMap>>("QVector<QVariantMap>");
    qRegisterMetaTypeStreamOperators<Qt::KeyboardModifiers>("Qt::KeyboardModifiers");
}

QByteArray ScriptableProxy::callFunction(const QByteArray &serializedFunctionCall)
{
    QVector<QVariant> functionCall;
    {
        QDataStream stream(serializedFunctionCall);
        stream.setVersion(QDataStream::Qt_5_0);

        quint32 magicNumber;
        quint32 version;
        stream >> magicNumber >> version;
        if (stream.status() != QDataStream::Ok) {
            log("Failed to read scriptable proxy slot call preamble", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }

        if (magicNumber != serializedFunctionCallMagicNumber) {
            log("Unexpected scriptable proxy slot call preamble magic number", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }

        if (version != serializedFunctionCallVersion) {
            log("Unexpected scriptable proxy slot call preamble version", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }

        stream >> functionCall;
        if (stream.status() != QDataStream::Ok) {
            log("Failed to read scriptable proxy slot call", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }
    }

    m_tabName = functionCall.value(0).toString();
    const auto slotName = functionCall.value(1).toByteArray();
    const auto slotIndex = metaObject()->indexOfSlot(slotName);
    if (slotIndex == -1) {
        log("Failed to find scriptable proxy slot: " + slotName, LogError);
        Q_ASSERT(false);
        return QByteArray();
    }

    const int argumentsStartIndex = 2;

    const auto metaMethod = metaObject()->method(slotIndex);
    const auto typeId = metaMethod.returnType();

    QGenericArgument args[9];
    for (int i = argumentsStartIndex; i < functionCall.size(); ++i) {
        auto &value = functionCall[i];
        const int j = i - argumentsStartIndex;
        const int argumentTypeId = metaMethod.parameterType(j);
        if (argumentTypeId == QMetaType::QVariant) {
            args[j] = Q_ARG(QVariant, value);
        } else if ( value.userType() == argumentTypeId ) {
            args[j] = QGenericArgument( value.typeName(), static_cast<void*>(value.data()) );
        } else {
            log( QString("Bad argument type (at index %1) for scriptable proxy slot: %2")
                 .arg(j)
                 .arg(metaMethod.methodSignature().constData()), LogError);
            Q_ASSERT(false);
            return QByteArray();
        }
    }

    if (typeId == QMetaType::Void) {
        const bool called = metaMethod.invoke(
                this, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);

        if (!called) {
            log( QString("Bad scriptable proxy slot call: %1")
                 .arg(metaMethod.methodSignature().constData()), LogError);
            Q_ASSERT(false);
        }

        return QByteArray();
    }

    QVariant returnValue(typeId, nullptr);
    const auto genericReturnValue = returnValue.isValid()
            ? QGenericReturnArgument(returnValue.typeName(), static_cast<void*>(returnValue.data()) )
            : Q_RETURN_ARG(QVariant, returnValue);

    const bool called = metaMethod.invoke(
                this, genericReturnValue,
                args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);

    if (!called) {
        log("Bad scriptable proxy slot call (with return value)", LogError);
        Q_ASSERT(false);
        return QByteArray();
    }

    QByteArray bytes;
    {
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream << returnValue;
    }

    return bytes;
}

void ScriptableProxy::setReturnValue(const QByteArray &returnValue)
{
    QDataStream stream(returnValue);
    stream >> m_returnValue;
}

QVariantMap ScriptableProxy::getActionData(int id)
{
    INVOKE(getActionData, (id));
    m_actionData = m_wnd->actionData(id);
    m_actionId = id;

    auto data = m_actionData;
    data.remove(mimeSelectedItems);
    data.remove(mimeCurrentItem);
    return data;
}

void ScriptableProxy::setActionData(int id, const QVariantMap &data)
{
    INVOKE2(setActionData, (id, data));
    m_wnd->setActionData(id, data);
}

void ScriptableProxy::exit()
{
    INVOKE2(exit, ());
    qApp->quit();
}

void ScriptableProxy::close()
{
    INVOKE2(close, ());
    m_wnd->close();
}

bool ScriptableProxy::showWindow()
{
    INVOKE(showWindow, ());
    m_wnd->showWindow();
    return m_wnd->isVisible();
}

bool ScriptableProxy::showWindowAt(QRect rect)
{
    INVOKE(showWindowAt, (rect));
    setGeometryWithoutSave(m_wnd, rect);
    return showWindow();
}

bool ScriptableProxy::pasteToCurrentWindow()
{
    INVOKE(pasteToCurrentWindow, ());

    PlatformWindowPtr window = createPlatformNativeInterface()->getCurrentWindow();
    if (!window)
        return false;
    window->pasteClipboard();
    return true;
}

bool ScriptableProxy::copyFromCurrentWindow()
{
    INVOKE(copyFromCurrentWindow, ());

    PlatformWindowPtr window = createPlatformNativeInterface()->getCurrentWindow();
    if (!window)
        return false;
    window->copy();
    return true;
}

bool ScriptableProxy::isMonitoringEnabled()
{
    INVOKE(isMonitoringEnabled, ());
    return m_wnd->isMonitoringEnabled();
}

bool ScriptableProxy::isMainWindowVisible()
{
    INVOKE(isMainWindowVisible, ());
    return !m_wnd->isMinimized() && m_wnd->isVisible();
}

bool ScriptableProxy::isMainWindowFocused()
{
    INVOKE(isMainWindowFocused, ());
    return m_wnd->isActiveWindow();
}

void ScriptableProxy::disableMonitoring(bool arg1)
{
    INVOKE2(disableMonitoring, (arg1));
    m_wnd->disableClipboardStoring(arg1);
}

void ScriptableProxy::setClipboard(const QVariantMap &data, ClipboardMode mode)
{
    INVOKE2(setClipboard, (data, mode));
    m_wnd->setClipboard(data, mode);
}

QString ScriptableProxy::renameTab(const QString &arg1, const QString &arg2)
{
    INVOKE(renameTab, (arg1, arg2));

    if ( arg1.isEmpty() || arg2.isEmpty() )
        return tabNameEmptyError();

    const int i = m_wnd->findTabIndex(arg2);
    if (i == -1)
        return tabNotFoundError();

    if ( m_wnd->findTabIndex(arg1) != -1 )
        return ScriptableProxy::tr("Tab with given name already exists!");

    m_wnd->renameTab(arg1, i);

    return QString();
}

QString ScriptableProxy::removeTab(const QString &arg1)
{
    INVOKE(removeTab, (arg1));

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
    INVOKE(tabIcon, (tabName));
    return getIconNameForTabName(tabName);
}

void ScriptableProxy::setTabIcon(const QString &tabName, const QString &icon)
{
    INVOKE2(setTabIcon, (tabName, icon));
    m_wnd->setTabIcon(tabName, icon);
}

bool ScriptableProxy::showBrowser(const QString &tabName)
{
    INVOKE(showBrowser, (tabName));
    ClipboardBrowser *c = fetchBrowser(tabName);
    if (c)
        m_wnd->showBrowser(c);
    return m_wnd->isVisible();
}

bool ScriptableProxy::showBrowserAt(const QString &tabName, QRect rect)
{
    INVOKE(showBrowserAt, (tabName, rect));
    setGeometryWithoutSave(m_wnd, rect);
    return showBrowser(tabName);
}

bool ScriptableProxy::showCurrentBrowser()
{
    INVOKE(showCurrentBrowser, ());
    return showBrowser(m_tabName);
}

void ScriptableProxy::action(const QVariantMap &arg1, const Command &arg2)
{
    INVOKE2(action, (arg1, arg2));
    m_wnd->action(arg1, arg2, QModelIndex());
}

void ScriptableProxy::runInternalAction(const QVariantMap &data, const QString &command)
{
    INVOKE2(runInternalAction, (data, command));
    auto action = new Action();
    action->setCommand(command);
    action->setData(data);
    m_wnd->runInternalAction(action);
}

void ScriptableProxy::showMessage(const QString &title,
        const QString &msg,
        const QString &icon,
        int msec,
        const QString &notificationId,
        const NotificationButtons &buttons)
{
    INVOKE2(showMessage, (title, msg, icon, msec, notificationId, buttons));

    auto notification = m_wnd->createNotification(notificationId);
    notification->setTitle(title);
    notification->setMessage(msg);
    notification->setIcon(icon);
    notification->setInterval(msec);
    notification->setButtons(buttons);
}

QVariantMap ScriptableProxy::nextItem(int where)
{
    INVOKE(nextItem, (where));
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

void ScriptableProxy::browserMoveToClipboard(int row)
{
    INVOKE2(browserMoveToClipboard, (row));
    ClipboardBrowser *c = fetchBrowser();
    m_wnd->moveToClipboard(c, row);
}

void ScriptableProxy::browserSetCurrent(int arg1)
{
    INVOKE2(browserSetCurrent, (arg1));
    BROWSER(setCurrent(arg1));
}

QString ScriptableProxy::browserRemoveRows(QVector<int> rows)
{
    INVOKE(browserRemoveRows, (rows));
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
    INVOKE2(browserEditRow, (arg1));
    BROWSER(editRow(arg1));
}

void ScriptableProxy::browserEditNew(const QString &arg1, bool changeClipboard)
{
    INVOKE2(browserEditNew, (arg1, changeClipboard));
    BROWSER(editNew(arg1, changeClipboard));
}

QStringList ScriptableProxy::tabs()
{
    INVOKE(tabs, ());
    return m_wnd->tabs();
}

bool ScriptableProxy::toggleVisible()
{
    INVOKE(toggleVisible, ());
    return m_wnd->toggleVisible();
}

bool ScriptableProxy::toggleMenu(const QString &tabName, int maxItemCount, QPoint position)
{
    INVOKE(toggleMenu, (tabName, maxItemCount, position));
    return m_wnd->toggleMenu(tabName, maxItemCount, position);
}

bool ScriptableProxy::toggleCurrentMenu()
{
    INVOKE(toggleCurrentMenu, ());
    return m_wnd->toggleMenu();
}

int ScriptableProxy::findTabIndex(const QString &arg1)
{
    INVOKE(findTabIndex, (arg1));
    return m_wnd->findTabIndex(arg1);
}

void ScriptableProxy::openActionDialog(const QVariantMap &arg1)
{
    INVOKE2(openActionDialog, (arg1));
    m_wnd->openActionDialog(arg1);
}

bool ScriptableProxy::loadTab(const QString &arg1)
{
    INVOKE(loadTab, (arg1));
    return m_wnd->loadTab(arg1);
}

bool ScriptableProxy::saveTab(const QString &arg1)
{
    INVOKE(saveTab, (arg1));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return false;

    const int i = m_wnd->findTabIndex( c->tabName() );
    return m_wnd->saveTab(arg1, i);
}

bool ScriptableProxy::importData(const QString &fileName)
{
    INVOKE(importData, (fileName));
    return m_wnd->importData(fileName, ImportOptions::All);
}

bool ScriptableProxy::exportData(const QString &fileName)
{
    INVOKE(exportData, (fileName));
    return m_wnd->exportAllData(fileName);
}

QVariant ScriptableProxy::config(const QStringList &nameValue)
{
    INVOKE(config, (nameValue));
    return m_wnd->config(nameValue);
}

QVariant ScriptableProxy::toggleConfig(const QString &optionName)
{
    INVOKE(toggleConfig, (optionName));

    QStringList nameValue(optionName);
    const auto values = m_wnd->config(nameValue);
    if ( values.type() == QVariant::StringList )
        return values;

    const auto oldValue = values.toMap().constBegin().value();
    if ( oldValue.type() != QVariant::Bool )
        return QVariant();

    const auto newValue = !QVariant(oldValue).toBool();
    nameValue.append( QVariant(newValue).toString() );
    return m_wnd->config(nameValue).toMap().constBegin().value();
}

QByteArray ScriptableProxy::getClipboardData(const QString &mime, ClipboardMode mode)
{
    INVOKE(getClipboardData, (mime, mode));
    const QMimeData *data = clipboardData(mode);
    if (!data)
        return QByteArray();

    if (mime == "?")
        return data->formats().join("\n").toUtf8() + '\n';

    return cloneData(*data, QStringList(mime)).value(mime).toByteArray();
}

bool ScriptableProxy::hasClipboardFormat(const QString &mime, ClipboardMode mode)
{
    INVOKE(hasClipboardFormat, (mime, mode));
    const QMimeData *data = clipboardData(mode);
    return data && data->hasFormat(mime);
}

int ScriptableProxy::browserLength()
{
    INVOKE(browserLength, ());
    ClipboardBrowser *c = fetchBrowser();
    return c ? c->length() : 0;
}

bool ScriptableProxy::browserOpenEditor(const QByteArray &arg1, bool changeClipboard)
{
    INVOKE(browserOpenEditor, (arg1, changeClipboard));
    ClipboardBrowser *c = fetchBrowser();
    return c && c->openEditor(arg1, changeClipboard);
}

QString ScriptableProxy::browserInsert(int row, const QVector<QVariantMap> &items)
{
    INVOKE(browserInsert, (row, items));

    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return "Invalid tab";

    if ( !c->allocateSpaceForNewItems(items.size()) )
        return "Tab is full (cannot remove any items)";

    for (const auto &item : items) {
        if ( !c->add(item, row) )
            return "Failed to new add items";
    }

    return QString();
}

bool ScriptableProxy::browserChange(const QVariantMap &data, int row)
{
    INVOKE(browserChange, (data, row));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return false;

    const auto index = c->index(row);
    QVariantMap itemData = c->model()->data(index, contentType::data).toMap();
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        if ( it.value().isValid() )
            itemData.insert( it.key(), it.value() );
        else
            itemData.remove( it.key() );
    }

    return c->model()->setData(index, itemData, contentType::data);
}

QByteArray ScriptableProxy::browserItemData(int arg1, const QString &arg2)
{
    INVOKE(browserItemData, (arg1, arg2));
    return itemData(arg1, arg2);
}

QVariantMap ScriptableProxy::browserItemData(int arg1)
{
    INVOKE(browserItemData, (arg1));
    return itemData(arg1);
}

void ScriptableProxy::setCurrentTab(const QString &tabName)
{
    INVOKE2(setCurrentTab, (tabName));
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
    INVOKE(tab, ());
    ClipboardBrowser *c = fetchBrowser();
    return c ? c->tabName() : QString();
}

int ScriptableProxy::currentItem()
{
    INVOKE(currentItem, ());

    const QPersistentModelIndex current =
            m_actionData.value(mimeCurrentItem).value<QPersistentModelIndex>();
    return current.isValid() ? current.row() : -1;
}

bool ScriptableProxy::selectItems(const QVector<int> &rows)
{
    INVOKE(selectItems, (rows));
    ClipboardBrowser *c = fetchBrowser();
    if (!c)
        return false;

    c->clearSelection();

    if ( !rows.isEmpty() ) {
        c->setCurrent(rows.last());

        for (int i : rows) {
            const QModelIndex index = c->index(i);
            if ( index.isValid() && !c->isFiltered(i) )
                c->selectionModel()->select(index, QItemSelectionModel::Select);
        }
    }

    return true;
}

QVector<int> ScriptableProxy::selectedItems()
{
    INVOKE(selectedItems, ());

    QVector<int> selectedRows;
    const QList<QPersistentModelIndex> selected = selectedIndexes();
    selectedRows.reserve(selected.count());
    for (const auto &index : selected) {
        if (index.isValid())
            selectedRows.append(index.row());
    }

    return selectedRows;
}

int ScriptableProxy::selectedItemsDataCount()
{
    INVOKE(selectedItemsDataCount, ());
    return selectedIndexes().size();
}

QVariantMap ScriptableProxy::selectedItemData(int selectedIndex)
{
    INVOKE(selectedItemData, (selectedIndex));

    auto c = currentBrowser();
    if (!c)
        return QVariantMap();

    const auto index = selectedIndexes().value(selectedIndex);
    Q_ASSERT( !index.isValid() || index.model() == c->model() );
    return c->copyIndex(index);
}

bool ScriptableProxy::setSelectedItemData(int selectedIndex, const QVariantMap &data)
{
    INVOKE(setSelectedItemData, (selectedIndex, data));

    auto c = currentBrowser();
    if (!c)
        return false;

    const auto index = selectedIndexes().value(selectedIndex);
    if ( !index.isValid() )
        return false;

    Q_ASSERT( index.model() == c->model() );
    return c->model()->setData(index, data, contentType::data);
}

QVector<QVariantMap> ScriptableProxy::selectedItemsData()
{
    INVOKE(selectedItemsData, ());

    auto c = currentBrowser();
    if (!c)
        return QVector<QVariantMap>();

    const auto model = c->model();

    QVector<QVariantMap> dataList;
    const auto selected = selectedIndexes();
    dataList.reserve(selected.size());

    for (const auto &index : selected) {
        if ( index.isValid() ) {
            Q_ASSERT( index.model() == model );
            dataList.append( c->copyIndex(index) );
        }
    }

    return dataList;
}

void ScriptableProxy::setSelectedItemsData(const QVector<QVariantMap> &dataList)
{
    INVOKE2(setSelectedItemsData, (dataList));

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
    INVOKE2(sendKeys, (keys, delay));
    m_sentKeyClicks = m_wnd->sendKeyClicks(keys, delay);
}

bool ScriptableProxy::keysSent()
{
    INVOKE(keysSent, ());
    QCoreApplication::processEvents();
    return m_wnd->lastReceivedKeyClicks() >= m_sentKeyClicks;
}

QString ScriptableProxy::testSelected()
{
    INVOKE(testSelected, ());

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
#endif // HAS_TESTS

void ScriptableProxy::serverLog(const QString &text)
{
    INVOKE2(serverLog, (text));
    log(text, LogAlways);
}

QString ScriptableProxy::currentWindowTitle()
{
    INVOKE(currentWindowTitle, ());
    PlatformWindowPtr window = createPlatformNativeInterface()->getCurrentWindow();
    return window ? window->getTitle() : QString();
}

NamedValueList ScriptableProxy::inputDialog(const NamedValueList &values)
{
    INVOKE(inputDialog, (values));

    InputDialog inputDialog(m_wnd);
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
    QObject::connect( &buttons, SIGNAL(accepted()), &dialog, SLOT(accept()) );
    QObject::connect( &buttons, SIGNAL(rejected()), &dialog, SLOT(reject()) );
    layout.addWidget(&buttons);

    installShortcutToCloseDialog(&dialog, &dialog, Qt::CTRL | Qt::Key_Enter);
    installShortcutToCloseDialog(&dialog, &dialog, Qt::CTRL | Qt::Key_Return);

    if (icon.isNull())
        icon = appIcon();
    dialog.setWindowIcon(icon);

    dialog.show();
    raiseWindow(&dialog);

    if ( !dialog.exec() )
        return NamedValueList();

    NamedValueList result;
    result.reserve( widgets.size() );

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
    INVOKE2(setUserValue, (key, value));
    Settings settings;
    settings.beginGroup("script");
    settings.setValue(key, value);
}

void ScriptableProxy::setSelectedItemsData(const QString &mime, const QVariant &value)
{
    INVOKE2(setSelectedItemsData, (mime, value));
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
    INVOKE2(filter, (text));
    m_wnd->setFilter(text);
}

QVector<Command> ScriptableProxy::commands()
{
    INVOKE(commands, ());
    return loadAllCommands();
}

void ScriptableProxy::setCommands(const QVector<Command> &commands)
{
    INVOKE2(setCommands, (commands));
    m_wnd->setCommands(commands);
}

void ScriptableProxy::addCommands(const QVector<Command> &commands)
{
    INVOKE2(addCommands, (commands));
    m_wnd->addCommands(commands);
}

QByteArray ScriptableProxy::screenshot(const QString &format, const QString &screenName, bool select)
{
    INVOKE(screenshot, (format, screenName, select));

    QScreen *selectedScreen = nullptr;
    if ( screenName.isEmpty() ) {
        const auto mousePosition = QCursor::pos();
        for ( const auto screen : QApplication::screens() ) {
            if ( screen->geometry().contains(mousePosition) ) {
                selectedScreen = screen;
                break;
            }
        }
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

    const auto geometry = selectedScreen->geometry();

    if (select) {
        ScreenshotRectWidget rectWidget(pixmap);
        rectWidget.setGeometry(geometry);
        rectWidget.setWindowState(Qt::WindowFullScreen);
        rectWidget.setWindowModality(Qt::ApplicationModal);
        rectWidget.show();
        raiseWindow(&rectWidget);

        while ( !rectWidget.isHidden() )
            QCoreApplication::processEvents();
        const auto rect = rectWidget.selectionRect;
        if ( rect.isValid() ) {
            const auto ratio = pixmap.devicePixelRatio();
            const QRect rect2( rect.topLeft() * ratio, rect.size() * ratio );
            pixmap = pixmap.copy(rect2);
        }
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

QStringList ScriptableProxy::screenNames()
{
    INVOKE(screenNames, ());

    QStringList result;
    const auto screens = QApplication::screens();
    result.reserve( screens.size() );

    for ( const auto screen : screens )
        result.append(screen->name());

    return result;
}

Qt::KeyboardModifiers ScriptableProxy::queryKeyboardModifiers()
{
    INVOKE(queryKeyboardModifiers, ());
    return QApplication::queryKeyboardModifiers();
}

QString ScriptableProxy::pluginsPath()
{
    INVOKE(pluginsPath, ());
    return ::pluginsPath();
}

QString ScriptableProxy::themesPath()
{
    INVOKE(themesPath, ());
    return ::themesPath();
}

QString ScriptableProxy::translationsPath()
{
    INVOKE(translationsPath, ());
    return ::translationsPath();
}

QString ScriptableProxy::iconColor()
{
    INVOKE(iconColor, ());
    const auto color = m_wnd->sessionIconColor();
    return color.isValid() ? color.name() : QString();
}

bool ScriptableProxy::setIconColor(const QString &colorName)
{
    INVOKE(setIconColor, (colorName));

    QColor color(colorName);
    if ( !colorName.isEmpty() && !color.isValid() )
        return false;

    m_wnd->setSessionIconColor(color);
    return true;
}

QString ScriptableProxy::iconTag()
{
    INVOKE(iconTag, ());
    return m_wnd->sessionIconTag();
}

void ScriptableProxy::setIconTag(const QString &tag)
{
    INVOKE2(setIconTag, (tag));
    m_wnd->setSessionIconTag(tag);
}

QString ScriptableProxy::iconTagColor()
{
    INVOKE(iconTagColor, ());
    return m_wnd->sessionIconTagColor().name();
}

bool ScriptableProxy::setIconTagColor(const QString &colorName)
{
    INVOKE(setIconTagColor, (colorName));
    QColor color(colorName);
    if ( !color.isValid() )
        return false;

    m_wnd->setSessionIconTagColor(color);
    return true;
}

void ScriptableProxy::setClipboardData(const QVariantMap &data)
{
    INVOKE2(setClipboardData, (data));
    m_wnd->setClipboardData(data);
}

void ScriptableProxy::setTitle(const QString &title)
{
    INVOKE2(setTitle, (title));

    const QString sessionName = qApp->property("CopyQ_session_name").toString();
    if (title.isEmpty()) {
        m_wnd->setWindowTitle("CopyQ");
        m_wnd->setTrayTooltip("CopyQ");
    } else {
        const QString fullTitle = sessionName.isEmpty()
                ? tr("%1 - CopyQ",
                     "Main window title format (%1 is clipboard content label)")
                  .arg(title)
                : tr("%1 - %2 - CopyQ",
                     "Main window title format (%1 is clipboard content label, %2 is session name)")
                  .arg(title, sessionName);

        m_wnd->setWindowTitle(fullTitle);
        m_wnd->setTrayTooltip(fullTitle);
    }
}

void ScriptableProxy::setTitleForData(const QVariantMap &data)
{
    INVOKE2(setTitleForData, (data));

    const QString clipboardContent = textLabelForData(data);
    setTitle(clipboardContent);
}

void ScriptableProxy::saveData(const QString &tab, const QVariantMap &data)
{
    INVOKE2(saveData, (tab, data));

    auto c = m_wnd->tab(tab);
    if (c) {
        c->addUnique(data);
        c->setCurrent(0);
    }
}

void ScriptableProxy::showDataNotification(const QVariantMap &data)
{
    INVOKE2(showDataNotification, (data));

    const AppConfig appConfig;
    const auto maxLines = appConfig.option<Config::clipboard_notification_lines>();
    if (maxLines <= 0)
        return;

    const auto intervalSeconds = appConfig.option<Config::item_popup_interval>();
    if (intervalSeconds == 0)
        return;

    auto notification = m_wnd->createNotification("CopyQ_clipboard_notification");
    notification->setIcon( QString(QChar(IconPaste)) );
    notification->setInterval(intervalSeconds * 1000);

    const int maximumWidthPoints = appConfig.option<Config::notification_maximum_width>();
    const int width = pointsToPixels(maximumWidthPoints) - 16 - 8;

    const QStringList formats = data.keys();
    const int imageIndex = formats.indexOf(QRegExp("^image/.*"));
    const QFont &font = notification->font();
    const bool isHidden = data.contains(mimeHidden);

    if (data.isEmpty()) {
        notification->setInterval(0);
    } if ( !isHidden && data.contains(mimeText) ) {
        QString text = getTextData(data);
        const int n = text.count('\n') + 1;

        QString format;
        if (n > 1) {
            format = QObject::tr("%1<div align=\"right\"><small>&mdash; %n lines &mdash;</small></div>",
                                 "Notification label for multi-line text in clipboard", n);
        } else {
            format = QObject::tr("%1", "Notification label for single-line text in clipboard");
        }

        text = elideText(text, font, QString(), false, width, maxLines);
        text = escapeHtml(text);
        text.replace( QString("\n"), QString("<br />") );
        notification->setMessage( format.arg(text), Qt::RichText );
    } else if (!isHidden && imageIndex != -1) {
        QPixmap pix;
        const QString &imageFormat = formats[imageIndex];
        pix.loadFromData( data[imageFormat].toByteArray(), imageFormat.toLatin1() );

        const int height = maxLines * QFontMetrics(font).lineSpacing();
        if (pix.width() > width || pix.height() > height)
            pix = pix.scaled(QSize(width, height), Qt::KeepAspectRatio);

        notification->setPixmap(pix);
    } else {
        const QString text = textLabelForData(data, font, QString(), false, width, maxLines);
        notification->setMessage(text, Qt::PlainText);
    }
}

QStringList ScriptableProxy::menuItemMatchCommands(int actionId)
{
    INVOKE(menuItemMatchCommands, (actionId));
    return m_wnd->menuItemMatchCommands(actionId);
}

bool ScriptableProxy::enableMenuItem(int actionId, int menuItemMatchCommandIndex, bool enabled)
{
    INVOKE(enableMenuItem, (actionId, menuItemMatchCommandIndex, enabled));
    return m_wnd->setMenuItemEnabled(actionId, menuItemMatchCommandIndex, enabled);
}

QVariantMap ScriptableProxy::setDisplayData(int actionId, const QVariantMap &displayData)
{
    INVOKE(setDisplayData, (actionId, displayData));
    m_actionData = m_wnd->setDisplayData(actionId, displayData);
    return m_actionData;
}

QVector<Command> ScriptableProxy::automaticCommands()
{
    INVOKE(automaticCommands, ());
    return m_wnd->automaticCommands();
}

QVector<Command> ScriptableProxy::displayCommands()
{
    INVOKE(displayCommands, ());
    return m_wnd->displayCommands();
}

QVector<Command> ScriptableProxy::scriptCommands()
{
    INVOKE(scriptCommands, ());
    return m_wnd->scriptCommands();
}

bool ScriptableProxy::openUrls(const QStringList &urls)
{
    INVOKE(openUrls, (urls));

    for (const auto &url : urls) {
        if ( !QDesktopServices::openUrl(QUrl(url)) )
            return false;
    }

    return true;
}

ClipboardBrowser *ScriptableProxy::fetchBrowser(const QString &tabName)
{
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
    auto c = fetchBrowser();
    return c ? c->copyIndex( c->index(i) ) : QVariantMap();
}

QByteArray ScriptableProxy::itemData(int i, const QString &mime)
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

ClipboardBrowser *ScriptableProxy::currentBrowser() const
{
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
