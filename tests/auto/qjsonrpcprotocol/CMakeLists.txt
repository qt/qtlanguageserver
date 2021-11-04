#####################################################################
## tst_jsonrpc Test:
#####################################################################
qt_internal_add_test(tst_qjsonrpcprotocol
    SOURCES
        tst_qjsonrpcprotocol.cpp
    DEFINES
        QT_DEPRECATED_WARNINGS
    PUBLIC_LIBRARIES
        Qt::JsonRpcPrivate
        Qt::Test
    TESTDATA ${test_data}
)
