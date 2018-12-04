#!/bin/bash

# Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
#
# Published under Apache 2.0 license.
# See LICENSE for details.


# =============================================================================
# "So it begins..." ===========================================================

echo "========= Building dg."

# Build dg
cd dg
rm -rf build
mkdir build
cd build
cmake ..
make -j9 # compiling really fast here dude
cd ../..

echo "========= Building apex."

# Build APEX
rm -rf build
mkdir build
cd build
#cmake .. -LAH # verbose
cmake ..
make -j9
cd ..