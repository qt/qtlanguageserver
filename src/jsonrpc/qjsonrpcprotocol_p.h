/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtLanguageServer module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QJSONRPCPROTOCOL_P_H
#define QJSONRPCPROTOCOL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtJsonRpc/qtjsonrpcglobal.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qjsondocument.h>

#include <memory>
#include <thread>

QT_BEGIN_NAMESPACE

class QJsonRpcTransport;
class QJsonRpcProtocolPrivate;
class Q_JSONRPC_EXPORT QJsonRpcProtocol
{
    Q_DISABLE_COPY(QJsonRpcProtocol)
public:
    QJsonRpcProtocol();
    QJsonRpcProtocol(QJsonRpcProtocol &&) noexcept;
    QJsonRpcProtocol &operator=(QJsonRpcProtocol &&) noexcept;

    ~QJsonRpcProtocol();

    enum class ErrorCode {
        ParseError = -32700,
        InvalidRequest = -32600,
        MethodNotFound = -32601,
        InvalidParams = -32602,
        InternalError = -32603,
    };

    struct Request
    {
        QJsonValue id = QJsonValue::Undefined;
        QString method;
        QJsonValue params = QJsonValue::Undefined;
    };

    struct Response
    {
        // ID is disregarded on responses generated from MessageHandlers.
        // You cannot reply to a request you didn't handle. The original request ID is used instead.
        QJsonValue id = QJsonValue::Undefined;

        // In case of !errorCode.isDouble(), data is the "data" member of the error object.
        // Otherwise it's the "result" member of the base response.
        QJsonValue data = QJsonValue::Undefined;

        // Error codes from and including -32768 to -32000 are reserved for pre-defined errors.
        // Don't use them for your own protocol. We cannot enforce this here.
        QJsonValue errorCode = QJsonValue::Undefined;

        QString errorMessage;
    };

    template<typename T>
    using Handler = std::function<void(const T &)>;

    struct Notification
    {
        QString method;
        QJsonValue params = QJsonValue::Undefined;
    };

    class Q_JSONRPC_EXPORT MessageHandler
    {
        Q_DISABLE_COPY_MOVE(MessageHandler)
    public:
        using ResponseHandler = std::function<void(const Response &)>;

        MessageHandler();
        virtual ~MessageHandler();
        virtual void handleRequest(const Request &request, const ResponseHandler &handler);
        virtual void handleNotification(const Notification &notification);

    protected:
        static Response error(ErrorCode code);
        static Response error(int code, const QString &message,
                              const QJsonValue &data = QJsonValue::Undefined);
        static Response result(const QJsonValue &result);

        template<typename Function>
        void asynchronous(Function processor, const Request &request,
                          const ResponseHandler &handler)
        {
            std::thread([processor, request, handler]() { handler(processor(request)); }).detach();
        }

        template<typename Function>
        void asynchronous(Function processor, const Notification &notification)
        {
            std::thread([processor, notification]() { processor(notification); }).detach();
        }
    };

    class BatchPrivate;
    class Q_JSONRPC_EXPORT Batch
    {
        Q_DISABLE_COPY(Batch)
    public:
        Batch();
        ~Batch();

        Batch(Batch &&) noexcept;
        Batch &operator=(Batch &&) noexcept;

        void addNotification(const Notification &notification);
        void addRequest(const Request &request);

    private:
        friend class QJsonRpcProtocol;
        std::unique_ptr<BatchPrivate> d;
    };

    void setMessageHandler(const QString &method, MessageHandler *handler);
    void setDefaultMessageHandler(MessageHandler *handler);
    MessageHandler *messageHandler(const QString &method) const;
    MessageHandler *defaultMessageHandler() const;

    void sendRequest(const Request &request, const QJsonRpcProtocol::Handler<Response> &handler);
    void sendNotification(const Notification &notification);
    void sendBatch(Batch &&batch, const QJsonRpcProtocol::Handler<Response> &handler);

    void setTransport(QJsonRpcTransport *transport);

    // For id:null responses
    using ResponseHandler = std::function<void(const Response &)>;
    void setProtocolErrorHandler(const ResponseHandler &handler);
    ResponseHandler protocolErrorHandler() const;

    // For responses with unknown IDs
    void setInvalidResponseHandler(const ResponseHandler &handler);
    ResponseHandler invalidResponseHandler() const;

private:
    std::unique_ptr<QJsonRpcProtocolPrivate> d;
};

QT_END_NAMESPACE

#endif // QJSONRPCPROTOCOL_P_H
