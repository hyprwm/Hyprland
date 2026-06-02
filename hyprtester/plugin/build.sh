#!/bin/sh

make clean
make all LUA_INCLUDES="${1}"
