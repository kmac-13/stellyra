#pragma once

// Q_OBJECT fixtures for test_qt_integration.cpp, kept in their own header
// rather than defined inline in that .cpp (with a manual #include ".moc").
// Reason: in this toolchain, a Q_OBJECT class whose base is also Q_OBJECT
// triggers a moc/GCC "incomplete type" error during Qt6's implicit
// QMetaType registration, specifically when gtest.h (or any similarly
// large standard header such as <regex>) has also been included earlier
// in the same translation unit - reproduced independent of Stellyra with
// plain QObject-derived-from-QObject classes.  Keeping these fixtures in a
// header AUTOMOC moc's into their own small, gtest-free translation unit
// sidesteps the combination entirely.

#include <kmac/stellyra_qt.h>

#include <thread>

class Sender : public QObject
{
	Q_OBJECT
public:
	explicit Sender( QObject* parent = nullptr )
		: QObject( parent )
	{
	}

signals:
	void valueChanged( int value );
};

// Subclasses TrackableBridge to add a real Qt signal/slot of its own - the
// intended way to use connectEventToTrackableBridge() (see its doc comment in
// qt_connect.h).
class TestBridge : public kmac::stellyra::qt::TrackableBridge
{
	Q_OBJECT
public:
	int callCount = 0;
	int lastValue = 0;
	std::thread::id lastThread {};

	void onValue( int value )
	{
		++callCount;
		lastValue = value;
		lastThread = std::this_thread::get_id();
	}
};
