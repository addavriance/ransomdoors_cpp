#ifdef __APPLE__

#include "ConfigDialog.hpp"

namespace rd::Platform {

// NSPanel-based editor not written yet
bool ShowConfigDialog(Config&) { return false; }

} // namespace rd::Platform

#endif // __APPLE__
