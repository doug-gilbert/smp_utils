#!/bin/sh

echo "Entering lib/cmake_del_artifacts.sh"

rm -rf \
	CMakeFiles \
	cmake_install.cmake \
	smputils1Config.cmake \
	smputils1ConfigVersion.cmake \
	Makefile

