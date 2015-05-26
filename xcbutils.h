/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012, 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_XCB_UTILS_H
#define KWIN_XCB_UTILS_H

#include <xcb/xcb.h>
#include <xcb/composite.h>
#include <xcb/randr.h>
#include <xcb/shm.h>

#include <QX11Info>

namespace Xcb {

typedef xcb_window_t WindowId;

template <typename T> using ScopedCPointer = QScopedPointer<T, QScopedPointerPodDeleter>;

class Atom
{
public:
    explicit Atom(const QByteArray &name, bool onlyIfExists = false, xcb_connection_t *c = QX11Info::connection())
        : m_connection(c)
        , m_retrieved(false)
        , m_cookie(xcb_intern_atom_unchecked(m_connection, onlyIfExists, name.length(), name.constData()))
        , m_atom(XCB_ATOM_NONE)
        , m_name(name)
        {
        }
    Atom() = delete;
    Atom(const Atom &) = delete;

    ~Atom() {
        if (!m_retrieved && m_cookie.sequence) {
            xcb_discard_reply(m_connection, m_cookie.sequence);
        }
    }

    operator xcb_atom_t() const {
        (const_cast<Atom*>(this))->getReply();
        return m_atom;
    }
    bool isValid() {
        getReply();
        return m_atom != XCB_ATOM_NONE;
    }
    bool isValid() const {
        (const_cast<Atom*>(this))->getReply();
        return m_atom != XCB_ATOM_NONE;
    }

    inline const QByteArray &name() const {
        return m_name;
    }

private:
    void getReply() {
        if (m_retrieved || !m_cookie.sequence) {
            return;
        }
        ScopedCPointer<xcb_intern_atom_reply_t> reply(xcb_intern_atom_reply(m_connection, m_cookie, nullptr));
        if (!reply.isNull()) {
            m_atom = reply->atom;
        }
        m_retrieved = true;
    }
    xcb_connection_t *m_connection;
    bool m_retrieved;
    xcb_intern_atom_cookie_t m_cookie;
    xcb_atom_t m_atom;
    QByteArray m_name;
};

} // namespace Xcb

#endif // KWIN_X11_UTILS_H