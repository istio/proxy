#!/bin/sh

echo "TARGET $TARGET" >> $OUTPUT
echo "CONFIG_MODE $CONFIG_MODE" >> $OUTPUT
echo "STAMPED $STAMPED" >> $OUTPUT
if [ -n "$VERSION_FILE" ]; then
  cat "$VERSION_FILE" >> "$OUTPUT"
fi
if [ -n "$INFO_FILE" ]; then
  cat "$INFO_FILE" >> "$OUTPUT"
fi
exit 0
