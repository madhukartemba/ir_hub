#!/usr/bin/env python3
"""
Script to automatically add LOG_REGISTER_CLASS to class definitions.
This helps migrate existing code to use the enhanced logging system.
"""

import os
import re
import sys
from pathlib import Path


def find_cpp_files(directory):
    """Find all .cpp and .h files in the directory."""
    cpp_files = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith((".cpp", ".h")):
                cpp_files.append(os.path.join(root, file))
    return cpp_files


def add_log_registrations(file_path):
    """Add LOG_REGISTER_CLASS to class definitions in a file."""
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()

        original_content = content

        # Pattern to match class definitions
        # Matches: class ClassName { or class ClassName : public BaseClass {
        class_pattern = r"class\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?::\s*public\s+[A-Za-z_][A-Za-z0-9_]*\s*)?{"

        # Find all class definitions
        matches = list(re.finditer(class_pattern, content))

        # Process matches in reverse order to avoid offset issues
        for match in reversed(matches):
            class_name = match.group(1)
            start_pos = match.start()

            # Check if LOG_REGISTER_CLASS is already present before this class
            before_class = content[:start_pos]
            if f'LOG_REGISTER_CLASS("{class_name}")' not in before_class:
                # Add LOG_REGISTER_CLASS before the class definition
                registration_line = f'LOG_REGISTER_CLASS("{class_name}")\n'
                content = content[:start_pos] + registration_line + content[start_pos:]

        # Only write if content changed
        if content != original_content:
            with open(file_path, "w", encoding="utf-8") as f:
                f.write(content)
            print(f"Updated: {file_path}")
            return True
        else:
            print(f"No changes needed: {file_path}")
            return False

    except Exception as e:
        print(f"Error processing {file_path}: {e}")
        return False


def remove_manual_tags(file_path):
    """Remove manual [ClassName] tags from LOG_* calls."""
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()

        original_content = content

        # Pattern to match LOG_* calls with manual class tags
        # Matches: LOG_INFO("[ClassName] message") or LOG_DEBUG("[ClassName] message")
        log_pattern = r'(LOG_(DEBUG|INFO|WARN|ERROR))\s*\(\s*"\[([A-Za-z_][A-Za-z0-9_]*)\]\s*([^"]*)"'

        def replace_log_call(match):
            log_macro = match.group(1)
            log_level = match.group(2)
            class_name = match.group(3)
            message = match.group(4)
            return f'{log_macro}("{message}"'

        content = re.sub(log_pattern, replace_log_call, content)

        # Only write if content changed
        if content != original_content:
            with open(file_path, "w", encoding="utf-8") as f:
                f.write(content)
            print(f"Removed manual tags from: {file_path}")
            return True
        else:
            print(f"No manual tags found in: {file_path}")
            return False

    except Exception as e:
        print(f"Error processing {file_path}: {e}")
        return False


def main():
    if len(sys.argv) < 2:
        print("Usage: python add_log_registrations.py <directory> [--remove-tags]")
        print("  <directory> - Directory containing C++ files")
        print("  --remove-tags - Also remove manual [ClassName] tags from LOG_* calls")
        sys.exit(1)

    directory = sys.argv[1]
    remove_tags = "--remove-tags" in sys.argv

    if not os.path.exists(directory):
        print(f"Directory not found: {directory}")
        sys.exit(1)

    print(f"Scanning directory: {directory}")
    cpp_files = find_cpp_files(directory)
    print(f"Found {len(cpp_files)} C++ files")

    updated_files = 0

    for file_path in cpp_files:
        # Skip the Log.h file itself
        if "Log.h" in file_path:
            print(f"Skipping: {file_path}")
            continue

        if add_log_registrations(file_path):
            updated_files += 1

        if remove_tags and remove_manual_tags(file_path):
            updated_files += 1

    print(f"\nSummary: Updated {updated_files} files")
    print("\nNext steps:")
    print("1. Review the changes to ensure they're correct")
    print("2. Test your code to make sure it compiles")
    print("3. Check that the logging output looks correct")


if __name__ == "__main__":
    main()
