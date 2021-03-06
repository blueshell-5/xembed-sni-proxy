/***************************************************************************
 *   fdoselectionmanager.cpp                                               *
 *                                                                         *
 *   Copyright (C) 2008 Jason Stubbs <jasonbstubbs@gmail.com>              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "fdoselectionmanager.h"

#include <QDebug>

#include <QCoreApplication>
#include <QHash>
#include <QTimer>

#include <QTextDocument>
#include <QX11Info>

// #include <KIconLoader>

// #include <Plasma/DataEngine>
// #include <Plasma/DataEngineManager>
// #include <Plasma/ServiceJob>

// #include <config-X11.h>

#include <KWindowSystem>

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_event.h>
#include <xcb/composite.h>
#include <xcb/damage.h>

#include "xcbutils.h"
#include "sniproxy.h"

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

static FdoSelectionManager *s_manager = 0;

class FdoSelectionManagerPrivate
{
public:
    FdoSelectionManagerPrivate(FdoSelectionManager *q)
        : q(q)
    {
    }

    void handleRequestDock(xcb_window_t embed_win);

    uint8_t damageEventBase;
    u_int32_t m_damage;

    QHash<WId, u_int32_t> m_damageWatches;
    QHash<WId, SNIProxy*> m_proxies;

//     QHash<WId, MessageRequest> messageRequests;
//     QHash<WId, FdoTask*> tasks;

    FdoSelectionManager *q;
};

FdoSelectionManager::FdoSelectionManager()
    : d(new FdoSelectionManagerPrivate(this))
{
    initSelection();

    //load damage extension
    xcb_connection_t *c = QX11Info::connection();
    xcb_prefetch_extension_data(c, &xcb_damage_id);
    const auto *reply = xcb_get_extension_data(c, &xcb_damage_id);
    d->damageEventBase = reply->first_event;
    if (reply->present) {
        xcb_damage_query_version_unchecked(c, XCB_DAMAGE_MAJOR_VERSION, XCB_DAMAGE_MINOR_VERSION);
    }
}

FdoSelectionManager::~FdoSelectionManager()
{

}

void FdoSelectionManager::addDamageWatch(WId client)
{
    xcb_connection_t *c = QX11Info::connection();
    const auto attribsCookie = xcb_get_window_attributes_unchecked(c, client);

    auto damageId = xcb_generate_id(c);
    d->m_damageWatches[client] = damageId;
    xcb_damage_create(c, damageId, client, XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);

    QScopedPointer<xcb_get_window_attributes_reply_t, QScopedPointerPodDeleter> attr(xcb_get_window_attributes_reply(c, attribsCookie, Q_NULLPTR));
    uint32_t events = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    if (!attr.isNull()) {
        events = events | attr->your_event_mask;
    }
    // the event mask will not be removed again. We cannot track whether another component also needs STRUCTURE_NOTIFY (e.g. KWindowSystem).
    // if we would remove the event mask again, other areas will break.
    xcb_change_window_attributes(c, client, XCB_CW_EVENT_MASK, &events);
}

bool FdoSelectionManager::nativeEventFilter(const QByteArray& eventType, void* message, long int* result)
{
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t* ev = static_cast<xcb_generic_event_t *>(message);

        auto responseType = XCB_EVENT_RESPONSE_TYPE(ev);
        if (responseType == XCB_CLIENT_MESSAGE) {
            auto ce = reinterpret_cast<xcb_client_message_event_t *>(ev);
            if (ce->type == Xcb::atoms->opcodeAtom) {
                switch (ce->data.data32[1]) {
                    case SYSTEM_TRAY_REQUEST_DOCK:
                        d->handleRequestDock(ce->data.data32[2]);
                        return true;
                }
            }
        }
        else if (responseType == d->damageEventBase + XCB_DAMAGE_NOTIFY) {
            auto damagedWId = reinterpret_cast<xcb_damage_notify_event_t *>(ev)->drawable;
            auto sniProx = d->m_proxies[damagedWId];
            Q_ASSERT(sniProx);
            sniProx->update();
            xcb_damage_subtract(QX11Info::connection(), d->m_damageWatches[damagedWId], XCB_NONE, XCB_NONE);
        }
        else  if (responseType == XCB_PROPERTY_NOTIFY) {
            qDebug() << "prop notify";
            xcb_property_notify_event_t *pe = reinterpret_cast<xcb_property_notify_event_t *>(ev);
            Xcb::XCBEventDispatcher::instance()->flush(pe->time);
        }
    }
    return false;
}

void FdoSelectionManager::initSelection()
{
    s_manager = this;

    xcb_client_message_event_t ev;

    auto cookie = xcb_intern_atom(QX11Info::connection(), false, strlen("MANAGER"), "MANAGER");
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = QX11Info::appRootWindow();
    ev.format = 32;
    ev.type = xcb_intern_atom_reply(QX11Info::connection(), cookie, NULL)->atom;
    ev.data.data32[0] = XCB_CURRENT_TIME;
    ev.data.data32[1] = Xcb::atoms->selectionAtom;
    ev.data.data32[2] = winId();
    ev.data.data32[3] = 0;
    ev.data.data32[4] = 0;

    xcb_set_selection_owner(QX11Info::connection(),
                            winId(),
                            Xcb::atoms->selectionAtom,
                            XCB_CURRENT_TIME);

    xcb_send_event(QX11Info::connection(), false, QX11Info::appRootWindow(), 0xFFFFFF, (char *) &ev);

    qApp->installNativeEventFilter(this);
}

void FdoSelectionManagerPrivate::handleRequestDock(xcb_window_t winId)
{
    qDebug() << "DOCK REQUESTED!";
//     if (tasks.contains(winId)) {
//         qDebug() << "got a dock request from an already existing task";
//         return;
//     }

    m_proxies[winId] = new SNIProxy(winId);

    q->addDamageWatch(winId);
 //     emit q->taskCreated(task);

}

// void FdoSelectionManager::cleanupTask(WId winId)
// {
//     d->tasks.remove(winId);
// }


