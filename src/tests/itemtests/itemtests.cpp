// SPDX-License-Identifier: GPL-3.0-or-later

#include "itemtests.h"

#include <QCheckBox>
#include <QDrag>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTest>
#include <QTimer>

namespace {

Q_DECLARE_LOGGING_CATEGORY(plugin)
Q_LOGGING_CATEGORY(plugin, "copyq.keys")

QString widgetAddress(QWidget *widget)
{
    if (!widget)
        return QStringLiteral("<null>");

    QString result;
    QWidget *current = widget;
    while (current) {
        const QString className = current->metaObject()->className();
        const QString objectName = current->objectName();
        const QString name = className == objectName
            ? className : QStringLiteral("%1:%2").arg(objectName, className);
        // Skip some default widgets.
        if ( name != QLatin1String(":QWidget")
          && !name.startsWith(QLatin1String("qt_scrollarea_viewport:QWidget")) )
        {
            if (!result.isEmpty())
                result.append('<');
            result.append(name);
            QString text = current->property("text").toString()
                .remove('&')
                // Remove HTML tags
                .remove(QRegularExpression(QStringLiteral("</?[^>]*>")));
            if ( text.isEmpty() && current->isWindow() )
                text = current->windowTitle();
            if ( !text.isEmpty() )
                result.append(QStringLiteral("'%1'").arg(text));
        }
        current = current->parentWidget();
    }
    return result;
}

bool matchesProperties(QObject *object, const QStringList &properties)
{
    for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
        const QString key = it->section('=', 0, 0);
        const QString value = it->section('=', 1, 1);
        if ( value.isEmpty() ) {
            if ( object->objectName() != key && object->metaObject()->className() != key )
                return false;
        }

        const QVariant propValue = object->property(key.toUtf8());
        if ( propValue.toString() != value )
            return false;
    }

    return true;
}

QWidget *findWidgetWithProperties(const QString &properties, QWidget *parent)
{
    QStringList names = properties.split('<');
    return std::accumulate(
        names.cbegin(), names.cend(), parent,
        [](QWidget *parent, const QString &name) -> QWidget* {
            if (parent == nullptr)
                return nullptr;

            const QStringList props = name.split('|', Qt::SkipEmptyParts);
            for (QWidget *child : parent->findChildren<QWidget*>()) {
                if (child->isVisible() && matchesProperties(child, props)) {
                    qCDebug(plugin) << "Found target:" << widgetAddress(child);
                    return child;
                }
            }
            qCCritical(plugin) << "Failed to find mouse action target:" << name;
            return nullptr;
        });
}

bool checkEventTarget(
    QWidget *target, const QString &keys, const QString &widgetName, const char *event)
{
    if (target && target->isVisible())
        return true;

    qCCritical(plugin) << "Failed to send postponed" << event
        << keys << "to" << widgetName << ";"
        << (target ? "Target widget is no longer visible" : "Target widget no longer exists");
    return false;
}

} // namespace

class KeyClicker final : public QObject {
public:
    enum Status {
        Pending,
        Success,
        Failed
    };

    KeyClicker(QObject *parent)
        : QObject(parent)
    {
        for (const auto w : qApp->topLevelWidgets()) {
            if (w->objectName() == QLatin1String("MainWindow")) {
                m_wnd = w;
                break;
            }
        }
        if (m_wnd == nullptr)
            qCCritical(plugin) << "Failed to find MainWindow";
    }

    void keyClicksRetry(const QRegularExpression &expectedWidgetName, const QString &keys, int delay, int retry)
    {
        if (retry > 0)
            sendKeyClicks(expectedWidgetName, keys, delay + 100, retry - 1);
        else
            keyClicksFailed();
    }

    void keyClicksFailed()
    {
        qCCritical(plugin) << "Failed to send key press to target widget";

        const auto actual = keyClicksTarget();
        const auto popup = QApplication::activePopupWidget();
        const auto widget = QApplication::focusWidget();
        const auto window = QApplication::activeWindow();
        const auto modal = QApplication::activeModalWidget();
        const auto state = qApp->applicationState();

        if (state == Qt::ApplicationActive)
            qCCritical(plugin) << "App is INACTIVE! State:" << state;

        qCCritical(plugin).noquote().nospace()
            << "Expected: /" + m_expectedWidgetName.pattern() + "/"
            << "\nActual:   " + widgetAddress(actual)
            << "\nPopup:    " + widgetAddress(popup)
            << "\nWidget:   " + widgetAddress(widget)
            << "\nWindow:   " + widgetAddress(window)
            << "\nModal:    " + widgetAddress(modal);

        m_status = Failed;
    }

    void keyClicks(const QString &keys, int delay, int retry)
    {
        auto widget = keyClicksTarget();
        if (!widget) {
            keyClicksRetry(m_expectedWidgetName, keys, delay, retry);
            return;
        }

#if defined(Q_OS_MAC)
        // WORKAROUND for focusing back to the main window on macOS.
        if (qApp->applicationState() != Qt::ApplicationActive && m_wnd->isVisible()) {
            qCDebug(plugin) << "Re-activating the main window (macOS)";
            m_wnd->activateWindow();
            m_wnd->raise();
        }
#endif

        if (qApp->applicationState() != Qt::ApplicationActive) {
            qCDebug(plugin) << "Waiting for application to become active";
            keyClicksRetry(m_expectedWidgetName, keys, delay, retry);
            return;
        }

        const QString widgetName = widgetAddress(widget);
        if ( !m_expectedWidgetName.pattern().isEmpty()
             && !m_expectedWidgetName.match(widgetName).hasMatch() )
        {
            keyClicksRetry(m_expectedWidgetName, keys, delay, retry);
            return;
        }

        // Only verified focused widget.
        if ( keys.isEmpty() ) {
            m_status = Success;
            return;
        }

        // There could be some animation/transition effect on check boxes
        // so wait for checkbox to be set.
        if ( qobject_cast<QCheckBox*>(widget) )
            QTest::qWait(100);

        qCDebug(plugin) << "Sending" << keys << "to" << widgetName;

        static const auto keyClicksPrefix = QLatin1String(":");
        static const auto mousePrefix = QLatin1String("mouse|");
        static const auto dragPrefix = QLatin1String("isDraggingFrom|");
        if ( keys.startsWith(keyClicksPrefix) ) {
            const auto text = keys.mid(keyClicksPrefix.size());
            QTest::keyClicks(widget, text, Qt::NoModifier);
        } else if ( keys.startsWith(mousePrefix) ) {
            const QString action = keys.section('|', 1, 1);
            const QString properties = keys.section('|', 2);
            static const auto mousePress = QStringLiteral("PRESS");
            static const auto mouseRelease = QStringLiteral("RELEASE");
            static const auto mouseClick = QStringLiteral("CLICK");
            static const auto mouseDrag = QStringLiteral("DRAG");
            static const QStringList validActions = {
                mousePress, mouseRelease, mouseClick, mouseDrag};
            if ( !validActions.contains(action) ) {
                qCCritical(plugin) << "Failed to match mouse action:" << keys;
                qCCritical(plugin) << "Valid mouse actions are:" << validActions;
                m_status = Failed;
                return;
            }
            QPointer<QWidget> source = findWidgetWithProperties(properties, m_wnd);
            if (!source) {
                m_status = Failed;
                return;
            }
            // Don't block while processing the events.
            runAfterInterval(delay, [=](){
                if (!checkEventTarget(source, keys, widgetName, "mouse"))
                    return;

                // Send the event to window instead - it works better.
                QWidget *windowWidget = source->window();
                if (!windowWidget || !source->window()->windowHandle()) {
                    qCCritical(plugin) << "Target widget has no window handle";
                    return;
                }
                QWindow *window = windowWidget->windowHandle();
                const QPoint pos = source->mapTo(windowWidget, source->rect().center());

                if (action == mousePress) {
                    QTest::mousePress(window, Qt::LeftButton, {}, pos);
                } else if (action == mouseRelease) {
                    QTest::mouseRelease(window, Qt::LeftButton, {}, pos);
                } else if (action == mouseClick) {
                    QTest::mouseClick(window, Qt::LeftButton, {}, pos);
                } else if (action == mouseDrag) {
                    // This should trigger creating a QDrag in a parent widget.
                    QTest::mouseMove(source, source->rect().topLeft());
                    QTest::mouseMove(source, source->rect().bottomRight());
                }
            });
        } else if ( keys.startsWith(dragPrefix) ) {
            const QObject *drag = m_wnd->findChild<QDrag*>();
            if (!drag) {
                qCCritical(plugin) << "QDrag not started";
                m_status = Failed;
                return;
            }
            qCDebug(plugin) << "QDrag started with parent:" << widgetAddress(qobject_cast<QWidget*>(drag->parent()));
            const QString properties = keys.section('|', 1);
            QWidget* source = findWidgetWithProperties(properties, m_wnd);
            if (!source) {
                m_status = Failed;
                return;
            }
            if (drag->parent() != source) {
                qCCritical(plugin) << "Unexpected QDrag parent; Expected:" << properties
                    << "; Actual:" << widgetAddress(qobject_cast<QWidget*>(drag->parent()));
                m_status = Failed;
                return;
            }
        } else {
            const QKeySequence shortcut(keys, QKeySequence::PortableText);
            if ( shortcut.isEmpty() ) {
                qCCritical(plugin) << "Failed to parse shortcut" << keys;
                m_status = Failed;
                return;
            }

            const auto key = static_cast<uint>(shortcut[0]);
            const QPointer<QWidget> target = widget;
            // Avoid blocking on modal dialogs
            runAfterInterval(0, [=](){
                if (!target || !target->isVisible()) {
                    qCCritical(plugin) << "Target no longer valid:" << widgetName;
                    return;
                }
                QTest::keyClick(
                    target,
                    Qt::Key(key & ~Qt::KeyboardModifierMask),
                    Qt::KeyboardModifiers(key & Qt::KeyboardModifierMask));
            });
        }

        m_status = Success;
        qCDebug(plugin) << "Sent" << keys << "to" << widgetName;
    }

    void sendKeyClicks(const QRegularExpression &expectedWidgetName, const QString &keys, int delay, int retry)
    {
        m_status = Pending;
        m_expectedWidgetName = expectedWidgetName;

        // Don't stop when modal window is open.
        runAfterInterval(delay, [=](){ keyClicks(keys, delay, retry); });
    }

    int status(bool forceRetrieve) {
        if (m_status == Pending && forceRetrieve) {
            keyClicksFailed();
            return Failed;
        }
        return m_status;
    }

private:
    template <typename Callable>
    void runAfterInterval(int delay, Callable func)
    {
        auto t = new QTimer(m_wnd);
        t->setSingleShot(true);
        QObject::connect(t, &QTimer::timeout, m_wnd, func);
        QObject::connect(t, &QTimer::timeout, t, &QTimer::deleteLater);
        t->start(delay);
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

    QWidget *m_wnd = nullptr;
    Status m_status = Success;
    QRegularExpression m_expectedWidgetName;
};

void ItemTestsScriptable::keys()
{
    bool ok;

    // Wait interval after shortcut pressed or text typed.
    const auto waitValue = qgetenv("COPYQ_TESTS_KEYS_WAIT");
    int wait = waitValue.toInt(&ok);
    if (!ok)
        wait = 0;

    // Delay while typing.
    const auto delayValue = qgetenv("COPYQ_TESTS_KEY_DELAY");
    int delay = delayValue.toInt(&ok);
    if (!ok)
        delay = 0;

    QString expectedWidgetName;

    const auto focusPrefix = QLatin1String("focus:");
    bool interrupted = false;
    for (const auto &arg : currentArguments()) {
        if (interrupted) {
            const auto message =
                "Client was interrupted."
                " This is allowed to happen only when processing the last key event.";
            qCCritical(plugin) << message;
            throwError(message);
            return;
        }

        const QString keys = arg.toString();

        if (keys.startsWith(focusPrefix)) {
            expectedWidgetName = keys.mid(focusPrefix.size());
            call("callPlugin", {"itemtests", "sendKeys", expectedWidgetName, QString(), 0});
        } else {
            call("sleep", {wait});
            call("callPlugin", {"itemtests", "sendKeys", expectedWidgetName, keys, delay});
            QTest::qWait(qMax(5, delay));
        }

        // Make sure all keys are send (shortcuts are postponed because they can be blocked by modal windows).
        for (int i = 1; ; ++i) {
            const QVariant result = call("callPlugin", {"itemtests", "sendKeysStatus", i > 15});
            bool ok = false;
            const int status = result.toInt(&ok);
            if (!ok) {
                qCDebug(plugin) << "Client interrupted, got status:" << result;
                interrupted = true;
                break;
            }

            if (status == KeyClicker::Success)
                break;

            if (status == KeyClicker::Failed) {
                throwError("Failed to send key presses");
                return;
            }

            const int waitMs = 8 * i * i;
            QTest::qWait(qMin(1000, waitMs));
        }
    }
}

ItemScriptable *ItemTestsLoader::scriptableObject()
{
    return new ItemTestsScriptable();
}

QVariant ItemTestsLoader::scriptCallback(const QVariantList &arguments)
{
    const auto cmd = arguments[0].toString();

    if (cmd == "sendKeys") {
        const QString expectedWidgetName = arguments.value(1).toString();
        const QString keys = arguments.value(2).toString();
        const int delay = arguments.value(3).toInt();
        const QRegularExpression re = QRegularExpression(
            QString(expectedWidgetName)
            .replace(QLatin1String("<"), QLatin1String(".*<.*"))
        );
        keyClicker()->sendKeyClicks(re, keys, delay, 10);
        return {};
    }

    if (cmd == "sendKeysStatus")
        return keyClicker()->status(arguments.value(1).toBool());

    return QStringLiteral("Unexpected command: %1").arg(cmd);
}

KeyClicker *ItemTestsLoader::keyClicker()
{
    if (!m_keyClicker)
        m_keyClicker = new KeyClicker(this);
    return m_keyClicker;
}
