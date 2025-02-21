// SPDX-License-Identifier: GPL-3.0-or-later

#include "scriptableproxy.h"

#include "common/action.h"
#include "common/appconfig.h"
#include "common/command.h"
#include "common/commandstatus.h"
#include "common/commandstore.h"
#include "common/common.h"
#include "common/config.h"
#include "common/contenttype.h"
#include "common/display.h"
#include "common/log.h"
#include "common/mimetypes.h"
#include "common/sleeptimer.h"
#include "common/textdata.h"
#include "gui/clipboardbrowser.h"
#include "gui/filedialog.h"
#include "gui/iconfactory.h"
#include "gui/icons.h"
#include "gui/mainwindow.h"
#include "gui/notification.h"
#include "gui/pixelratio.h"
#include "gui/screen.h"
#include "gui/selectiondata.h"
#include "gui/tabicons.h"
#include "gui/traymenu.h"
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
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
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
#include <QStyleFactory>
#include <QTextEdit>
#include <QUrl>

#ifdef HAS_TESTS
#   include <QTest>
#endif

#include <type_traits>

namespace {

const quint32 serializedFunctionCallMagicNumber = 0x58746908;
const quint32 serializedFunctionCallVersion = 2;

void registerMetaTypes() {
    static bool registered = false;
    if (registered)
        return;

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    qRegisterMetaType<QPointer<QWidget>>("QPointer<QWidget>");
    qRegisterMetaTypeStreamOperators<ClipboardMode>("ClipboardMode");
    qRegisterMetaTypeStreamOperators<Command>("Command");
    qRegisterMetaTypeStreamOperators<NamedValueList>("NamedValueList");
    qRegisterMetaTypeStreamOperators<NotificationButtonList>("NotificationButtonList");
    qRegisterMetaTypeStreamOperators<QVector<int>>("QVector<int>");
    qRegisterMetaTypeStreamOperators<QVector<Command>>("QVector<Command>");
    qRegisterMetaTypeStreamOperators<VariantMapList>("VariantMapList");
    qRegisterMetaTypeStreamOperators<KeyboardModifierList>("KeyboardModifierList");
#else
    qRegisterMetaType<QPointer<QWidget>>("QPointer<QWidget>");
    qRegisterMetaType<ClipboardMode>("ClipboardMode");
    qRegisterMetaType<Command>("Command");
    qRegisterMetaType<NamedValueList>("NamedValueList");
    qRegisterMetaType<NotificationButtonList>("NotificationButtonList");
    qRegisterMetaType<QVector<int>>("QVector<int>");
    qRegisterMetaType<QVector<Command>>("QVector<Command>");
    qRegisterMetaType<VariantMapList>("VariantMapList");
    qRegisterMetaType<KeyboardModifierList>("KeyboardModifierList");
#endif

    registered = true;
}

template<typename Predicate>
void selectionRemoveIf(QList<QPersistentModelIndex> *indexes, Predicate predicate)
{
    indexes->erase(
        std::remove_if(indexes->begin(), indexes->end(), predicate),
        indexes->end());
}

void selectionRemoveInvalid(QList<QPersistentModelIndex> *indexes)
{
    selectionRemoveIf(
        indexes,
        [](const QPersistentModelIndex &index){
            return !index.isValid();
        });
}

} // namespace

#define BROWSER(tabName, call) \
    ClipboardBrowser *c = fetchBrowser(tabName); \
    if (c) \
        (c->call)

#define STR(str) str

#define INVOKE_(function, arguments, functionCallId) do { \
    static const auto f = FunctionCallSerializer(QByteArrayLiteral(STR(#function))).withSlotArguments arguments; \
    const auto args = f.argumentList arguments; \
    emit sendMessage(f.serialize(functionCallId, args), CommandFunctionCall); \
} while(false)

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#   define CHECK_STREAM_OPERATORS(CALL) \
        static constexpr auto metaType = QMetaType::fromType<Result>(); \
        COPYQ_LOG_VERBOSE( \
            QStringLiteral("%1 invoking: %2 " CALL)\
                .arg(m_wnd ? "Server" : "Client") \
                .arg(metaType.name())); \
        Q_ASSERT(metaType.hasRegisteredDataStreamOperators())
#else
#   define CHECK_STREAM_OPERATORS(CALL) \
        if ( hasLogLevel(LogTrace) ) { \
            static const auto metaTypeName = QMetaType::typeName(qMetaTypeId<Result>()); \
            COPYQ_LOG_VERBOSE( \
                QStringLiteral("%1 invoking: %2 " CALL)\
                    .arg(m_wnd ? "Server" : "Client") \
                    .arg(metaTypeName) \
            ); \
        }
#endif

#define INVOKE(FUNCTION, ARGUMENTS) do { \
    using Result = decltype(FUNCTION ARGUMENTS); \
    CHECK_STREAM_OPERATORS(STR(#FUNCTION #ARGUMENTS)); \
    if (!m_wnd) { \
        const auto functionCallId = ++m_lastFunctionCallId; \
        INVOKE_(FUNCTION, ARGUMENTS, functionCallId); \
        const auto result = waitForFunctionCallFinished(functionCallId); \
        return result.value<Result>(); \
    } \
} while(false)

#define INVOKE2(FUNCTION, ARGUMENTS) do { \
    if (!m_wnd) { \
        const auto functionCallId = ++m_lastFunctionCallId; \
        INVOKE_(FUNCTION, ARGUMENTS, functionCallId); \
        waitForFunctionCallFinished(functionCallId); \
        return; \
    } \
} while(false)

Q_DECLARE_METATYPE(QFile*)

QDataStream &operator<<(QDataStream &out, const NotificationButtonList &list)
{
    out << list.items.size();
    for (const auto &button : list.items)
        out << button.name << button.script << button.data;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, NotificationButtonList &list)
{
    decltype(list.items.size()) size;
    in >> size;
    list.items.reserve(size);
    for (int i = 0; i < size; ++i) {
        NotificationButton button;
        in >> button.name >> button.script >> button.data;
        list.items.append(button);
    }
    Q_ASSERT(in.status() == QDataStream::Ok);
    return in;
}

QDataStream &operator<<(QDataStream &out, const NamedValueList &list)
{
    out << list.items.size();
    for (const auto &item : list.items)
        out << item.name << item.value;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, NamedValueList &list)
{
    decltype(list.items.size()) size;
    in >> size;
    list.items.reserve(size);
    for (int i = 0; i < size; ++i) {
        NamedValue item;
        in >> item.name >> item.value;
        list.items.append(item);
    }
    Q_ASSERT(in.status() == QDataStream::Ok);
    return in;
}

QDataStream &operator<<(QDataStream &out, const VariantMapList &items)
{
    out << items.items;
    Q_ASSERT(out.status() == QDataStream::Ok);
    return out;
}

QDataStream &operator>>(QDataStream &in, VariantMapList &items)
{
    in >> items.items;
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

QDataStream &operator<<(QDataStream &out, KeyboardModifierList value)
{
    return out << static_cast<int>(value.items);
}

QDataStream &operator>>(QDataStream &in, KeyboardModifierList &value)
{
    int valueInt;
    in >> valueInt;
    Q_ASSERT(in.status() == QDataStream::Ok);
    value.items = static_cast<Qt::KeyboardModifiers>(valueInt);
    return in;
}

namespace {

const char propertyWidgetName[] = "CopyQ_widget_name";
const char propertyWidgetProperty[] = "CopyQ_widget_property";

struct InputDialog {
    QPointer<QDialog> dialog;
    QVariant defaultChoice; /// Default text for list widgets.
};

class FunctionCallSerializer final {
public:
    explicit FunctionCallSerializer(QByteArray functionName)
        : m_slotName(std::move(functionName))
    {
    }

    template<typename ...Ts>
    FunctionCallSerializer &withSlotArguments(Ts... arguments)
    {
        QByteArray args;
        for (const auto argType : std::initializer_list<const char *>{ argumentType(arguments)... }) {
            args.append(argType);
            args.append(',');
        }
        args.chop(1);
        setSlotArgumentTypes(args);
        return *this;
    }

    QByteArray serialize(int functionCallId, const QVector<QVariant> args) const
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        stream.setVersion(QDataStream::Qt_6_0);
#else
        stream.setVersion(QDataStream::Qt_5_0);
#endif
        stream << serializedFunctionCallMagicNumber << serializedFunctionCallVersion
               << functionCallId << m_slotName << args;
        return bytes;
    }

    template<typename ...Ts>
    static QVector<QVariant> argumentList(Ts... arguments)
    {
        return { QVariant::fromValue(arguments)... };
    }

    template<typename T>
    static const char *argumentType(const T &)
    {
        if ( std::is_same<QVariant, T>::value )
            return "QVariant";

        return QMetaType::typeName(qMetaTypeId<T>());
    }

private:
    void setSlotArgumentTypes(const QByteArray &args)
    {
        m_slotName += "(" + args + ")";
        const int slotIndex = ScriptableProxy::staticMetaObject.indexOfSlot(m_slotName);
        if (slotIndex == -1) {
            log("Failed to find scriptable proxy slot: " + m_slotName, LogError);
            Q_ASSERT(false);
        }
    }

    QByteArray m_slotName;
};

class ScreenshotRectWidget final : public QLabel {
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
            const auto w = pointsToPixels(1, this);

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

void installShortcutToCloseDialog(QDialog *dialog, QWidget *shortcutParent, int shortcut)
{
    QShortcut *s = new QShortcut(QKeySequence(shortcut), shortcutParent);
    QObject::connect(s, &QShortcut::activated, dialog, &QDialog::accept);
    QObject::connect(s, &QShortcut::activatedAmbiguously, dialog, &QDialog::accept);
}

QWidget *createListWidget(const QString &name, const QStringList &items, InputDialog *inputDialog)
{
    QDialog *parent = inputDialog->dialog;

    const QString currentText = inputDialog->defaultChoice.isValid()
            ? inputDialog->defaultChoice.toString()
            : items.value(0);

    const QLatin1String listPrefix(".list:");
    if ( name.startsWith(listPrefix) ) {
        QListWidget *w = createAndSetWidget<QListWidget>("currentRow", QVariant(), parent);
        w->addItems(items);
        const int i = items.indexOf(currentText);
        if (i != -1)
            w->setCurrentRow(i);
        w->setAlternatingRowColors(true);
        installShortcutToCloseDialog(parent, w, Qt::Key_Enter);
        installShortcutToCloseDialog(parent, w, Qt::Key_Return);
        return label(Qt::Vertical, name.mid(listPrefix.size()), w);
    }

    QComboBox *w = createAndSetWidget<QComboBox>("currentText", QVariant(), parent);
    w->setEditable(true);
    w->addItems(items);
    w->setCurrentIndex(items.indexOf(currentText));
    w->lineEdit()->setText(currentText);
    w->lineEdit()->selectAll();
    w->setMaximumWidth( pointsToPixels(400, w) );
    installShortcutToCloseDialog(parent, w, Qt::Key_Enter);
    installShortcutToCloseDialog(parent, w, Qt::Key_Return);

    const QLatin1String comboPrefix(".combo:");
    if ( name.startsWith(comboPrefix) ) {
        w->setEditable(false);
        return label(Qt::Horizontal, name.mid(comboPrefix.size()), w);
    }

    return label(Qt::Horizontal, name, w);
}

QWidget *createSpinBox(const QString &name, const QVariant &value, QWidget *parent)
{
    QSpinBox *w = createAndSetWidget<QSpinBox>("value", value, parent);
    w->setRange(-1e9, 1e9);
    return label(Qt::Horizontal, name, w);
}

QLineEdit *createLineEdit(const QVariant &value, QWidget *parent)
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

    QLineEdit *lineEdit = createLineEdit(path, w);
    lineEdit->setProperty(propertyWidgetName, name);

    QPushButton *browseButton = new QPushButton("...");

    FileDialog *dialog = new FileDialog(w, name, path);
    QObject::connect( browseButton, &QAbstractButton::clicked,
                      dialog, &FileDialog::exec );
    QObject::connect( dialog, &FileDialog::fileSelected,
                      lineEdit, &QLineEdit::setText );

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
    QDialog *parent = inputDialog->dialog;

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
        if ( value.type() == QVariant::Url ) {
            const auto path = value.toUrl();
            return createFileNameEdit(name, path.toLocalFile(), parent);
        }

        const QString text = value.toString();
        if (text.contains('\n'))
            return createTextEdit(name, value.toStringList(), parent);

        return label(Qt::Horizontal, name, createLineEdit(value, parent));
    }
}

void setGeometryWithoutSave(QWidget *window, QRect geometry)
{
    setGeometryGuardBlockedUntilHidden(window, true);

    const auto pos = (geometry.x() == -1 && geometry.y() == -1)
            ? QCursor::pos()
            : geometry.topLeft();

    const int w = pointsToPixels(geometry.width(), window);
    const int h = pointsToPixels(geometry.height(), window);
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

} // namespace

#ifdef HAS_TESTS
class KeyClicker final : public QObject {
public:
    KeyClicker(MainWindow *wnd, QObject *parent)
        : QObject(parent)
        , m_wnd(wnd)
    {
    }

    void keyClicksRetry(const QString &expectedWidgetName, const QString &keys, int delay, int retry)
    {
        if (retry > 0)
            sendKeyClicks(expectedWidgetName, keys, delay + 100, retry - 1);
        else
            keyClicksFailed(expectedWidgetName);
    }

    void keyClicksFailed(const QString &expectedWidgetName)
    {
        auto actual = keyClicksTarget();
        auto popup = QApplication::activePopupWidget();
        auto widget = QApplication::focusWidget();
        auto window = QApplication::activeWindow();
        auto modal = QApplication::activeModalWidget();
        const auto currentWindow = platformNativeInterface()->getCurrentWindow();
        const auto currentWindowTitle = currentWindow ? currentWindow->getTitle() : QString();
        log( QString("Failed to send key press to target widget")
            + QLatin1String(qApp->applicationState() == Qt::ApplicationActive ? "" : "\nApp is INACTIVE!")
            + "\nExpected: " + (expectedWidgetName.isEmpty() ? "Any" : expectedWidgetName)
            + "\nActual:   " + keyClicksTargetDescription(actual)
            + "\nPopup:    " + keyClicksTargetDescription(popup)
            + "\nWidget:   " + keyClicksTargetDescription(widget)
            + "\nWindow:   " + keyClicksTargetDescription(window)
            + "\nModal:    " + keyClicksTargetDescription(modal)
            + "\nTitle:    " + currentWindowTitle
            , LogError );

        m_failed = true;
    }

    void keyClicks(const QString &expectedWidgetName, const QString &keys, int delay, int retry)
    {
        auto widget = keyClicksTarget();
        if (!widget) {
            keyClicksRetry(expectedWidgetName, keys, delay, retry);
            return;
        }

        if (qApp->applicationState() != Qt::ApplicationActive) {
#if defined(Q_OS_MAC) && QT_VERSION >= QT_VERSION_CHECK(6,0,0)
            // WORKAROUND for focusing back to main window on macOS.
            if (m_wnd->isVisible())
                m_wnd->activateWindow();
#endif
            keyClicksRetry(expectedWidgetName, keys, delay, retry);
            return;
        }

        auto widgetName = keyClicksTargetDescription(widget);
        if ( !expectedWidgetName.isEmpty() && !widgetName.contains(expectedWidgetName) ) {
            keyClicksRetry(expectedWidgetName, keys, delay, retry);
            return;
        }

        // Only verified focused widget.
        if ( keys.isEmpty() ) {
            m_succeeded = true;
            return;
        }

        // There could be some animation/transition effect on check boxes
        // so wait for checkbox to be set.
        if ( qobject_cast<QCheckBox*>(widget) )
            waitFor(100);

        COPYQ_LOG( QString("Sending keys \"%1\" to %2.")
                   .arg(keys, widgetName) );

        const auto popupMessage = QString::fromLatin1("%1 (%2)")
                .arg( quoteString(keys), widgetName );
        auto notification = m_wnd->createNotification(QLatin1String("tests"));
        notification->setMessage(popupMessage);
        notification->setIcon(IconKeyboard);
        notification->setInterval(2000);

        if ( keys.startsWith(":") ) {
            const auto text = keys.mid(1);

            QTest::keyClicks(widget, text, Qt::NoModifier, 0);

            // Increment key clicks sequence number after typing all the text.
            m_succeeded = true;
        } else {
            const QKeySequence shortcut(keys, QKeySequence::PortableText);

            if ( shortcut.isEmpty() ) {
                log( QString("Cannot parse shortcut \"%1\"!").arg(keys), LogError );
                m_failed = true;
                return;
            }

            // Increment key clicks sequence number before opening any modal dialogs.
            m_succeeded = true;

            const auto key = static_cast<uint>(shortcut[0]);
            QTest::keyClick( widget,
                             Qt::Key(key & ~Qt::KeyboardModifierMask),
                             Qt::KeyboardModifiers(key & Qt::KeyboardModifierMask),
                             0 );
        }

        COPYQ_LOG( QString("Key \"%1\" sent to %2.")
                   .arg(keys, widgetName) );
    }

    void sendKeyClicks(const QString &expectedWidgetName, const QString &keys, int delay, int retry)
    {
        m_succeeded = false;
        m_failed = false;

        // Don't stop when modal window is open.
        auto t = new QTimer(m_wnd);
        t->setSingleShot(true);
        QObject::connect( t, &QTimer::timeout, this, [=]() {
            keyClicks(expectedWidgetName, keys, delay, retry);
            t->deleteLater();
        });
        t->start(delay);
    }

    bool succeeded() const { return m_succeeded; }
    bool failed() const { return m_failed; }

private:
    static QString keyClicksTargetDescription(QWidget *widget)
    {
        if (widget == nullptr)
            return "None";

        const auto className = widget->metaObject()->className();

        auto widgetName = QString::fromLatin1("%1:%2")
                .arg(widget->objectName(), className);

        const auto window = widget->window();
        if (window && widget != window) {
            widgetName.append(
                QString::fromLatin1(" in %1:%2")
                    .arg(window->objectName(), window->metaObject()->className())
            );
        }

        auto parent = widget->parentWidget();
        while (parent) {
            if ( parent != window && !parent->objectName().startsWith("qt_") ) {
                widgetName.append(
                    QString::fromLatin1(" in %1:%2")
                        .arg(parent->objectName(), parent->metaObject()->className())
                );
            }
            parent = parent->parentWidget();
        }

        return widgetName;
    }

    QWidget *keyClicksTarget()
    {
        auto popup = QApplication::activePopupWidget();
        if (popup)
            return popup;

        auto widget = QApplication::focusWidget();
        if (widget)
            return widget;

        auto window = QApplication::activeWindow();
        if (window)
            return window->focusWidget();

        auto modal = QApplication::activeModalWidget();
        if (modal)
            return modal->focusWidget();

#ifdef Q_OS_MAC
        return m_wnd->focusWidget();
#else
        return nullptr;
#endif
    }


    MainWindow *m_wnd = nullptr;
    bool m_succeeded = true;
    bool m_failed = false;
};
#endif // HAS_TESTS

ScriptableProxy::ScriptableProxy(MainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_wnd(mainWindow)
{
    connect( this, &ScriptableProxy::clientDisconnected,
             this, [this]() {
                 m_disconnected = true;
                 emit abortEvaluation();
             } );

    registerMetaTypes();
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    Q_ASSERT(QMetaType::fromType<Command>().hasRegisteredDataStreamOperators());
    Q_ASSERT(QMetaType::fromType<ClipboardMode>().hasRegisteredDataStreamOperators());
#endif
}

void ScriptableProxy::callFunction(const QByteArray &serializedFunctionCall)
{
    if (m_shouldBeDeleted)
        return;

    ++m_functionCallStack;
    auto t = new QTimer(this);
    t->setSingleShot(true);
    QObject::connect( t, &QTimer::timeout, this, [=]() {
        const auto result = callFunctionHelper(serializedFunctionCall);
        emit sendMessage(result, CommandFunctionCallReturnValue);
        t->deleteLater();

        --m_functionCallStack;
        if (m_shouldBeDeleted && m_functionCallStack == 0)
            deleteLater();
    });
    t->start(0);
}

QByteArray ScriptableProxy::callFunctionHelper(const QByteArray &serializedFunctionCall)
{
    QVector<QVariant> arguments;
    QByteArray slotName;
    int functionCallId;
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

        stream >> functionCallId;
        if (stream.status() != QDataStream::Ok) {
            log("Failed to read scriptable proxy slot call ID", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }

        stream >> slotName;
        if (stream.status() != QDataStream::Ok) {
            log("Failed to read scriptable proxy slot call name", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }

        stream >> arguments;
        if (stream.status() != QDataStream::Ok) {
            log("Failed to read scriptable proxy slot call", LogError);
            Q_ASSERT(false);
            return QByteArray();
        }
    }

    const auto slotIndex = metaObject()->indexOfSlot(slotName);
    if (slotIndex == -1) {
        log("Failed to find scriptable proxy slot: " + slotName, LogError);
        Q_ASSERT(false);
        return QByteArray();
    }

    const auto metaMethod = metaObject()->method(slotIndex);
    const auto typeId = metaMethod.returnType();

    QGenericArgument args[9];
    for (int i = 0; i < arguments.size(); ++i) {
        auto &value = arguments[i];
        const int argumentTypeId = metaMethod.parameterType(i);
        if (argumentTypeId == QMetaType::QVariant) {
            args[i] = QGenericArgument( "QVariant", static_cast<void*>(value.data()) );
        } else if ( value.userType() == argumentTypeId ) {
            args[i] = QGenericArgument( value.typeName(), static_cast<void*>(value.data()) );
        } else {
            log( QString("Bad argument type (at index %1) for scriptable proxy slot: %2")
                 .arg(i)
                 .arg(metaMethod.methodSignature().constData()), LogError);
            Q_ASSERT(false);
            return QByteArray();
        }
    }

    QVariant returnValue;
    bool called;

    if (typeId == QMetaType::Void) {
        called = metaMethod.invoke(
                this, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        const QMetaType metaType(typeId);
        COPYQ_LOG_VERBOSE(QStringLiteral("Script function return type: %1").arg(metaType.name()));
        Q_ASSERT(metaType.hasRegisteredDataStreamOperators());
        returnValue = QVariant(metaType, nullptr);
#else
        returnValue = QVariant(typeId, nullptr);
#endif
        const auto genericReturnValue = returnValue.isValid()
                ? QGenericReturnArgument( returnValue.typeName(), static_cast<void*>(returnValue.data()) )
                : QGenericReturnArgument( "QVariant", static_cast<void*>(returnValue.data()) );

        called = metaMethod.invoke(
                this, genericReturnValue,
                args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
    }

    if (!called) {
        log( QString("Bad scriptable proxy slot call: %1")
             .arg(metaMethod.methodSignature().constData()), LogError);
        Q_ASSERT(false);
    }

    QByteArray bytes;
    {
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream << functionCallId << returnValue;
        if (stream.status() != QDataStream::Ok) {
            log("Failed to write scriptable proxy slot call return value", LogError);
            Q_ASSERT(false);
        }
    }

    return bytes;
}

void ScriptableProxy::setFunctionCallReturnValue(const QByteArray &bytes)
{
    QDataStream stream(bytes);
    int functionCallId;
    QVariant returnValue;
    stream >> functionCallId >> returnValue;
    if (stream.status() != QDataStream::Ok) {
        log("Failed to read scriptable proxy slot call return value", LogError);
        Q_ASSERT(false);
        return;
    }
    emit functionCallFinished(functionCallId, returnValue);
}

void ScriptableProxy::setInputDialogResult(const QByteArray &bytes)
{
    QDataStream stream(bytes);
    int dialogId;
    NamedValueList result;
    stream >> dialogId >> result;
    if (stream.status() != QDataStream::Ok) {
        log("Failed to read input dialog result", LogError);
        Q_ASSERT(false);
        return;
    }
    emit inputDialogFinished(dialogId, result);
}

void ScriptableProxy::safeDeleteLater()
{
    m_shouldBeDeleted = true;
    if (m_functionCallStack == 0)
        deleteLater();
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

bool ScriptableProxy::focusPrevious()
{
    INVOKE(focusPrevious, ());
    return m_wnd->focusPrevious();
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

    PlatformWindowPtr window = platformNativeInterface()->getCurrentWindow();
    if (!window)
        return false;
    window->pasteClipboard();
    return true;
}

bool ScriptableProxy::copyFromCurrentWindow()
{
    INVOKE(copyFromCurrentWindow, ());

    PlatformWindowPtr window = platformNativeInterface()->getCurrentWindow();
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

bool ScriptableProxy::preview(const QVariant &arg)
{
    INVOKE(preview, (arg));

    const bool wasVisible = m_wnd->isItemPreviewVisible();

    if ( arg.isValid() ) {
        const bool enable =
                arg.canConvert<bool>() ? arg.toBool()
              : arg.canConvert<int>() ? arg.toInt() != 0
              : arg.toString() == QLatin1String("true");
        m_wnd->setItemPreviewVisible(enable);
    }

    return wasVisible;
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

QStringList ScriptableProxy::unloadTabs(const QStringList &tabs)
{
    INVOKE(unloadTabs, (tabs));
    QStringList unloaded;
    for (const auto &tab : tabs) {
        if ( m_wnd->unloadTab(tab) )
            unloaded.append(tab);
    }
    return unloaded;
}

void ScriptableProxy::forceUnloadTabs(const QStringList &tabs)
{
    INVOKE2(forceUnloadTabs, (tabs));
    for (const auto &tab : tabs)
        m_wnd->forceUnloadTab(tab);
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
        const NotificationButtonList &buttons)
{
    INVOKE2(showMessage, (title, msg, icon, msec, notificationId, buttons));

    auto notification = m_wnd->createNotification(notificationId);
    notification->setTitle(title);
    notification->setMessage(msg, Qt::AutoText);
    notification->setIcon(icon);
    notification->setInterval(msec);
    notification->setButtons(buttons.items);
}

QVariantMap ScriptableProxy::nextItem(const QString &tabName, int where)
{
    INVOKE(nextItem, (tabName, where));
    ClipboardBrowser *c = fetchBrowser(tabName);
    if (!c)
        return QVariantMap();

    const int row = qMax(0, c->currentIndex().row()) + where;
    const QModelIndex index = c->index(row);

    if (!index.isValid())
        return QVariantMap();

    c->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    return c->copyIndex(index);
}

void ScriptableProxy::browserMoveToClipboard(const QString &tabName, int row)
{
    INVOKE2(browserMoveToClipboard, (tabName, row));
    ClipboardBrowser *c = fetchBrowser(tabName);
    m_wnd->moveToClipboard(c, row);
}

void ScriptableProxy::browserSetCurrent(const QString &tabName, int arg1)
{
    INVOKE2(browserSetCurrent, (tabName, arg1));
    BROWSER(tabName, setCurrent(arg1));
}

QString ScriptableProxy::browserRemoveRows(const QString &tabName, QVector<int> rows)
{
    INVOKE(browserRemoveRows, (tabName, rows));
    ClipboardBrowser *c = fetchBrowser(tabName);
    if (!c)
        return QStringLiteral("Invalid tab");

    std::sort( rows.begin(), rows.end(), std::greater<int>() );

    QModelIndexList indexes;
    indexes.reserve(rows.size());

    for (int row : rows) {
        const QModelIndex indexToRemove = c->index(row);
        if ( indexToRemove.isValid() )
            indexes.append(indexToRemove);
    }

    QString error;
    c->removeIndexes(indexes, &error);

    if ( !error.isEmpty() )
        return error;

    return QString();
}

void ScriptableProxy::browserMoveSelected(int targetRow)
{
    INVOKE2(browserMoveSelected, (targetRow));

    QList<QPersistentModelIndex> selected = selectedIndexes();
    selectionRemoveInvalid(&selected);
    ClipboardBrowser *c = browserForIndexes(selected);
    if (c == nullptr)
        return;

    QModelIndexList indexes;
    indexes.reserve(selected.size());
    for (const auto &index : selected)
        indexes.append(index);
    c->move(indexes, targetRow);
}

void ScriptableProxy::browserEditRow(const QString &tabName, int row, const QString &format)
{
    INVOKE2(browserEditRow, (tabName, row, format));
    BROWSER(tabName, editRow(row, format));
}

void ScriptableProxy::browserEditNew(const QString &tabName, const QString &format, const QByteArray &content, bool changeClipboard)
{
    INVOKE2(browserEditNew, (tabName, format, content, changeClipboard));
    BROWSER(tabName, editNew(format, content, changeClipboard));
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

int ScriptableProxy::menuItems(const VariantMapList &items)
{
    INVOKE(menuItems, (items));

    TrayMenu menu;
    menu.setObjectName("CustomMenu");
    menu.setRowIndexFromOne( AppConfig().option<Config::row_index_from_one>() );

    const auto addMenuItems = [&](const QString &searchText) {
        menu.clearClipboardItems();
        for (const QVariantMap &data : items.items) {
            const QString text = getTextData(data);
            if ( text.contains(searchText, Qt::CaseInsensitive) )
                menu.addClipboardItemAction(data, true);
        }
    };
    addMenuItems(QString());

    connect(&menu, &TrayMenu::searchRequest, addMenuItems);

    const QPoint pos = QCursor::pos();
    QAction *act = menu.exec(pos);
    if (act == nullptr)
        return -1;

    return items.items.indexOf(act->data().toMap());
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

bool ScriptableProxy::saveTab(const QString &tabName, const QString &arg1)
{
    INVOKE(saveTab, (tabName, arg1));
    ClipboardBrowser *c = fetchBrowser(tabName);
    if (!c)
        return false;

    const int i = m_wnd->findTabIndex( c->tabName() );
    return m_wnd->saveTab(arg1, i);
}

bool ScriptableProxy::importData(const QString &fileName)
{
    INVOKE(importData, (fileName));
    return m_wnd->importDataFrom(fileName, ImportOptions::All);
}

bool ScriptableProxy::exportData(const QString &fileName)
{
    INVOKE(exportData, (fileName));
    return m_wnd->exportAllData(fileName);
}

QVariant ScriptableProxy::config(const QVariantList &nameValue)
{
    INVOKE(config, (nameValue));
    return m_wnd->config(nameValue);
}

QString ScriptableProxy::configDescription()
{
    INVOKE(configDescription, ());
    return m_wnd->configDescription();
}

QVariant ScriptableProxy::toggleConfig(const QString &optionName)
{
    INVOKE(toggleConfig, (optionName));

    QVariantList nameValue;
    nameValue.append(optionName);
    const auto values = m_wnd->config(nameValue);
    if ( values.type() == QVariant::StringList )
        return values;

    const auto oldValue = values.toMap().constBegin().value();
    if ( oldValue.type() != QVariant::Bool )
        return QVariant();

    const auto newValue = !QVariant(oldValue).toBool();
    nameValue.append(newValue);
    return m_wnd->config(nameValue).toMap().constBegin().value();
}

int ScriptableProxy::browserLength(const QString &tabName)
{
    INVOKE(browserLength, (tabName));
    ClipboardBrowser *c = fetchBrowser(tabName);
    return c ? c->length() : 0;
}

bool ScriptableProxy::browserOpenEditor(
    const QString &tabName, int row, const QString &format, const QByteArray &content, bool changeClipboard)
{
    INVOKE(browserOpenEditor, (tabName, row, format, content, changeClipboard));
    ClipboardBrowser *c = fetchBrowser(tabName);
    if (!c)
        return false;

    const auto index = c->index(row);
    return c->openEditor(index, format, content, changeClipboard);
}

QString ScriptableProxy::browserInsert(const QString &tabName, int row, const VariantMapList &items)
{
    INVOKE(browserInsert, (tabName, row, items));

    ClipboardBrowser *c = fetchBrowser(tabName);
    if (!c)
        return QStringLiteral("Invalid tab");

    if ( !c->allocateSpaceForNewItems(items.items.size()) )
        return QStringLiteral("Tab is full (cannot remove any items)");

    if ( !c->addReversed(items.items, row) )
        return QStringLiteral("Failed to add items");

    return QString();
}

QString ScriptableProxy::browserChange(const QString &tabName, int row, const VariantMapList &items)
{
    INVOKE(browserChange, (tabName, row, items));

    ClipboardBrowser *c = fetchBrowser(tabName);
    if (!c)
        return QStringLiteral("Invalid tab");

    QMap<QPersistentModelIndex, QVariantMap> itemsData;

    int currentRow = row;
    for (const auto &data : items.items) {
        const auto index = c->index(currentRow);
        QVariantMap itemData = index.data(contentType::data).toMap();
        for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
            if ( it.value().isValid() )
                itemData.insert( it.key(), it.value() );
            else
                itemData.remove( it.key() );
        }
        itemsData[index] = itemData;
        ++currentRow;
    }
    c->setItemsData(itemsData);

    return QString();
}

QByteArray ScriptableProxy::browserItemData(const QString &tabName, int arg1, const QString &arg2)
{
    INVOKE(browserItemData, (tabName, arg1, arg2));
    return itemData(tabName, arg1, arg2);
}

QVariantMap ScriptableProxy::browserItemData(const QString &tabName, int arg1)
{
    INVOKE(browserItemData, (tabName, arg1));
    return itemData(tabName, arg1);
}

void ScriptableProxy::setCurrentTab(const QString &tabName)
{
    INVOKE2(setCurrentTab, (tabName));
    m_wnd->addAndFocusTab(tabName);
}

QString ScriptableProxy::tab(const QString &tabName)
{
    INVOKE(tab, (tabName));
    ClipboardBrowser *c = fetchBrowser(tabName);
    return c ? c->tabName() : QString();
}

int ScriptableProxy::currentItem()
{
    INVOKE(currentItem, ());

    const QPersistentModelIndex current = currentIndex();
    return current.isValid() ? current.row() : -1;
}

bool ScriptableProxy::selectItems(const QString &tabName, const QVector<int> &rows)
{
    INVOKE(selectItems, (tabName, rows));
    ClipboardBrowser *c = fetchBrowser(tabName);
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
    QList<QPersistentModelIndex> selected = selectedIndexes();
    selectedRows.reserve(selected.count());

    for (const auto &index : selected)
        selectedRows.append(index.row());

    return selectedRows;
}

QString ScriptableProxy::selectedTab()
{
    INVOKE(selectedTab, ());

    const QPersistentModelIndex current = currentIndex();
    ClipboardBrowser *c = m_wnd->browserForItem(current);
    if (c != nullptr)
        return c->tabName();

    const QList<QPersistentModelIndex> selected = selectedIndexes();
    c = m_wnd->browserForItem(current);
    if (c != nullptr)
        return c->tabName();

    return m_actionData.value(mimeCurrentTab).toString();
}

QVariantMap ScriptableProxy::selectedItemData(int selectedIndex)
{
    INVOKE(selectedItemData, (selectedIndex));

    const auto index = selectedIndexes().value(selectedIndex);
    if ( !index.isValid() )
        return {};

    ClipboardBrowser *c = m_wnd->browserForItem(index);
    if (c == nullptr)
        return {};

    return c->copyIndex(index);
}

bool ScriptableProxy::setSelectedItemData(int selectedIndex, const QVariantMap &data)
{
    INVOKE(setSelectedItemData, (selectedIndex, data));

    const auto index = selectedIndexes().value(selectedIndex);
    if ( !index.isValid() )
        return false;

    ClipboardBrowser *c = m_wnd->browserForItem(index);
    if (c == nullptr)
        return false;

    return c->model()->setData(index, data, contentType::data);
}

VariantMapList ScriptableProxy::selectedItemsData()
{
    INVOKE(selectedItemsData, ());

    QList<QPersistentModelIndex> selected = selectedIndexes();
    ClipboardBrowser *c = browserForIndexes(selected);
    if (c == nullptr)
        return {QVector<QVariantMap>(selected.size(), {})};

    QVector<QVariantMap> dataList;
    dataList.reserve(selected.size());

    for (const auto &index : selected)
        dataList.append( c->copyIndex(index) );

    return {dataList};
}

void ScriptableProxy::setSelectedItemsData(const VariantMapList &dataList)
{
    INVOKE2(setSelectedItemsData, (dataList));

    const QList<QPersistentModelIndex> selected = selectedIndexes();
    ClipboardBrowser *c = nullptr;
    QMap<QPersistentModelIndex, QVariantMap> itemsData;
    const auto count = std::min( selected.size(), dataList.items.size() );
    for ( int i = 0; i < count; ++i ) {
        const auto &index = selected[i];
        if ( !index.isValid() )
            continue;

        if (c == nullptr)
            c = m_wnd->browserForItem(index);

        itemsData[index] = dataList.items[i];
    }

    if (c != nullptr)
        c->setItemsData(itemsData);
}

int ScriptableProxy::createSelection(const QString &tabName)
{
    INVOKE(createSelection, (tabName));
    const int newSelectionId = ++m_lastSelectionId;
    ClipboardBrowser *c = fetchBrowser(tabName);
    if (c)
        m_selections[newSelectionId] = {c, {}};
    return newSelectionId;
}

int ScriptableProxy::selectionCopy(int id)
{
    INVOKE(selectionCopy, (id));
    const int newSelectionId = ++m_lastSelectionId;
    auto selection = m_selections.value(id);
    if (selection.browser)
        m_selections[newSelectionId] = selection;
    return newSelectionId;
}

void ScriptableProxy::destroySelection(int id)
{
    INVOKE2(destroySelection, (id));
    m_selections.remove(id);
}

void ScriptableProxy::selectionRemoveAll(int id)
{
    INVOKE2(selectionRemoveAll, (id));
    auto selection = m_selections.take(id);
    if (!selection.browser)
        return;
    selectionRemoveInvalid(&selection.indexes);

    QModelIndexList indexes;
    for (const auto &index : selection.indexes)
        indexes.append(index);

    selection.browser->removeIndexes(indexes);

    selectionRemoveInvalid(&selection.indexes);
    m_selections[id] = selection;
}

void ScriptableProxy::selectionSelectRemovable(int id)
{
    INVOKE2(selectionSelectRemovable, (id));
    auto selection = m_selections.take(id);
    if (!selection.browser)
        return;

    // Use error argument for canRemoveItems() to ensure that a message dialog is not shown.
    QString error;
    QList<QPersistentModelIndex> indexes;
    for (int row = 0; row < selection.browser->length(); ++row) {
        const auto index = selection.browser->index(row);
        if ( !selection.indexes.contains(index) && selection.browser->canRemoveItems({index}, &error) )
            indexes.append(index);
    }
    selection.indexes.append(indexes);
    m_selections[id] = selection;
}

void ScriptableProxy::selectionInvert(int id)
{
    INVOKE2(selectionInvert, (id));
    auto selection = m_selections.take(id);
    if (!selection.browser)
        return;

    QList<QPersistentModelIndex> indexes;
    for (int row = 0; row < selection.browser->length(); ++row) {
        const auto index = selection.browser->index(row);
        if ( !selection.indexes.contains(index) )
            indexes.append(index);
    }
    selection.indexes = indexes;
    m_selections[id] = selection;
}

void ScriptableProxy::selectionSelectAll(int id)
{
    INVOKE2(selectionSelectAll, (id));
    auto selection = m_selections.take(id);
    if (!selection.browser)
        return;

    selection.indexes.clear();
    for (int row = 0; row < selection.browser->length(); ++row)
        selection.indexes.append(selection.browser->index(row));
    m_selections[id] = selection;
}

void ScriptableProxy::selectionSelect(int id, const QVariant &maybeRe, const QString &mimeFormat)
{
    INVOKE2(selectionSelect, (id, maybeRe, mimeFormat));
    auto selection = m_selections.take(id);
    if (!selection.browser)
        return;

    const QRegularExpression re = maybeRe.toRegularExpression();
    QList<QPersistentModelIndex> indexes;
    for (int row = 0; row < selection.browser->length(); ++row) {
        const auto index = selection.browser->index(row);
        if ( selection.indexes.contains(index) )
            continue;

        const QVariantMap dataMap = index.data(contentType::data).toMap();
        if ( mimeFormat.isEmpty() ) {
            if ( !maybeRe.isValid() )
                continue;
            const QString text = getTextData(dataMap);
            if ( text.contains(re) )
                indexes.append(index);
        } else if ( dataMap.contains(mimeFormat) == maybeRe.isValid() ) {
            const QString text = getTextData(dataMap, mimeFormat);
            if ( text.contains(re) )
                indexes.append(index);
        }
    }
    selection.indexes.append(indexes);
    m_selections[id] = selection;
}

void ScriptableProxy::selectionDeselectIndexes(int id, const QVector<int> &indexes)
{
    INVOKE2(selectionDeselectIndexes, (id, indexes));

    auto selection = m_selections.take(id);
    auto indexesSorted = indexes;
    std::sort(indexesSorted.begin(), indexesSorted.end(), std::greater<int>());
    for (int index : indexesSorted)
        selection.indexes.removeAt(index);
    m_selections[id] = selection;
}

void ScriptableProxy::selectionDeselectSelection(int id, int toDeselectId)
{
    INVOKE2(selectionDeselectSelection, (id, toDeselectId));
    auto selection = m_selections.take(id);
    const auto deselection = m_selections.value(toDeselectId);

    selectionRemoveIf(
        &selection.indexes,
        [&](const QPersistentModelIndex &index){
            return !index.isValid() || deselection.indexes.contains(index);
        });
    m_selections[id] = selection;
}

void ScriptableProxy::selectionGetCurrent(int id)
{
    INVOKE2(selectionGetCurrent, (id));

    QList<QPersistentModelIndex> selected = selectedIndexes();
    ClipboardBrowser *c = browserForIndexes(selected);
    if (c == nullptr) {
        const QString currentTab = m_actionData.value(mimeCurrentTab).toString();
        c = fetchBrowser(currentTab);
    }
    m_selections[id] = {c, selected};
}

int ScriptableProxy::selectionGetSize(int id)
{
    INVOKE(selectionGetSize, (id));
    return m_selections.value(id).indexes.size();
}

QString ScriptableProxy::selectionGetTabName(int id)
{
    INVOKE(selectionGetTabName, (id));
    const auto selection = m_selections.value(id);
    return selection.browser ? selection.browser->tabName() : QString();
}

QVector<int> ScriptableProxy::selectionGetRows(int id)
{
    INVOKE(selectionGetRows, (id));

    auto selection = m_selections.value(id);
    QVector<int> rows;
    rows.reserve(selection.indexes.size());
    for (const auto &index : selection.indexes)
        rows.append(index.row());
    return rows;
}

QVariantMap ScriptableProxy::selectionGetItemIndex(int id, int index)
{
    INVOKE(selectionGetItemIndex, (id, index));

    auto selection = m_selections.value(id);
    if ( selection.indexes.isEmpty() || index < 0 || index >= selection.indexes.size() )
        return {};

    return selection.indexes[index].data(contentType::data).toMap();
}

void ScriptableProxy::selectionSetItemIndex(int id, int index, const QVariantMap &item)
{
    INVOKE2(selectionSetItemIndex, (id, index, item));

    const auto selection = m_selections.value(id);
    if ( !selection.browser || index < 0 || index >= selection.indexes.size() )
        return;

    const QModelIndex ind = selection.indexes[index];
    selection.browser->model()->setData(ind, item, contentType::data);
}

QVariantList ScriptableProxy::selectionGetItemsData(int id)
{
    INVOKE(selectionGetItemsData, (id));

    QVariantList dataList;
    const auto selection = m_selections.value(id);
    for (const auto &index : selection.indexes) {
        const auto data = index.data(contentType::data).toMap();
        dataList.append(data);
    }
    return dataList;
}

void ScriptableProxy::selectionSetItemsData(int id, const QVariantList &dataList)
{
    INVOKE2(selectionSetItemsData, (id, dataList));

    QMap<QPersistentModelIndex, QVariantMap> itemsData;
    const auto selection = m_selections.value(id);
    const auto count = std::min( selection.indexes.size(), dataList.size() );
    for ( int i = 0; i < count; ++i ) {
        const auto &index = selection.indexes[i];
        if ( index.isValid() )
            itemsData[index] = dataList[i].toMap();
    }

    selection.browser->setItemsData(itemsData);
}

QVariantList ScriptableProxy::selectionGetItemsFormat(int id, const QString &format)
{
    INVOKE(selectionGetItemsFormat, (id, format));

    QVariantList dataList;
    const auto selection = m_selections.value(id);
    for (const auto &index : selection.indexes) {
        const auto data = index.data(contentType::data).toMap();
        dataList.append( data.value(format) );
    }
    return dataList;
}

void ScriptableProxy::selectionSetItemsFormat(int id, const QString &mime, const QVariant &value)
{
    INVOKE2(selectionSetItemsFormat, (id, mime, value));

    const auto selection = m_selections.value(id);
    setItemsData(selection.browser, selection.indexes, mime, value);
}

void ScriptableProxy::selectionMove(int id, int row)
{
    INVOKE2(selectionMove, (id, row));
    auto selection = m_selections.value(id);
    if (!selection.browser)
        return;

    QModelIndexList indexes;
    indexes.reserve(selection.indexes.size());
    for (const auto &index : selection.indexes) {
        if (index.isValid())
            indexes.append(index);
    }

    if ( !indexes.isEmpty() )
        selection.browser->move(indexes, row);
}

void ScriptableProxy::selectionSort(int id, const QVector<int> &indexes)
{
    INVOKE2(selectionSort, (id, indexes));

    auto selection = m_selections.value(id);

    QList<QPersistentModelIndex> sorted;
    sorted.reserve( indexes.size() );
    for (const int i : indexes) {
        if (i < 0 || i >= selection.indexes.size())
            continue;

        const auto index = selection.indexes[i];
        if ( index.isValid() )
            sorted.append(index);
    }
    selection.indexes = sorted;

    if ( !sorted.isEmpty() )
        selection.browser->sortItems(sorted);
}

#ifdef HAS_TESTS
void ScriptableProxy::sendKeys(const QString &expectedWidgetName, const QString &keys, int delay)
{
    INVOKE2(sendKeys, (expectedWidgetName, keys, delay));
    Q_ASSERT( keyClicker()->succeeded() || keyClicker()->failed() );
    keyClicker()->sendKeyClicks(expectedWidgetName, keys, delay, 10);
}

bool ScriptableProxy::sendKeysSucceeded()
{
    INVOKE(sendKeysSucceeded, ());
    return keyClicker()->succeeded();
}

bool ScriptableProxy::sendKeysFailed()
{
    INVOKE(sendKeysFailed, ());
    return keyClicker()->failed();
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
    std::sort( selectedRows.begin(), selectedRows.end() );

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
    PlatformWindowPtr window = platformNativeInterface()->getCurrentWindow();
    return window ? window->getTitle() : QString();
}

int ScriptableProxy::inputDialog(const NamedValueList &values)
{
    INVOKE(inputDialog, (values));

    InputDialog inputDialog;
    inputDialog.dialog = new QDialog(m_wnd);
    QDialog &dialog = *inputDialog.dialog;

    QString dialogTitle;
    QIcon icon;
    QVBoxLayout layout(&dialog);
    QWidgetList widgets;
    widgets.reserve(values.items.size());

    QString styleSheet;
    QRect geometry(-1, -1, 0, 0);

    for (const auto &value : values.items) {
        if (value.name == ".title")
            dialogTitle = value.value.toString();
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
        else if (value.name == ".modal")
            dialog.setModal(value.value.toBool());
        else if (value.name == ".onTop")
            dialog.setWindowFlag(Qt::WindowStaysOnTopHint, value.value.toBool());
        else if (value.name == ".noParent")
            dialog.setParent(value.value.toBool() ? nullptr : m_wnd);
        else if (value.name == ".popupWindow")
            dialog.setWindowFlag(Qt::Popup, value.value.toBool());
        else if (value.name == ".toolWindow")
            dialog.setWindowFlag(Qt::Tool, value.value.toBool());
        else if (value.name == ".sheetWindow")
            dialog.setWindowFlag(Qt::Sheet, value.value.toBool());
        else if (value.name == ".foreignWindow")
            dialog.setWindowFlag(Qt::ForeignWindow, value.value.toBool());
        else
            widgets.append( createWidget(value.name, value.value, &inputDialog) );
    }

    // WORKAROUND for broken initial focus in Qt 6.6 (QTBUG-121514)
    if (!widgets.isEmpty())
        widgets.first()->setFocus();

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

    auto buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    QObject::connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
    QObject::connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
    layout.addWidget(buttons);

    installShortcutToCloseDialog(&dialog, &dialog, Qt::ControlModifier | Qt::Key_Enter);
    installShortcutToCloseDialog(&dialog, &dialog, Qt::ControlModifier | Qt::Key_Return);

    if (icon.isNull())
        icon = appIcon();
    dialog.setWindowIcon(icon);

    const int dialogId = ++m_lastInputDialogId;
    connect(&dialog, &QDialog::finished, this, [this, dialogId, inputDialog, widgets]() {
        if (inputDialog.dialog == nullptr)
            return;

        NamedValueList result;
        result.items.reserve( widgets.size() );

        if ( inputDialog.dialog->result() ) {
            for ( auto w : widgets ) {
                const QString propertyName = w->property(propertyWidgetProperty).toString();
                const QString name = w->property(propertyWidgetName).toString();
                const QVariant value = w->property(propertyName.toUtf8().constData());
                result.items.append( NamedValue(name, value) );
            }
            if ( widgets.isEmpty() )
                result.items.append( NamedValue(QString(), true) );
        }

        QByteArray bytes;
        {
            QDataStream stream(&bytes, QIODevice::WriteOnly);
            stream << dialogId << result;
        }

        inputDialog.dialog->deleteLater();
        emit sendMessage(bytes, CommandInputDialogFinished);
    });

    // Connecting this directly to QEventLoop::quit() doesn't seem to work always.
    connect(this, &ScriptableProxy::abortEvaluation, &dialog, &QDialog::reject);

    if ( !dialogTitle.isNull() ) {
        dialog.setWindowTitle(dialogTitle);
        dialog.setObjectName(QStringLiteral("dialog_") + dialogTitle);
        WindowGeometryGuard::create(&dialog);
    }

    dialog.show();

    raiseWindow(&dialog);

    return dialogId;
}

void ScriptableProxy::setSelectedItemsData(const QString &mime, const QVariant &value)
{
    INVOKE2(setSelectedItemsData, (mime, value));

    QList<QPersistentModelIndex> selected = selectedIndexes();
    selectionRemoveInvalid(&selected);
    ClipboardBrowser *c = browserForIndexes(selected);
    if (c == nullptr)
        return;

    setItemsData(c, selected, mime, value);
}

void ScriptableProxy::filter(const QString &text)
{
    INVOKE2(filter, (text));
    m_wnd->setFilter(text);
}

QString ScriptableProxy::filter()
{
    INVOKE(filter, ());
    return m_wnd->filter();
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
        const int screenNumber = ::screenNumberAt(mousePosition);
        if (screenNumber != -1)
            selectedScreen = QGuiApplication::screens().value(screenNumber);
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
            const auto ratio = pixelRatio(&pixmap);
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

KeyboardModifierList ScriptableProxy::queryKeyboardModifiers()
{
    INVOKE(queryKeyboardModifiers, ());
    return {QApplication::queryKeyboardModifiers()};
}

QPoint ScriptableProxy::pointerPosition()
{
    INVOKE(pointerPosition, ());
    return QCursor::pos();
}

void ScriptableProxy::setPointerPosition(int x, int y)
{
    INVOKE2(setPointerPosition, (x, y));
    const QPoint pos(x, y);
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
    const auto screens = QApplication::screens();
    const auto found = std::find_if(
        std::begin(screens), std::end(screens),
        [pos](QScreen *screen) { return screen->geometry().contains(pos); });
    if (found == std::end(screens))
        return;
    QScreen *screen = *found;
#else
    QScreen *screen = QGuiApplication::screenAt(pos);
#endif

    if (screen)
        QCursor::setPos(screen, pos);
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

    if (title.isEmpty()) {
        const QString defaultTitle = isMonitoringEnabled()
            ? QString()
            : tr("*Clipboard Storing Disabled*", "Main window title if clipboard storing is disabled");
        m_wnd->setWindowTitle(defaultTitle);
        m_wnd->setTrayTooltip(defaultTitle);
    } else {
        m_wnd->setWindowTitle(title);
        m_wnd->setTrayTooltip(title);
    }
}

void ScriptableProxy::setTitleForData(const QVariantMap &data)
{
    INVOKE2(setTitleForData, (data));

    const QString clipboardContent = textLabelForData(data);
    setTitle(clipboardContent);
}

void ScriptableProxy::saveData(const QString &tab, const QVariantMap &data, ClipboardMode mode)
{
    INVOKE2(saveData, (tab, data, mode));

    auto c = m_wnd->tab(tab);
    if (c)
        c->addUnique(data, mode);
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
    notification->setIcon(IconPaste);
    notification->setInterval(intervalSeconds * 1000);

    const int maximumWidthPoints = appConfig.option<Config::notification_maximum_width>();
    const int width = pointsToPixels(maximumWidthPoints) - 16 - 8;

    const QStringList formats = data.keys();
    const int imageIndex = formats.indexOf(QRegularExpression("^image/.*"));
    const QFont &font = notification->widget()
        ? notification->widget()->font()
        : qApp->font();
    const bool isHidden = data.contains(mimeHidden);

    QString title;

    if (data.isEmpty()) {
        notification->setInterval(0);
    } if ( !isHidden && data.contains(mimeText) ) {
        QString text = getTextData(data);
        const int n = text.count('\n') + 1;

        if (n > 1) {
            title = QObject::tr("Text Copied (%n lines)",
                                 "Notification title for multi-line text in clipboard", n);
        } else {
            title = QObject::tr("Text Copied", "Notification title for single-line text in clipboard");
        }

        text = elideText(text, font, QString(), false, width, maxLines);
        notification->setMessage(text);
    } else if (!isHidden && imageIndex != -1) {
        QPixmap pix;
        const QString &imageFormat = formats[imageIndex];
        pix.loadFromData( data[imageFormat].toByteArray(), imageFormat.toLatin1() );

        const int height = maxLines * QFontMetrics(font).lineSpacing();
        if (pix.width() > width || pix.height() > height)
            pix = pix.scaled(QSize(width, height), Qt::KeepAspectRatio);

        notification->setPixmap(pix);
    } else {
        title = QObject::tr("Data Copied", "Notification title for a copied data in clipboard");
        const QString text = textLabelForData(data, font, QString(), false, width, maxLines);
        notification->setMessage(text);
    }

    notification->setTitle(title);
}

bool ScriptableProxy::enableMenuItem(int actionId, int currentRun, int menuItemMatchCommandIndex, const QVariantMap &menuItem)
{
    INVOKE(enableMenuItem, (actionId, currentRun, menuItemMatchCommandIndex, menuItem));
    return m_wnd->setMenuItemEnabled(actionId, currentRun, menuItemMatchCommandIndex, menuItem);
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

QString ScriptableProxy::loadTheme(const QString &path)
{
    INVOKE(loadTheme, (path));

    {
        const QFileInfo fileInfo(path);
        if ( !fileInfo.isFile() || !fileInfo.isReadable() )
            return "Failed to read theme";
    }

    const QSettings settings(path, QSettings::IniFormat);
    if ( settings.status() != QSettings::NoError )
        return "Failed to load theme";

    m_wnd->loadTheme(settings);
    if ( settings.status() != QSettings::NoError )
        return "Failed to parse theme";

    return QString();
}

QByteArray ScriptableProxy::getClipboardData(const QString &mime, ClipboardMode mode)
{
    INVOKE(getClipboardData, (mime, mode));

    const QMimeData *data = m_wnd->getClipboardData(mode);
    if (!data)
        return QByteArray();

    if (mime == "?")
        return data->formats().join("\n").toUtf8() + '\n';

    return cloneData(data, QStringList(mime)).value(mime).toByteArray();
}

bool ScriptableProxy::hasClipboardFormat(const QString &mime, ClipboardMode mode)
{
    INVOKE(hasClipboardFormat, (mime, mode));

    const QMimeData *data = m_wnd->getClipboardData(mode);
    return data && data->hasFormat(mime);
}

QStringList ScriptableProxy::styles()
{
    INVOKE(styles, ());
    return QStyleFactory::keys();
}

void ScriptableProxy::setScriptOverrides(const QVector<int> &overrides)
{
    INVOKE2(setScriptOverrides, (overrides));
    m_wnd->setScriptOverrides(overrides, m_actionId);
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

QVariantMap ScriptableProxy::itemData(const QString &tabName, int i)
{
    auto c = fetchBrowser(tabName);
    return c ? c->copyIndex( c->index(i) ) : QVariantMap();
}

QByteArray ScriptableProxy::itemData(const QString &tabName, int i, const QString &mime)
{
    const QVariantMap data = itemData(tabName, i);
    if ( data.isEmpty() )
        return QByteArray();

    if (mime == "?")
        return QStringList(data.keys()).join("\n").toUtf8() + '\n';

    if (mime == mimeItems)
        return serializeData(data);

    return data.value(mime).toByteArray();
}

void ScriptableProxy::setItemsData(
    ClipboardBrowser *c, const QList<QPersistentModelIndex> &indexes, const QString &mime, const QVariant &value)
{
    QMap<QPersistentModelIndex, QVariantMap> itemsData;

    for (const auto &index : indexes) {
        if ( !index.isValid() )
            continue;

        QVariantMap data = index.data(contentType::data).toMap();
        if (value.isValid())
            data[mime] = value;
        else
            data.remove(mime);
        itemsData[index] = data;
    }

    c->setItemsData(itemsData);
}

template<typename T>
T ScriptableProxy::getSelectionData(const QString &mime)
{
    QVariant value = m_actionData.value(mime);
    if ( !value.isValid() && !m_actionData.contains(mimeCurrentTab) && getSelectionData() )
        value = m_actionData.value(mime);
    return value.value<T>();
}

QPersistentModelIndex ScriptableProxy::currentIndex()
{
    return getSelectionData<QPersistentModelIndex>(mimeCurrentItem);
}

QList<QPersistentModelIndex> ScriptableProxy::selectedIndexes()
{
    return getSelectionData<QList<QPersistentModelIndex>>(mimeSelectedItems);
}

ClipboardBrowser *ScriptableProxy::browserForIndexes(const QList<QPersistentModelIndex> &indexes) const
{
    for (const auto &index : indexes) {
        if ( index.isValid() )
            return m_wnd->browserForItem(index);
    }
    return nullptr;
}

QVariant ScriptableProxy::waitForFunctionCallFinished(int functionCallId)
{
    if (m_disconnected)
        return QVariant();

    QVariant result;

    QEventLoop loop;
    connect(this, &ScriptableProxy::functionCallFinished, &loop,
            [&](int receivedFunctionCallId, const QVariant &returnValue) {
                if (receivedFunctionCallId != functionCallId)
                    return;
                result = returnValue;
                loop.quit();
            });
    connect(this, &ScriptableProxy::abortEvaluation, &loop, &QEventLoop::quit);

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &loop, &QEventLoop::quit);
    loop.exec();

    return result;
}

bool ScriptableProxy::getSelectionData()
{
    auto c = m_wnd->browser();
    if (c == nullptr)
        return false;

    const QVariantMap data = selectionData(*c);
    for (auto it = data.constBegin(); it != data.constEnd(); ++it)
        m_actionData[it.key()] = it.value();
    return true;
}

#ifdef HAS_TESTS
KeyClicker *ScriptableProxy::keyClicker()
{
    if (!m_keyClicker)
        m_keyClicker = new KeyClicker(m_wnd, this);
    return m_keyClicker;
}
#endif // HAS_TESTS

QString pluginsPath()
{
    QDir dir;
    if (platformNativeInterface()->findPluginDir(&dir))
        return dir.absolutePath();

    return QString();
}

QString themesPath()
{
    return platformNativeInterface()->themePrefix();
}

QString translationsPath()
{
    return platformNativeInterface()->translationPrefix();
}

void setClipboardMonitorRunning(bool running)
{
    QSettings settings(
          QSettings::IniFormat,
          QSettings::UserScope,
          QCoreApplication::organizationName(),
          QCoreApplication::applicationName() + "-monitor");
    settings.setValue(QStringLiteral("running"), running);
}
bool isClipboardMonitorRunning()
{
    const QSettings settings(
          QSettings::IniFormat,
          QSettings::UserScope,
          QCoreApplication::organizationName(),
          QCoreApplication::applicationName() + "-monitor");
    return settings.value(QStringLiteral("running")).toBool();
}
