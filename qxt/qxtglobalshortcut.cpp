#include "qxtglobalshortcut.h"
/****************************************************************************
** Copyright (c) 2006 - 2011, the LibQxt project.
** See the Qxt AUTHORS file for a list of authors and copyright holders.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the LibQxt project nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
** <http://libqxt.org>  <foundation@libqxt.org>
*****************************************************************************/

#include "qxtglobalshortcut_p.h"

#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QMessageBox>

namespace {

int referenceCounter = 0;

QHash<QPair<quint32, quint32>, QxtGlobalShortcut*> shortcuts;

} // namespace

QxtGlobalShortcutPrivate::QxtGlobalShortcutPrivate(QxtGlobalShortcut *q)
    : enabled(true)
    , key(Qt::Key(0))
    , mods(Qt::NoModifier)
    , nativeKey(0)
    , nativeMods(0)
    , registered(false)
    , q_ptr(q)
{
    init();
}

QxtGlobalShortcutPrivate::~QxtGlobalShortcutPrivate()
{
    unsetShortcut();
    destroy();
}

void QxtGlobalShortcutPrivate::initFallback()
{
    if (referenceCounter == 0) {
        QAbstractEventDispatcher::instance()->installNativeEventFilter(this);
    }
    ++referenceCounter;
}

void QxtGlobalShortcutPrivate::destroyFallback()
{
    --referenceCounter;
    if (referenceCounter == 0) {
        QAbstractEventDispatcher *ed = QAbstractEventDispatcher::instance();
        if (ed != nullptr) {
            ed->removeNativeEventFilter(this);
        }
    }
}

void QxtGlobalShortcutPrivate::setKeySequence(const QKeySequence& shortcut)
{
    const Qt::KeyboardModifiers allMods =
            Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::KeypadModifier;
    const auto xkeyCode = static_cast<uint>( shortcut.isEmpty() ? 0 : shortcut[0] );
    // WORKAROUND: Qt has convert some keys to upper case which
    //             breaks some shortcuts on some keyboard layouts.
    const uint keyCode = QChar::toLower(xkeyCode & ~allMods);

    key = Qt::Key(keyCode);
    mods = Qt::KeyboardModifiers(xkeyCode & allMods);
}

bool QxtGlobalShortcutPrivate::setShortcutFallback(const QKeySequence& shortcut)
{
    unsetShortcut();
    setKeySequence(shortcut);
    nativeKey = nativeKeycode(key, mods);
    nativeMods = nativeModifiers(mods);

    registered = registerShortcut(nativeKey, nativeMods);
    if (registered)
        shortcuts.insert(qMakePair(nativeKey, nativeMods), q_ptr);

    return registered;
}

bool QxtGlobalShortcutPrivate::unsetShortcutFallback()
{
    if (registered
            && shortcuts.value(qMakePair(nativeKey, nativeMods)) == q_ptr
            && unregisterShortcut(nativeKey, nativeMods))
    {
        shortcuts.remove(qMakePair(nativeKey, nativeMods));
        registered = false;
        return true;
    }

    return false;
}

void QxtGlobalShortcutPrivate::activateShortcut(quint32 nativeKey, quint32 nativeMods)
{
    QxtGlobalShortcut* shortcut = shortcuts.value(qMakePair(nativeKey, nativeMods));
    if (shortcut && shortcut->isEnabled())
        emit shortcut->activated(shortcut);
}

/*!
    \class QxtGlobalShortcut
    \brief The QxtGlobalShortcut class provides a global shortcut aka "hotkey".

    A global shortcut triggers even if the application is not active. This
    makes it easy to implement applications that react to certain shortcuts
    still if some other application is active or if the application is for
    example minimized to the system tray.

    Example usage:
    \code
    QxtGlobalShortcut* shortcut = new QxtGlobalShortcut(window);
    connect(shortcut, SIGNAL(activated()), window, SLOT(toggleVisibility()));
    shortcut->setShortcut(QKeySequence("Ctrl+Shift+F12"));
    \endcode
 */

/*!
    \fn QxtGlobalShortcut::activated()

    This signal is emitted when the user types the shortcut's key sequence.

    \sa shortcut
 */

/*!
    Constructs a new QxtGlobalShortcut with \a parent.
 */
QxtGlobalShortcut::QxtGlobalShortcut(QObject* parent)
    : QObject(parent)
    , d_ptr(new QxtGlobalShortcutPrivate(this))
{
}

/*!
    Constructs a new QxtGlobalShortcut with \a shortcut and \a parent.
 */
QxtGlobalShortcut::QxtGlobalShortcut(
    const QKeySequence& shortcut, const QString &name, QObject* parent)
    : QxtGlobalShortcut(parent)
{
    setShortcut(shortcut);
    setName(name);
}

/*!
    Destructs the QxtGlobalShortcut.
 */
QxtGlobalShortcut::~QxtGlobalShortcut()
{
    delete d_ptr;
}

/*!
    \property QxtGlobalShortcut::shortcut
    \brief the shortcut key sequence

    Notice that corresponding key press and release events are not
    delivered for registered global shortcuts even if they are disabled.
    Also, comma separated key sequences are not supported.
    Only the first part is used:

    \code
    qxtShortcut->setShortcut(QKeySequence("Ctrl+Alt+A,Ctrl+Alt+B"));
    Q_ASSERT(qxtShortcut->shortcut() == QKeySequence("Ctrl+Alt+A"));
    \endcode

    \sa setShortcut()
 */
QKeySequence QxtGlobalShortcut::shortcut() const
{
    return QKeySequence( static_cast<int>(d_ptr->key | d_ptr->mods) );
}

/*!
    \property QxtGlobalShortcut::shortcut
    \brief sets the shortcut key sequence

    \sa shortcut()
 */
bool QxtGlobalShortcut::setShortcut(const QKeySequence& shortcut)
{
    return d_ptr->setShortcut(shortcut);
}

QString QxtGlobalShortcut::name() const
{
    return d_ptr->name;
}

void QxtGlobalShortcut::setName(const QString& name)
{
    d_ptr->name = name;
}

/*!
    \property QxtGlobalShortcut::enabled
    \brief whether the shortcut is enabled

    A disabled shortcut does not get activated.

    The default value is \c true.

    \sa setEnabled(), setDisabled()
 */
bool QxtGlobalShortcut::isEnabled() const
{
    return d_ptr->enabled;
}

/*!
    \property QxtGlobalShortcut::valid
    \brief whether the shortcut was successfully set up
 */
bool QxtGlobalShortcut::isValid() const
{
    return d_ptr->registered;
}

/*!
    Activates the shortcut if enabled.

    \sa activated()
 */
void QxtGlobalShortcut::activate()
{
    if (d_ptr->enabled)
        emit activated(this);
}

/*!
    Notifies the user that changing global shortcuts requires application restart.

    This needed on some platforms like Wayland, because global shortcuts cannot
    be rebound again.

    This shows a message box for the first found top level window. If no
    visible window is found, nothing is shown. This avoids showing a message
    box when application is exiting.
 */
void QxtGlobalShortcut::notifyRestartNeeded()
{
    const QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
    QWidget *parentWindow = nullptr;
    for (QWidget *widget : topLevelWidgets) {
        if (widget->isVisible()) {
            parentWindow = widget;
            break;
        }
    }
    if (parentWindow == nullptr)
        return;
    QMessageBox::information(
        parentWindow,
        tr("Restart Required"),
        tr("Changing global shortcuts requires application restart.")
    );
}

/*!
    Sets the shortcut \a enabled.

    \sa enabled
 */
void QxtGlobalShortcut::setEnabled(bool enabled)
{
    d_ptr->enabled = enabled;
}

/*!
    Sets the shortcut \a disabled.

    \sa enabled
 */
void QxtGlobalShortcut::setDisabled(bool disabled)
{
    d_ptr->enabled = !disabled;
}
