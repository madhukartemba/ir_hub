"""Ensure include/secrets.h exists before compile (copy from example if missing)."""
from pathlib import Path
import shutil

Import("env")

src = Path("include/secrets.h")
example = Path("include/secrets.h.example")
if not src.exists() and example.exists():
    shutil.copyfile(example, src)
    print("MQTT: created include/secrets.h from secrets.h.example — set MQTT_PASSWORD before use")
