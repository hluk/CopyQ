
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

#include "qxtglobal.h"

/*!
    \headerfile <QxtGlobal>
    \title Global Qxt Declarations
    \inmodule QxtCore

    \brief The <QxtGlobal> header provides basic declarations and
    is included by all other Qxt headers.
 */

/*!
    \macro QXT_VERSION
    \relates <QxtGlobal>

    This macro expands a numeric value of the form 0xMMNNPP (MM =
    major, NN = minor, PP = patch) that specifies Qxt's version
    number. For example, if you compile your application against Qxt
    0.4.0, the QXT_VERSION macro will expand to 0x000400.

    You can use QXT_VERSION to use the latest Qt features where
    available. For example:
    \code
    #if QXT_VERSION >= 0x000400
        qxtTabWidget->setTabMovementMode(QxtTabWidget::InPlaceMovement);
    #endif
    \endcode

    \sa QXT_VERSION_STR, qxtVersion()
 */

/*!
    \macro QXT_VERSION_STR
    \relates <QxtGlobal>

    This macro expands to a string that specifies Qxt's version number
    (for example, "0.4.0"). This is the version against which the
    application is compiled.

    \sa qxtVersion(), QXT_VERSION
 */

/*!
    \relates <QxtGlobal>

    Returns the version number of Qxt at run-time as a string (for
    example, "0.4.0"). This may be a different version than the
    version the application was compiled against.

    \sa QXT_VERSION_STR
 */
const char * qxtVersion()
{
    return QXT_VERSION_STR;
}

/*!
\headerfile <QxtPimpl>
\title The Qxt private implementation
\inmodule QxtCore

\brief The <QxtPimpl> header provides tools for hiding
details of a class.

Application code generally doesn't have to be concerned about hiding its
implementation details, but when writing library code it is important to
maintain a constant interface, both source and binary. Maintaining a constant
source interface is easy enough, but keeping the binary interface constant
means moving implementation details into a private class. The PIMPL, or
d-pointer, idiom is a common method of implementing this separation. QxtPimpl
offers a convenient way to connect the public and private sides of your class.

\section1 Getting Started
Before you declare the public class, you need to make a forward declaration
of the private class. The private class must have the same name as the public
class, followed by the word Private. For example, a class named MyTest would
declare the private class with:
\code
class MyTestPrivate;
\endcode

\section1 The Public Class
Generally, you shouldn't keep any data members in the public class without a
good reason. Functions that are part of the public interface should be declared
in the public class, and functions that need to be available to subclasses (for
calling or overriding) should be in the protected section of the public class.
To connect the private class to the public class, include the
QXT_DECLARE_PRIVATE macro in the private section of the public class. In the
example above, the private class is connected as follows:
\code
private:
    QXT_DECLARE_PRIVATE(MyTest)
\endcode

Additionally, you must include the QXT_INIT_PRIVATE macro in the public class's
constructor. Continuing with the MyTest example, your constructor might look
like this:
\code
MyTest::MyTest() {
    // initialization
    QXT_INIT_PRIVATE(MyTest);
}
\endcode

\section1 The Private Class
As mentioned above, data members should usually be kept in the private class.
This allows the memory layout of the private class to change without breaking
binary compatibility for the public class. Functions that exist only as
implementation details, or functions that need access to private data members,
should be implemented here.

To define the private class, inherit from the template QxtPrivate class, and
include the QXT_DECLARE_PUBLIC macro in its public section. The template
parameter should be the name of the public class. For example:
\code
class MyTestPrivate : public QxtPrivate<MyTest> {
public:
    MyTestPrivate();
    QXT_DECLARE_PUBLIC(MyTest)
};
\endcode

\section1 Accessing Private Members
Use the qxt_d() function (actually a function-like object) from functions in
the public class to access the private class. Similarly, functions in the
private class can invoke functions in the public class by using the qxt_p()
function (this one's actually a function).

For example, assume that MyTest has methods named getFoobar and doBaz(),
and MyTestPrivate has a member named foobar and a method named doQuux().
The code might resemble this example:
\code
int MyTest::getFoobar() {
    return qxt_d().foobar;
}

void MyTestPrivate::doQuux() {
    qxt_p().doBaz(foobar);
}
\endcode
*/

/*! 
 * \macro QXT_DECLARE_PRIVATE(PUB)
 * \relates <QxtPimpl>
 * Declares that a public class has a related private class.
 *
 * This shuold be put in the private section of the public class. The
 * parameter \a PUB must be the name of the public class.
 */

/*!
 * \macro QXT_DECLARE_PUBLIC(PUB)
 * \relates <QxtPimpl>
 * Declares that a private class has a related public class named \a PUB.
 *
 * This may be put anywhere in the declaration of the private class. The parameter is the name of the public class.
 */

/*!
 * \macro QXT_INIT_PRIVATE(PUB)
 * \relates <QxtPimpl>
 * Initializes resources owned by the private class.
 *
 * This should be called from the public class's constructor,
 * before qxt_d() is used for the first time. The parameter \a PUB must be
 * the name of the public class.
 */

/*!
 * \macro QXT_D(PUB)
 * \relates <QxtPimpl>
 * Returns a reference in the current scope named "d" to the private class
 * associated with the public class \a PUB.
 *
 * This function is only available in a class using QXT_DECLARE_PRIVATE().
 */

/*!
 * \macro QXT_P(PUB)
 * \relates <QxtPimpl>
 * Creates a reference in the current scope named "q" to the public class
 * named \a PUB.
 *
 * This macro only works in a class using QXT_DECLARE_PUBLIC().
 */

/*!
 * \fn QxtPrivate<PUB>& PUB::qxt_d()
 * \relates <QxtPimpl>
 * Returns a reference to the private class.
 *
 * This function is only available in a class using \a QXT_DECLARE_PRIVATE.
 */

/*!
 * \fn const QxtPrivate<PUB>& PUB::qxt_d() const
 * \relates <QxtPimpl>
 * Returns a const reference to the private class.
 *
 * This function is only available in a class using \a QXT_DECLARE_PRIVATE.
 * This overload will be automatically used in const functions.
 */

/*!
 * \fn PUB& QxtPrivate::qxt_p()
 * \relates <QxtPimpl>
 * Returns a reference to the public class.
 *
 * This function is only available in a class using QXT_DECLARE_PUBLIC().
 */

/*!
 * \fn const PUB& QxtPrivate::qxt_p() const
 * \relates <QxtPimpl>
 * Returns a const reference to the public class.
 *
 * This function is only available in a class using QXT_DECLARE_PUBLIC().
 * This overload will be automatically used in const functions.
 */
