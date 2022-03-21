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

#ifndef QLANGUAGESERVERBASE_P_H
#define QLANGUAGESERVERBASE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtLanguageServer/qtlanguageserverglobal.h>
#include <QtLanguageServer/private/qlanguageserverspec_p.h>
#include <QtJsonRpc/private/qjsonrpcprotocol_p.h>
#include <QtJsonRpc/private/qjsonrpctransport_p.h>

#include <QtCore/QByteArray>

#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE

namespace QLspSpecification {

Q_DECLARE_LOGGING_CATEGORY(lspLog);

class ProtocolBasePrivate;
class Q_LANGUAGESERVER_EXPORT ProtocolBase
{
    Q_DISABLE_COPY_MOVE(ProtocolBase)
public:
    ~ProtocolBase();
    using GenericRequestHandler = std::function<void(const QJsonRpc::IdType &, const QByteArray &,
                                                     const QLspSpecification::RequestParams &,
                                                     QJsonRpc::TypedResponse &&)>;
    using GenericNotificationHandler =
            std::function<void(const QByteArray &, const QLspSpecification::NotificationParams &)>;
    using ResponseErrorHandler = std::function<void(const QLspSpecification::ResponseError &)>;

    // generated, defined in qlanguageservergen.cpp
    static QByteArray requestMethodToBaseCppName(const QByteArray &);

    // generated, defined in qlanguageservergen.cpp
    static QByteArray notificationMethodToBaseCppName(const QByteArray &);

    static void defaultUndispatchedRequestHandler(
            const QJsonRpc::IdType &id, const QByteArray &method,
            const QLspSpecification::RequestParams &params, QJsonRpc::TypedResponse &&response);
    static void defaultUndispatchedNotificationHandler(
            const QByteArray &method, const QLspSpecification::NotificationParams &params);
    static void defaultResponseErrorHandler(const QLspSpecification::ResponseError &err);

    void registerResponseErrorHandler(const ResponseErrorHandler &h);
    void registerUndispatchedRequestHandler(const GenericRequestHandler &handler);
    void registerUndispatchedNotificationHandler(const GenericNotificationHandler &handler);

    void handleResponseError(const ResponseError &err);
    void handleUndispatchedRequest(const QJsonRpc::IdType &id, const QByteArray &method,
                                   const QLspSpecification::RequestParams &params,
                                   QJsonRpc::TypedResponse &&response);
    void handleUndispatchedNotification(const QByteArray &method,
                                        const QLspSpecification::NotificationParams &params);
    QJsonRpc::TypedRpc *typedRpc();

protected:
    std::unique_ptr<ProtocolBasePrivate> d_ptr;

    ProtocolBase(std::unique_ptr<ProtocolBasePrivate> &&priv);
    QJsonRpcTransport *transport();

private:
    void registerMethods(QJsonRpc::TypedRpc *);
    Q_DECLARE_PRIVATE(ProtocolBase)
};

template<typename T, typename F>
void decodeAndCall(QJsonValue value, F funct,
                   ProtocolBase::ResponseErrorHandler errorHandler =
                           &ProtocolBase::defaultResponseErrorHandler)
{
    using namespace Qt::StringLiterals;

    T result;
    QTypedJson::Reader r(value);
    doWalk(r, result);
    if (!r.errorMessages().isEmpty()) {
        errorHandler(QLspSpecification::ResponseError {
                int(QLspSpecification::ErrorCodes::ParseError),
                u"Errors decoding data:\n    %1"_s.arg(r.errorMessages().join(u"\n    ")).toUtf8(),
                value });
        r.clearErrorMessages();
    } else {
        if constexpr (std::is_same_v<T, std::nullptr_t> && std::is_invocable_v<F>) {
            funct();
        } else {
            funct(result);
        }
    }
}

} // namespace QLspSpecification
QT_END_NAMESPACE
#endif // QLANGUAGESERVERBASE_P_H
