# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

if(NOT QT_BUILD_STANDALONE_TESTS AND NOT QT_BUILDING_QT)
    cmake_minimum_required(VERSION 3.16)
    project(tst_qlanguageserver LANGUAGES CXX)
    find_package(Qt6BuildInternals REQUIRED COMPONENTS STANDALONE_TEST)
endif()

#####################################################################
## tst_qlanguageserver Test:
#####################################################################

qt_internal_add_test(tst_qlanguageserver
    SOURCES
        tst_qlanguageserver.cpp
        qiopipe.h qiopipe.cpp
    DEFINES
        QT_DEPRECATED_WARNINGS
    LIBRARIES
        Qt::CorePrivate
        Qt::LanguageServerPrivate
        Qt::Test
    TESTDATA ${test_data}
)
