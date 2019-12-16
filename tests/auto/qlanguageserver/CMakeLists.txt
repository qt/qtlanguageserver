#####################################################################
## tst_qlanguageserver Test:
#####################################################################

qt_internal_add_test(tst_qlanguageserver
    SOURCES
        tst_qlanguageserver.cpp
        qiopipe.h qiopipe.cpp
    DEFINES
        QT_DEPRECATED_WARNINGS
    PUBLIC_LIBRARIES
        Qt::CorePrivate
        Qt::LanguageServerPrivate
        Qt::Test
    TESTDATA ${test_data}
)
