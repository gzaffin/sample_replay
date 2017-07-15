#!/bin/sh

# Launcher script to work around limitations in sample_replay:
#   OSS only - detects oss wrapper and allows explicit selection
# Original script by ZekeSulastin

exec /usr/bin/padsp -n "sample_replay" -m "OSS Emulation" ~/sample_replay/build/sample_replay "$1"
