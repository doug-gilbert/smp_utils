#!/bin/sh

cd include
./cmake_del_artifacts.sh
cd ..

cd lib
./cmake_del_artifacts.sh
cd ..

cd src
./cmake_del_artifacts.sh
cd ..

cd doc
./cmake_del_artifacts.sh
cd ..

rm -rf \
	build \
	CMakeCache.txt \
	CMakeFiles \
	CPackConfig.cmake \
	CPackSourceConfig.cmake \
	CMakeFiles \
	cmake_install.cmake \
	config.h \
	install_manifest.txt \
	_CPack_Packages \
	CTestTestfile.cmake \
	DartConfiguration.tcl \
	install_manifest_development.txt \
	install_manifest_runtime.txt \
	install_manifest_utilities.txt \
	Testing \
	Makefile

