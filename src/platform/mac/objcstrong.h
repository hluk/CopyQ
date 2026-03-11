// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#ifdef __OBJC__
#   import <Foundation/NSObject.h>
#endif

/**
 * RAII strong-reference holder for Objective-C objects (manual retain/release).
 *
 * Retains the object on construction/assignment and releases it on
 * destruction, exactly like a \c __strong property under ARC.  Using this
 * wrapper instead of raw pointers eliminates the class of bugs where a
 * constructor forgets to retain an autoreleased object while the destructor
 * unconditionally releases it.
 *
 * Safe to use with \c nil: retaining and releasing \c nil are no-ops.
 *
 * \code
 *     ObjCStrong<NSWindow> m_window;       // starts nil
 *     m_window = [NSApp windowWithWindowNumber: wid];  // retains
 *     m_window = nil;                      // releases old, holds nil
 *     // destructor releases automatically
 * \endcode
 */
template <typename T>
class ObjCStrong {
public:
    ObjCStrong() : m_ptr(nil) {}

    // Intentionally implicit: allows `m_member = [SomeClass ...]` naturally.
    // NOLINTNEXTLINE(google-explicit-constructor)
    ObjCStrong(T *ptr) : m_ptr([ptr retain]) {}

    ~ObjCStrong() { [m_ptr release]; }

    ObjCStrong(const ObjCStrong &other) : m_ptr([other.m_ptr retain]) {}

    ObjCStrong &operator=(const ObjCStrong &other)
    {
        if (m_ptr != other.m_ptr) {
            [m_ptr release];
            m_ptr = [other.m_ptr retain];
        }
        return *this;
    }

    ObjCStrong &operator=(T *ptr)
    {
        if (m_ptr != ptr) {
            [m_ptr release];
            m_ptr = [ptr retain];
        }
        return *this;
    }

    T *get() const { return m_ptr; }
    operator T *() const { return m_ptr; }
    T *operator->() const { return m_ptr; }
    explicit operator bool() const { return m_ptr != nil; }

private:
    T *m_ptr;
};
