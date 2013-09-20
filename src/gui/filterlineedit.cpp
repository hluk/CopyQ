/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "filterlineedit.h"

#include "gui/configurationmanager.h"
#include "gui/iconfactory.h"

#include <QMenu>
#include <QPainter>
#include <QTimer>

/*!
    \class Utils::FilterLineEdit

    \brief The FilterLineEdit class is a fancy line edit customized for
    filtering purposes with a clear button.
*/

namespace Utils {

FilterLineEdit::FilterLineEdit(QWidget *parent)
   : FancyLineEdit(parent)
   , m_timerSearch( new QTimer(this) )
{
    setButtonVisible(Left, true);
    setButtonVisible(Right, true);
    setButtonToolTip(Right, tr("Clear text"));
    setAutoHideButton(Right, true);
    connect(this, SIGNAL(rightButtonClicked()), this, SLOT(clear()));

    // search timer
    m_timerSearch->setSingleShot(true);
    m_timerSearch->setInterval(200);

    connect( m_timerSearch, SIGNAL(timeout()),
             this, SLOT(onTextChanged()) );
    connect( this, SIGNAL(textChanged(QString)),
             m_timerSearch, SLOT(start()) );

    // menu
    QMenu *menu = new QMenu(this);
    setButtonMenu(Left, menu);
    connect( menu, SIGNAL(triggered(QAction*)),
             this, SLOT(onMenuAction()) );

    m_actionRe = menu->addAction(tr("Regular Expression"));
    m_actionRe->setCheckable(true);

    m_actionCaseInsensitive = menu->addAction(tr("Case Insensitive"));
    m_actionCaseInsensitive->setCheckable(true);

    ConfigurationManager *cm = ConfigurationManager::instance();
    QVariant val;
    val = cm->loadValue("Options/filter_regular_expression");
    m_actionRe->setChecked(!val.isValid() || val.toBool());
    val = cm->loadValue("Options/filter_case_insensitive");
    m_actionCaseInsensitive->setChecked(!val.isValid() || val.toBool());

    // icons
    connect( ConfigurationManager::instance(), SIGNAL(configurationChanged()),
             this, SLOT(loadSettings()) );
    loadSettings();
}

QRegExp FilterLineEdit::filter() const
{
    Qt::CaseSensitivity sensitivity =
            m_actionCaseInsensitive->isChecked() ? Qt::CaseInsensitive : Qt::CaseSensitive;

    QString pattern;
    if (m_actionRe->isChecked()) {
        pattern = text();
    } else {
        foreach ( const QString &str, text().split(QRegExp("\\s+"), QString::SkipEmptyParts) )
            pattern.append( ".*" + QRegExp::escape(str) );
    }

    return QRegExp(pattern, sensitivity, QRegExp::RegExp2);
}

void FilterLineEdit::onTextChanged()
{
    emit filterChanged(filter());
}

void FilterLineEdit::onMenuAction()
{
    ConfigurationManager *cm = ConfigurationManager::instance();
    cm->saveValue("Options/filter_regular_expression", m_actionRe->isChecked());
    cm->saveValue("Options/filter_case_insensitive", m_actionCaseInsensitive->isChecked());

    const QRegExp re = filter();
    if ( !re.isEmpty() )
        emit filterChanged(re);
}

void FilterLineEdit::loadSettings()
{
    // KDE has custom icons for this. Notice that icon namings are counter intuitive.
    // If these icons are not available we use the freedesktop standard name before
    // falling back to a bundled resource.
    QIcon icon = QIcon::fromTheme(layoutDirection() == Qt::LeftToRight ?
                     "edit-clear-locationbar-rtl" : "edit-clear-locationbar-ltr",
                     getIcon("edit-clear", IconRemove));
    setButtonPixmap(Right, icon.pixmap(16));

    QPixmap pix = getIcon("edit-find", IconSearch).pixmap(16);

    // small triangle next to icon to indicate menu
    QPolygon triangle;
    triangle.append(QPoint(17, 12));
    triangle.append(QPoint(21, 12));
    triangle.append(QPoint(19, 14));

    QPixmap pix2(22, 16);
    pix2.fill(Qt::transparent);
    QPainter p(&pix2);
    p.drawPixmap(0, 0, pix);
    const QColor c = QColor(0,0,0,160);
    p.setBrush(c);
    p.setPen(c);
    p.drawPolygon(triangle);

    setButtonPixmap(Left, pix2);
}


} // namespace Utils
