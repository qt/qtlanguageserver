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

#ifndef QJSONRPCPROTOCOL_P_P_H
#define QJSONRPCPROTOCOL_P_P_H

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

#include <QtJsonRpc/private/qjsonrpcprotocol_p.h>
#include <QtJsonRpc/private/qjsonrpctransport_p.h>

#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

#include <unordered_map>
#include <memory>
#include <mutex>

QT_BEGIN_NAMESPACE

template<typename T>
struct QHasher
{
    using argument_type = T;
    using result_type = size_t;
    result_type operator()(const argument_type &value) const { return qHash(value); }
};

class QJsonRpcProtocolPrivate
{
public:
    template<typename K, typename V>
    using Map = std::unordered_map<K, V, QHasher<K>>;

    using ResponseHandler = QJsonRpcProtocol::ResponseHandler;
    using MessageHandler = QJsonRpcProtocol::MessageHandler;
    using OwnedMessageHandler = std::unique_ptr<MessageHandler>;
    using MessageHandlerMap = Map<QString, OwnedMessageHandler>;
    using ResponseMap = Map<QJsonValue, QJsonRpcProtocol::Handler<QJsonRpcProtocol::Response>>;

    void processMessage(const QJsonDocument &message, const QJsonParseError &error);
    void processError(const QString &error);

    template<typename JSON>
    void sendMessage(const JSON &value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_transport->sendMessage(QJsonDocument(value));
    }

    void processRequest(const QJsonObject &object);
    void processResponse(const QJsonObject &object);
    void processNotification(const QJsonObject &object);

    MessageHandler *messageHandler(const QString &method) const
    {
        auto it = m_messageHandlers.find(method);
        return it != m_messageHandlers.end() ? it->second.get() : m_defaultHandler.get();
    }

    void setMessageHandler(const QString &method, OwnedMessageHandler handler)
    {
        m_messageHandlers[method] = std::move(handler);
    }

    MessageHandler *defaultMessageHandler() const { return m_defaultHandler.get(); }
    void setDefaultMessageHandler(OwnedMessageHandler handler)
    {
        m_defaultHandler = std::move(handler);
    }

    bool addPendingRequest(const QJsonValue &id,
                           const QJsonRpcProtocol::Handler<QJsonRpcProtocol::Response> &handler)
    {
        auto it = m_pendingRequests.find(id);
        if (it == m_pendingRequests.end()) {
            m_pendingRequests.insert(std::make_pair(id, handler));
            return true;
        }
        return false;
    }

    void setTransport(QJsonRpcTransport *newTransport);
    QJsonRpcTransport *transport() const { return m_transport; }

    ResponseHandler invalidResponseHandler() const { return m_invalidResponseHandler; }
    void setInvalidResponseHandler(const ResponseHandler &handler)
    {
        m_invalidResponseHandler = handler;
    }

    ResponseHandler protocolErrorHandler() const { return m_protocolErrorHandler; }
    void setProtocolErrorHandler(const ResponseHandler &handler)
    {
        m_protocolErrorHandler = handler;
    }

    std::mutex *mutex() { return &m_mutex; }
    const std::mutex *mutex() const { return &m_mutex; }

    QJsonRpcProtocol::MessagePreprocessor messagePreprocessor() const
    {
        return m_messagePreprocessor;
    }
    void installMessagePreprocessor(QJsonRpcProtocol::MessagePreprocessor preHandler)
    {
        m_messagePreprocessor = preHandler;
    }

private:
    ResponseMap m_pendingRequests;
    MessageHandlerMap m_messageHandlers;

    OwnedMessageHandler m_defaultHandler;

    QJsonRpcTransport *m_transport;
    std::mutex m_mutex;

    ResponseHandler m_protocolErrorHandler;
    ResponseHandler m_invalidResponseHandler;
    QJsonRpcProtocol::MessagePreprocessor m_messagePreprocessor;
};

class QJsonRpcProtocol::BatchPrivate
{
public:
    struct Item
    {
        QJsonValue id = QJsonValue::Undefined;
        QString method;
        QJsonValue params = QJsonValue::Undefined;
    };

    std::vector<Item> m_items;
};

QT_END_NAMESPACE

#endif // QJSONRPCPROTOCOL_P_P_H
