# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#####################################################################
## tst_jsonrpc Test:
#####################################################################
qt_internal_add_test(tst_qjsonrpcprotocol
    SOURCES
        tst_qjsonrpcprotocol.cpp
    DEFINES
        QT_DEPRECATED_WARNINGS
    LIBRARIES
        Qt::JsonRpcPrivate
        Qt::Test
    TESTDATA ${test_data}
)
