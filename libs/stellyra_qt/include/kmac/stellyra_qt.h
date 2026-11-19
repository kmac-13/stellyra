#pragma once
#ifndef KMAC_STELLYRA_QT_H
#define KMAC_STELLYRA_QT_H

/**
 * @file stellyra_qt.h
 * @brief Umbrella header guarding the stellyra.h include to prevent
 * compilation issues due to conflicts between Qt's definition of the `emit`
 * keyword and the events' `emit` methods.  This header can be used in lieu
 * of defining the `QT_NO_KEYWORDS` compiler flag.
 *
 * Also pulls in the Qt integration module (kmac::stellyra::qt):
 * - <kmac/stellyra/qt/event_loop_bridge.h> - EventLoopBridge, which drains
 *   a Stellyra EventLoop on a QObject's thread via Qt's own event loop.
 * - <kmac/stellyra/qt/trackable_bridge.h> - TrackableBridge, a shared
 *   lifetime anchor for a receiver wired to both Qt signals and Stellyra
 *   events.
 * - <kmac/stellyra/qt/qt_connect.h> - connectSignalToEvent() and
 *   connectEventToBridge(), free functions bridging the two in each
 *   direction.
 */

// protect against conflict with Qt's emit keyword
#pragma push_macro("emit")
#undef emit
#include <kmac/stellyra.h>
#pragma pop_macro("emit")

#include <kmac/stellyra/qt/event_loop_bridge.h>
#include <kmac/stellyra/qt/qt_connect.h>
#include <kmac/stellyra/qt/trackable_bridge.h>

#endif // KMAC_STELLYRA_QT_H
