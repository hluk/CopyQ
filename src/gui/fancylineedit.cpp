/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "execmenu.h"
#include "fancylineedit.h"

#include "common/common.h"
#include "gui/iconfactory.h"

#include <QAbstractItemView>
#include <QKeyEvent>
#include <QMenu>
#include <QStylePainter>
#include <QStyle>

/*!
    \class Utils::FancyLineEdit

    \brief The FancyLineEdit class is an enhanced line edit with several
    opt-in features.

    A FancyLineEdit instance can have:

    \list
    \li An embedded pixmap on one side that is connected to a menu.

    \li A grayed hintText (like "Type Here to")
    when not focused and empty. When connecting to the changed signals and
    querying text, one has to be aware that the text is set to that hint
    text if isShowingHintText() returns true (that is, does not contain
    valid user input).
 */

namespace {

qreal devicePixelRatio(const QWidget &w)
{
#if QT_VERSION < 0x050000
    return w.logicalDpiX() > 150 ? 2.0 : 1.0;
#else
    return w.devicePixelRatio();
#endif
}

} // namespace

namespace Utils {

// --------- FancyLineEditPrivate
class FancyLineEditPrivate : public QObject
{
public:
    explicit FancyLineEditPrivate(FancyLineEdit *parent);

    bool eventFilter(QObject *obj, QEvent *event) override;

    FancyLineEdit *m_lineEdit;
    QString m_oldText;
    QMenu *m_menu[2]{};
    bool m_menuTabFocusTrigger[2]{};
    IconButton *m_iconbutton[2]{};
    bool m_iconEnabled[2]{};
};


FancyLineEditPrivate::FancyLineEditPrivate(FancyLineEdit *parent) :
    QObject(parent),
    m_lineEdit(parent)
{
    for (int i = 0; i < 2; ++i) {
        m_menu[i] = nullptr;
        m_menuTabFocusTrigger[i] = false;
        m_iconbutton[i] = new IconButton(parent);
        m_iconbutton[i]->installEventFilter(this);
        m_iconbutton[i]->hide();
        m_iconEnabled[i] = false;
    }
}

bool FancyLineEditPrivate::eventFilter(QObject *obj, QEvent *event)
{
    int buttonIndex = -1;
    for (int i = 0; i < 2; ++i) {
        if (obj == m_iconbutton[i]) {
            buttonIndex = i;
            break;
        }
    }

    if (buttonIndex == -1)
        return QObject::eventFilter(obj, event);

    if ( event->type() == QEvent::FocusIn
         && m_menuTabFocusTrigger[buttonIndex]
         && m_menu[buttonIndex])
    {
        m_lineEdit->setFocus();
        execMenuAtWidget(m_menu[buttonIndex], m_iconbutton[buttonIndex]);
        return true;
    }

    return QObject::eventFilter(obj, event);
}


// --------- FancyLineEdit
FancyLineEdit::FancyLineEdit(QWidget *parent) :
    QLineEdit(parent),
    d(new FancyLineEditPrivate(this))
{
    ensurePolished();
    updateMargins();

    connect(d->m_iconbutton[Left], SIGNAL(clicked()), this, SLOT(iconClicked()));
    connect(d->m_iconbutton[Right], SIGNAL(clicked()), this, SLOT(iconClicked()));
}

FancyLineEdit::~FancyLineEdit() = default;

void FancyLineEdit::setButtonVisible(Side side, bool visible)
{
    d->m_iconbutton[side]->setVisible(visible);
    d->m_iconEnabled[side] = visible;
    updateMargins();
}

bool FancyLineEdit::isButtonVisible(Side side) const
{
    return d->m_iconEnabled[side];
}

QAbstractButton *FancyLineEdit::button(FancyLineEdit::Side side) const
{
    return d->m_iconbutton[side];
}

void FancyLineEdit::iconClicked()
{
    IconButton *button = qobject_cast<IconButton *>(sender());
    int index = -1;
    for (int i = 0; i < 2; ++i)
        if (d->m_iconbutton[i] == button)
            index = i;
    if (index == -1)
        return;
    if (d->m_menu[index]) {
        execMenuAtWidget(d->m_menu[index], button);
    } else {
        emit buttonClicked(static_cast<Side>(index));
        if (index == Left)
            emit leftButtonClicked();
        else if (index == Right)
            emit rightButtonClicked();
    }
}

void FancyLineEdit::updateMargins()
{
    bool leftToRight = (layoutDirection() == Qt::LeftToRight);
    Side realLeft = (leftToRight ? Left : Right);
    Side realRight = (leftToRight ? Right : Left);

    auto leftMargin = static_cast<int>( d->m_iconbutton[realLeft]->sizeHint().width() + 8 );
    auto rightMargin = static_cast<int>( d->m_iconbutton[realRight]->sizeHint().width() + 8 );
    // Note KDE does not reserve space for the highlight color
    if (style()->inherits("OxygenStyle")) {
        leftMargin = qMax(24, leftMargin);
        rightMargin = qMax(24, rightMargin);
    }

    QMargins margins((d->m_iconEnabled[realLeft] ? leftMargin : 0), 0,
                     (d->m_iconEnabled[realRight] ? rightMargin : 0), 0);

    setTextMargins(margins);
}

void FancyLineEdit::updateButtonPositions()
{
    QRect contentRect = rect();
    for (int i = 0; i < 2; ++i) {
        Side iconpos = static_cast<Side>(i);
        if (layoutDirection() == Qt::RightToLeft)
            iconpos = (iconpos == Left ? Right : Left);

        if (iconpos == FancyLineEdit::Right) {
            const int iconoffset = textMargins().right() + 4;
            d->m_iconbutton[i]->setGeometry(contentRect.adjusted(width() - iconoffset, 0, 0, 0));
        } else {
            const int iconoffset = textMargins().left() + 4;
            d->m_iconbutton[i]->setGeometry(contentRect.adjusted(0, 0, -width() + iconoffset, 0));
        }
    }
}

void FancyLineEdit::resizeEvent(QResizeEvent *)
{
    updateButtonPositions();
}

void FancyLineEdit::setButtonIcon(Side side, const QIcon &icon)
{
    d->m_iconbutton[side]->setIcon( icon.pixmap(16) );
    updateMargins();
    updateButtonPositions();
    update();
}

void FancyLineEdit::setButtonMenu(Side side, QMenu *buttonMenu)
{
     d->m_menu[side] = buttonMenu;
     d->m_iconbutton[side]->setHasMenu(buttonMenu != nullptr);
 }

QMenu *FancyLineEdit::buttonMenu(Side side) const
{
    return  d->m_menu[side];
}

bool FancyLineEdit::hasMenuTabFocusTrigger(Side side) const
{
    return d->m_menuTabFocusTrigger[side];
}

void FancyLineEdit::setMenuTabFocusTrigger(Side side, bool v)
{
    if (d->m_menuTabFocusTrigger[side] == v)
        return;

    d->m_menuTabFocusTrigger[side] = v;
    d->m_iconbutton[side]->setFocusPolicy(v ? Qt::TabFocus : Qt::NoFocus);
}

void FancyLineEdit::setButtonToolTip(Side side, const QString &tip)
{
    d->m_iconbutton[side]->setToolTip(tip);
}

void FancyLineEdit::setButtonFocusPolicy(Side side, Qt::FocusPolicy policy)
{
    d->m_iconbutton[side]->setFocusPolicy(policy);
}


//
// IconButton - helper class to represent a clickable icon
//

IconButton::IconButton(QWidget *parent)
    : QAbstractButton(parent)
    , m_hasMenu(false)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
}

void IconButton::paintEvent(QPaintEvent *)
{
    const qreal ratio = ::devicePixelRatio(*this);
    const auto iconSize = static_cast<int>( qMin(width(), height()) - ratio * 8 );
    const QPixmap pixmap = m_icon.pixmap(iconSize);

    QRect pixmapRect = pixmap.rect();
    auto pixmapCenter = rect().center();
    if (m_hasMenu)
        pixmapCenter.setX( pixmapCenter.x() - 3 );
    pixmapRect.moveCenter(pixmapCenter);

    QStylePainter painter(this);

    m_icon.paint(&painter, pixmapRect);

    if (m_hasMenu) {
        // small triangle next to icon to indicate menu
        QPolygon triangle;
        triangle.append(QPoint(0, 0));
        triangle.append(QPoint(6, 0));
        triangle.append(QPoint(3, 3));

        const QColor c = getDefaultIconColor(*this);
        painter.save();
        painter.translate(pixmapRect.bottomRight() + QPoint(0, -6));
        painter.setBrush(c);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(triangle);
        painter.restore();
    }

    if (hasFocus()) {
        QStyleOptionFocusRect focusOption;
        focusOption.initFrom(this);
        focusOption.rect = pixmapRect;
#ifdef Q_OS_MAC
        focusOption.rect.adjust(-4, -4, 4, 4);
        painter.drawControl(QStyle::CE_FocusFrame, focusOption);
#endif
        painter.drawPrimitive(QStyle::PE_FrameFocusRect, focusOption);
    }
}

QSize IconButton::sizeHint() const
{
    return QSize(16 + (m_hasMenu ? 3 : 0), 16);
}

void IconButton::keyPressEvent(QKeyEvent *ke)
{
    QAbstractButton::keyPressEvent(ke);
    if (!ke->modifiers() && (ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Return))
        click();
    // do not forward to line edit
    ke->accept();
}

void IconButton::keyReleaseEvent(QKeyEvent *ke)
{
    QAbstractButton::keyReleaseEvent(ke);
    // do not forward to line edit
    ke->accept();
}

} // namespace Utils
