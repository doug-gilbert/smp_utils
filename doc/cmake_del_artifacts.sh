#!/bin/sh

echo "Entering doc/cmake_del_artifacts.sh"

rm -rf \
	CMakeFiles \
	cmake_install.cmake \
	*.8.gz \
	Makefile

