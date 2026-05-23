"""Ensure include/secrets.h exists before compile (copy from example if missing)."""
from pathlib import Path
import shutil

Import("env")

src = Path("include/secrets.h")
example = Path("include/secrets.h.example")
if not src.exists() and example.exists():
    shutil.copyfile(example, src)
    print(
        "MQTT: created include/secrets.h from secrets.h.example. "
        "Defaults are blank, so the device will boot with MQTT disabled. "
        "Configure the broker from the Wi-Fi captive portal after flashing."
    )
