# Copyright 2026
# SPDX-License-Identifier: BSL-1.0

# Can be built as part of MeshLab (MESHLAB_BUILD_TESTS=ON) or standalone:
#   cmake -S tests -B build-tests && cmake --build build-tests

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	cmake_minimum_required(VERSION 3.18)
	project(MeshLabLayerTests LANGUAGES CXX)
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	set(CMAKE_AUTOMOC ON)
	enable_testing()
endif()

find_package(Qt5 COMPONENTS Widgets Test QUIET)
if (NOT Qt5Test_FOUND)
	message(WARNING "Qt5 Test not found; skipping MeshLab unit tests")
	return()
endif()

set(MESHLAB_LAYER_TREE_SRC "${CMAKE_CURRENT_LIST_DIR}/../src/meshlab")

add_executable(layer_mesh_tree_test
	layer_mesh_tree_test.cpp
	${MESHLAB_LAYER_TREE_SRC}/layer_mesh_tree.cpp
	${MESHLAB_LAYER_TREE_SRC}/layer_mesh_tree.h)

set_target_properties(layer_mesh_tree_test PROPERTIES
	AUTOMOC ON
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON)
target_include_directories(layer_mesh_tree_test PRIVATE ${MESHLAB_LAYER_TREE_SRC})
target_link_libraries(layer_mesh_tree_test PRIVATE Qt5::Widgets Qt5::Test)

add_test(NAME layer_mesh_tree_test COMMAND layer_mesh_tree_test)
set_tests_properties(layer_mesh_tree_test PROPERTIES
	ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(layer_dialog_risk_test
	layer_dialog_risk_test.cpp
	${MESHLAB_LAYER_TREE_SRC}/layer_mesh_tree.cpp
	${MESHLAB_LAYER_TREE_SRC}/layer_mesh_tree.h)

set_target_properties(layer_dialog_risk_test PROPERTIES
	AUTOMOC ON
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON)
target_include_directories(layer_dialog_risk_test PRIVATE ${MESHLAB_LAYER_TREE_SRC})
target_compile_definitions(layer_dialog_risk_test PRIVATE
	MESHLAB_SRC_MESHLAB="${MESHLAB_LAYER_TREE_SRC}")
target_link_libraries(layer_dialog_risk_test PRIVATE Qt5::Widgets Qt5::Test)

add_test(NAME layer_dialog_risk_test COMMAND layer_dialog_risk_test)
set_tests_properties(layer_dialog_risk_test PROPERTIES
	ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
