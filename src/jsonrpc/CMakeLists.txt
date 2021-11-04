#####################################################################
## JsonRpc Module:
#####################################################################

qt_internal_add_module(JsonRpc
    SOURCES
        qjsonrpcprotocol.cpp qjsonrpcprotocol_p.h qjsonrpcprotocol_p_p.h
        qjsonrpctransport_p.h
        qtjsonrpcglobal.h
    DEFINES
        QT_BUILD_JSONRPC_LIB
    PUBLIC_LIBRARIES
        Qt::Core
)
