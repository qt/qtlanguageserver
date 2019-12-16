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

#include "qlanguageserverbase_p_p.h"

QT_BEGIN_NAMESPACE

namespace QLspSpecification {

Q_LOGGING_CATEGORY(lspLog, "qt.languageserver.protocol");

ProtocolBase::ProtocolBase(std::unique_ptr<ProtocolBasePrivate> &&priv) : d_ptr(std::move(priv))
{
    Q_D(ProtocolBase);
    d->typedRpc.setTransport(&d->transport);
    registerMethods(&d->typedRpc);
}

ProtocolBase::~ProtocolBase() = default;

void ProtocolBase::registerMethods(QJsonRpc::TypedRpc *typedRpc)
{
    auto defaultHandler = new QJsonRpc::TypedHandler(
            QByteArray(),
            [this, typedRpc](const QJsonRpcProtocol::Request &req,
                             const QJsonRpcProtocol::ResponseHandler &handler) {
                QJsonRpc::IdType id =
                        ((req.id.isDouble()) ? QJsonRpc::IdType(req.id.toInt())
                                             : QJsonRpc::IdType(req.id.toString().toUtf8()));
                QByteArray method = req.method.toUtf8();
                QJsonRpc::TypedResponse response(id, typedRpc, handler);
                handleUndispatchedRequest(id, method, req.params, std::move(response));
            },
            [this](const QJsonRpcProtocol::Notification &notif) {
                QByteArray method = notif.method.toUtf8();
                handleUndispatchedNotification(method, notif.params);
            });
    typedRpc->setDefaultMessageHandler(defaultHandler); // typedRpc gets ownership
    typedRpc->setInvalidResponseHandler([this](const QJsonRpcProtocol::Response &response) {
        handleResponseError(ResponseError { response.errorCode.toInt(),
                                            response.errorMessage.toUtf8(), response.data });
    });
}

void ProtocolBase::defaultUndispatchedRequestHandler(const QJsonRpc::IdType &id,
                                                     const QByteArray &method,
                                                     const QLspSpecification::RequestParams &params,
                                                     QJsonRpc::TypedResponse &&response)
{
    Q_UNUSED(id);
    Q_UNUSED(params);
    QByteArray msg;
    QByteArray cppBaseName = requestMethodToBaseCppName(method);
    if (cppBaseName.isEmpty()) {
        msg.append("Ignoring unknown request with method ");
        msg.append(method);
    } else {
        msg.append("There was no handler registered with register");
        msg.append(cppBaseName);
        msg.append("Handler to handle a requests with method ");
        msg.append(method);
    }
    response.sendErrorResponse(int(QJsonRpcProtocol::ErrorCode::MethodNotFound), msg);
    qCWarning(lspLog) << QString::fromUtf8(msg);
}

void ProtocolBase::defaultUndispatchedNotificationHandler(
        const QByteArray &method, const QLspSpecification::NotificationParams &params)
{
    Q_UNUSED(params);
    QByteArray msg;
    QByteArray cppBaseName = notificationMethodToBaseCppName(method);
    if (cppBaseName.isEmpty()) {
        msg.append("Unknown notification with method ");
        msg.append(method);
    } else {
        msg.append("There was not handler registered with register");
        msg.append(cppBaseName);
        msg.append("NotificationHandler to handle notification with method ");
        msg.append(method);
    }
    if (method.startsWith("$"))
        qCDebug(lspLog) << QString::fromUtf8(msg);
    else
        qCWarning(lspLog) << QString::fromUtf8(msg);
}

void ProtocolBase::defaultResponseErrorHandler(const QLspSpecification::ResponseError &err)
{
    qCWarning(lspLog) << u"ERROR" << err.code << u":" << QString::fromUtf8(err.message)
                      << ((!err.data) ? QString()
                                  : (err.data->isObject())
                                  ? QString::fromUtf8(QJsonDocument(err.data->toObject()).toJson())
                                  : (err.data->isArray())
                                  ? QString::fromUtf8(QJsonDocument(err.data->toArray()).toJson())
                                  : (err.data->isDouble()) ? QString::number(err.data->toDouble())
                                  : (err.data->isString()) ? err.data->toString()
                                  : (err.data->isNull())   ? u"null"_qs
                                                           : QString());
}

void ProtocolBase::registerResponseErrorHandler(const ResponseErrorHandler &handler)
{
    Q_D(ProtocolBase);
    Q_ASSERT(!d->errorHandler || !handler);
    d->errorHandler = handler;
}

void ProtocolBase::registerUndispatchedRequestHandler(const GenericRequestHandler &handler)
{
    Q_D(ProtocolBase);
    Q_ASSERT(!d->undispachedRequestHandler || !handler);
    d->undispachedRequestHandler = handler;
}

void ProtocolBase::registerUndispatchedNotificationHandler(
        const GenericNotificationHandler &handler)
{
    Q_D(ProtocolBase);
    Q_ASSERT(!d->undispachedNotificationHandler || !handler);
    d->undispachedNotificationHandler = handler;
}

void ProtocolBase::handleUndispatchedRequest(const QJsonRpc::IdType &id, const QByteArray &method,
                                             const QLspSpecification::RequestParams &params,
                                             QJsonRpc::TypedResponse &&response)
{
    Q_D(ProtocolBase);
    if (d->undispachedRequestHandler)
        d->undispachedRequestHandler(id, method, params, std::move(response));
    else
        defaultUndispatchedRequestHandler(id, method, params, std::move(response));
}

void ProtocolBase::handleUndispatchedNotification(const QByteArray &method,
                                                  const NotificationParams &params)
{
    Q_D(ProtocolBase);
    if (d->undispachedNotificationHandler)
        d->undispachedNotificationHandler(method, params);
    else
        defaultUndispatchedNotificationHandler(method, params);
}

void ProtocolBase::handleResponseError(const ResponseError &err)
{
    Q_D(ProtocolBase);
    if (d->errorHandler)
        d->errorHandler(err);
    else
        defaultResponseErrorHandler(err);
}

QJsonRpc::TypedRpc *ProtocolBase::typedRpc()
{
    Q_D(ProtocolBase);
    return &d->typedRpc;
}

QJsonRpcTransport *ProtocolBase::transport()
{
    Q_D(ProtocolBase);
    return &d->transport;
}

} // namespace QLspSpecification
QT_END_NAMESPACE
