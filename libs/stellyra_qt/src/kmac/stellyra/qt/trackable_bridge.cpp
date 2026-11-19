#include <kmac/stellyra/qt/trackable_bridge.h>

namespace kmac {
namespace stellyra {
namespace qt {

TrackableBridge::TrackableBridge( QObject* parent /*= nullptr*/ )
	: QObject( parent )
{
}

Trackable& TrackableBridge::trackable()
{
	return _trackable;
}

} // namespace qt
} // namespace stellyra
} // namespace kmac
