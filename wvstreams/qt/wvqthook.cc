/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * A Qt object that invokes its callback whenever it receives
 * an event.  This is useful for deferring processing to the
 * Qt event loop.  Use it to avoid problems resulting from the
 * non-reentrant nature of WvStream::execute().
 */
#include "wvqthook.moc"

WvQtHook::WvQtHook(WvQtHookCallback _callback) :
    callback(_callback)
{
}


void WvQtHook::setcallback(WvQtHookCallback _callback)
{
    callback = _callback;
}


bool WvQtHook::event(QEvent *event)
{
    if (! callback)
        return false;
    QEvent::Type eventtype = event->type();
    if (eventtype < QEvent::User || eventtype > QEvent::MaxUser)
        return false;
    QCustomEvent *ce = static_cast<QCustomEvent*>(event);
    callback(*this, eventtype - QEvent::User, ce->data());
    return true;
}


void WvQtHook::post(int type, void *data)
{
    // event must be allocated on heap for postEvent
    QEvent::Type eventtype = QEvent::Type(QEvent::User + type);
    QCustomEvent *event = new QCustomEvent(eventtype, data);
    QApplication::postEvent(this, event);
}


void WvQtHook::send(int type, void *data)
{
    QEvent::Type eventtype = QEvent::Type(QEvent::User + type);
    QCustomEvent event(eventtype, data);
    QApplication::sendEvent(this, & event);
}
