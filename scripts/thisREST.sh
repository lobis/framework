#!/bin/bash

REST_DIR=$(cd "$(dirname "$0")"; pwd)
if [ ! -s $REST_DIR/lib/libRestCore.so ]; then
    REST_DIR=$(cd "$(dirname "$BASH_SOURCE")"; pwd)
fi

if [ ! -s "$REST_DIR/lib/libRestCore.so" ]; then
    echo "Can not find REST path."
	exit 1
fi

echo "                                                    "
echo "  **************************************************"
echo "              W E L C O M E   to  R E S T           "
echo "                  Environmental set-up              "
echo "                                                    "
echo "        Version : untagged     5th August 2016      "
echo "                                                    "
echo "        REST_PATH = $REST_PATH                      "
echo "                                                    "
echo "        Use restRoot command to launch ROOT with    "
echo "        REST libraries and scripts loaded           "
echo "                                                    "
echo "  **************************************************"
echo "                                                    "

alias restRoot="root -l $REST_DIR/LoadRESTScripts.C"

export REST_PATH=$REST_DIR
export PATH=$PATH:$REST_DIR/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$REST_DIR/lib
export LIBRARY_PATH=$LIBRARY_PATH:$REST_DIR/lib
