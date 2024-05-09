#!/bin/bash

# Helper variables:
STANDARD="-std=c++11"
OPTIMIZATION="-O2"
INCLUDES="-Imodules"
WARNINGS="-Wall -Wextra -Wpedantic -Wconversion -Wshadow"
EXTRA_OPTIONS="-fno-exceptions"

# Actual build commands:
g++ $STANDARD $OPTIMIZATION $INCLUDES $WARNINGS $EXTRA_OPTIONS main.cpp -o .build/main
