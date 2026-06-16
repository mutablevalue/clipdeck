#pragma once

#include "../settings/settings_store.hpp"

namespace clipdeck {

bool SetupNativeRecorder(ClipDeckSettings &settings);
bool DiagnoseNativeRecorder(const ClipDeckSettings &settings);

} // namespace clipdeck
