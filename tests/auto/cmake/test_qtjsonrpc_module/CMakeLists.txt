# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 2.8)

project(test_qtjsonrpc_module)

find_package(Qt6JsonRpcPrivate REQUIRED)

add_executable(mainapp main.cpp)
target_link_libraries(mainapp Qt6::JsonRpcPrivate)
