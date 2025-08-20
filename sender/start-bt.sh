#!/bin/bash

TARGET_FILE="/etc/bluetooth/main.conf"

# The pattern to search for. Lines will be inserted *after* this pattern.
SEARCH_PATTERN="\[General\]" # [General] needs to be escaped because [] are special characters in regex.

# The content you want to insert.
# Use '\n' for new lines within the content.
# Ensure each line you want to insert is properly escaped if it contains special characters
# or if it needs to be treated as a literal string by sed.
# For example, to insert "Key1=Value1" and "Key2=Value2":
LINES_TO_INSERT="Name = bt-codec2-sender\nClass = 0x200414\nDiscoverableTimeout = 0\nEnable = Source,Sink,Media,Socket"

# Check if the target file exists
if [ ! -f "$TARGET_FILE" ]; then
    echo "Error: File '$TARGET_FILE' not found."
    exit 1
fi

# Check if the pattern exists in the file
if ! grep -q "$SEARCH_PATTERN" "$TARGET_FILE"; then
    echo "Error: Pattern '$SEARCH_PATTERN' not found in '$TARGET_FILE'."
    echo "No lines were added."
    exit 1
fi

# Check if those lines exists in the file
if ! grep -q "$LINES_TO_INSERT" "$TARGET_FILE"; then
    TIMESTAMP=$(date +%F_%H-%M-%S)
    cp "$TARGET_FILE" "$TARGET_FILE.$TIMESTAMP"

    sed -i "/$SEARCH_PATTERN/a\\
    $LINES_TO_INSERT" "$TARGET_FILE"
fi

systemctl restart bluetooth

echo power on | bluetoothctl && echo discoverable on | bluetoothctl

# python bt-agent.py



