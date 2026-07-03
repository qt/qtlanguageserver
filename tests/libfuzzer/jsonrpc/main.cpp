// Copyright (C) 2020 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QtJsonRpc/private/qjsonrpcprotocol_p.h>
#include <QtJsonRpc/private/qhttpmessagestreamparser_p.h>

extern "C" int LLVMFuzzerTestOneInput(const char *Data, size_t Size)
{
    QByteArray content = QByteArray::fromRawData(Data, Size);
    int warnings = 0;
    const auto headerHandler = [](const QByteArray &, const QByteArray &) { };
    const auto diagnosticHandler = [&](QtMsgType error, QString msg) {
        ++warnings;
        qDebug() << "Warning Type:" << error << "-- Message:" << msg;
    };
    const auto bodyHandler = [&](const QByteArray &body) {
        QJsonParseError error = { 0, QJsonParseError::NoError };
        const QJsonDocument doc = QJsonDocument::fromJson(body, &error);
        if (error.error != QJsonParseError::NoError) {
            diagnosticHandler(QtDebugMsg, QString("Errors in JSON parsing: %1").arg(QString::fromUtf8(body)));
        }
    };
    QHttpMessageStreamParser parser(
            headerHandler, bodyHandler, diagnosticHandler);

    // 
    parser.receiveData(content);
    return 0;
}
