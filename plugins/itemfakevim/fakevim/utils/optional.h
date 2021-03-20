/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#pragma once

/*
    optional<T>
    make_optional(t)

    See std(::experimental)::optional.
*/

// std::optional from Apple's Clang supports methods that throw std::bad_optional_access only
// with deployment target >= macOS 10.14
// TODO: Use std::optional everywhere when we can require macOS 10.14
#if !defined(__apple_build_version__)

#include <optional>

namespace Utils {

using std::optional;
using std::nullopt;
using std::nullopt_t;
using std::in_place;

// make_optional is a copy, since there is no sensible way to import functions in C++
template<class T>
constexpr optional<std::decay_t<T>> make_optional(T &&v)
{
    return optional<std::decay_t<T>>(std::forward<T>(v));
}

template<class T, class... Args>
optional<T> make_optional(Args &&... args)
{
    return optional<T>(in_place, std::forward<Args>(args)...);
}

template<class T, class Up, class... Args>
constexpr optional<T> make_optional(std::initializer_list<Up> il, Args &&... args)
{
    return optional<T>(in_place, il, std::forward<Args>(args)...);
}

} // namespace Utils

#else

#include <3rdparty/optional/optional.hpp>

namespace Utils {

// --> Utils::optional
using std::experimental::optional;
// --> Utils::nullopt
using std::experimental::nullopt;
using std::experimental::nullopt_t;
// --> Utils::in_place
using std::experimental::in_place;

// TODO: make_optional is a copy, since there is no sensible way to import functions in C++
template <class T>
constexpr optional<typename std::decay<T>::type> make_optional(T&& v)
{
  return optional<typename std::decay<T>::type>(std::experimental::constexpr_forward<T>(v));
}

template <class X>
constexpr optional<X&> make_optional(std::reference_wrapper<X> v)
{
  return optional<X&>(v.get());
}

} // Utils

#endif
