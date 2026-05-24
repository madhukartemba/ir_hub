#pragma once

#include "PendingOta.h"

namespace ota_downloader {

[[noreturn]] void runDownloaderMode(const pending_ota::Slot& slot);

}  // namespace ota_downloader
