/****************************************************************************
** 
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
** 
** This file is part of a Qt Solutions component.
**
** Commercial Usage  
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Solutions Commercial License Agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and Nokia.
** 
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
** 
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.1, included in the file LGPL_EXCEPTION.txt in this
** package.
** 
** GNU General Public License Usage 
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
** 
** Please note Third Party Software included with Qt Solutions may impose
** additional restrictions and it is the user's responsibility to ensure
** that they have met the licensing requirements of the GPL, LGPL, or Qt
** Solutions Commercial license and the relevant license of the Third
** Party Software they are using.
** 
** If you are unsure which license is appropriate for your use, please
** contact Nokia at qt-info@nokia.com.
** 
****************************************************************************/

#include <qtsingleapplication.h>
#include <QtCore/QFile>
#include <QtGui/QMainWindow>
#include <QtGui/QPrinter>
#include <QtGui/QPainter>
#include <QtGui/QTextEdit>
#include <QtGui/QMdiArea>
#include <QtCore/QTextStream>

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow();

public slots:
    void handleMessage(const QString& message);

signals:
    void needToShow();

private:
    QMdiArea *workspace;
};

MainWindow::MainWindow()
{
    workspace = new QMdiArea(this);

    setCentralWidget(workspace);
}

void MainWindow::handleMessage(const QString& message)
{
    enum Action {
	Nothing,
	Open,
	Print
    } action;

    action = Nothing;
    QString filename = message;
    if (message.toLower().startsWith("/print ")) {
	filename = filename.mid(7);
	action = Print;
    } else if (!message.isEmpty()) {
	action = Open;
    }
    if (action == Nothing) {
        emit needToShow();
	return;
    }

    QFile file(filename);
    QString contents;
    if (file.open(QIODevice::ReadOnly))
        contents = file.readAll();
    else
        contents = "[[Error: Could not load file " + filename + "]]";

    QTextEdit *view = new QTextEdit;
    view->setPlainText(contents);

    switch(action) {
    case Print:
	{
	    QPrinter printer;
            view->print(&printer);
            delete view;
        }
	break;

    case Open:
	{
	    workspace->addSubWindow(view);
	    view->setWindowTitle(message);
	    view->show();
            emit needToShow();
	}
	break;
    default:
	break;
    };
}

#include "main.moc"

int main(int argc, char **argv)
{
    QtSingleApplication instance("File loader QtSingleApplication example", argc, argv);
    QString message;
    for (int a = 1; a < argc; ++a) {
	message += argv[a];
	if (a < argc-1)
	    message += " ";
    }

    if (instance.sendMessage(message))
	return 0;

    MainWindow mw;
    mw.handleMessage(message);
    mw.show();

    QObject::connect(&instance, SIGNAL(messageReceived(const QString&)),
		     &mw, SLOT(handleMessage(const QString&)));

    instance.setActivationWindow(&mw, false);
    QObject::connect(&mw, SIGNAL(needToShow()), &instance, SLOT(activateWindow()));

    return instance.exec();
}
