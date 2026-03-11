// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <CoreFoundation/CoreFoundation.h>

/**
 * RAII wrapper for Core Foundation references (CFRetain/CFRelease).
 *
 * Automatically releases the held reference on destruction.  Eliminates
 * the class of bugs where an early return or exception skips a CFRelease.
 *
 * By default the constructor **adopts** (takes ownership of) a +1 reference
 * returned by CF *Create* / *Copy* functions — no extra retain.
 *
 * \code
 *     // Adopt a +1 ref from a Create/Copy function (no extra retain):
 *     CFRef<CGEventRef> event = CGEventCreateKeyboardEvent(src, key, YES);
 *
 *     // Adopt a +1 ref from a Copy function:
 *     CFRef<CFArrayRef> items = LSSharedFileListCopySnapshot(list, &seed);
 *
 *     // Implicit conversion to the raw type for passing to CF APIs:
 *     CGEventPost(kCGHIDEventTap, event);
 *
 *     // Release is automatic; no manual CFRelease needed.
 * \endcode
 */
template <typename T>
class CFRef {
public:
    CFRef() : m_ref(nullptr) {}

    // Adopts a +1 reference (from Create/Copy).  Intentionally implicit so
    // `CFRef<X> x = SomeCreate(...)` works naturally.
    // NOLINTNEXTLINE(google-explicit-constructor)
    CFRef(T ref) : m_ref(ref) {}

    ~CFRef()
    {
        if (m_ref)
            CFRelease(m_ref);
    }

    CFRef(const CFRef &other) : m_ref(other.m_ref)
    {
        if (m_ref)
            CFRetain(m_ref);
    }

    CFRef &operator=(const CFRef &other)
    {
        if (m_ref != other.m_ref) {
            if (m_ref)
                CFRelease(m_ref);
            m_ref = other.m_ref;
            if (m_ref)
                CFRetain(m_ref);
        }
        return *this;
    }

    CFRef &operator=(T ref)
    {
        if (m_ref != ref) {
            if (m_ref)
                CFRelease(m_ref);
            m_ref = ref;
        }
        return *this;
    }

    T get() const { return m_ref; }
    operator T() const { return m_ref; }
    explicit operator bool() const { return m_ref != nullptr; }

private:
    T m_ref;
};
