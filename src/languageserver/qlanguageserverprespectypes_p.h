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
#ifndef QLANGUAGESERVERPRESPECTYPES_P_H
#define QLANGUAGESERVERPRESPECTYPES_P_H

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

#include <QtJsonRpc/private/qtypedjson_p.h>
#include <QtJsonRpc/private/qjsontypedrpc_p.h>
#include <QtLanguageServer/qtlanguageserverglobal.h>
#include <QtCore/QByteArray>
#include <QtCore/QMetaEnum>
#include <QtCore/QString>

#include <variant>
#include <type_traits>

QT_BEGIN_NAMESPACE
namespace QLspSpecification {

using ProgressToken = std::variant<int, QByteArray>;

class Q_LANGUAGESERVER_EXPORT StringAndLanguage
{
public:
    QString language;
    QString value;

    template<typename W>
    void walk(W &w)
    {
        field(w, "language", language);
        field(w, "value", value);
    }
};

using MarkedString = std::variant<QByteArray, StringAndLanguage>;

template<typename RType>
class LSPResponse : public QJsonRpc::TypedResponse
{
    Q_DISABLE_COPY(LSPResponse)
public:
    LSPResponse() = default;
    LSPResponse(LSPResponse &&o) noexcept = default;
    LSPResponse &operator=(LSPResponse &&o) noexcept = default;
    using ResponseType = RType;

    LSPResponse(QJsonRpc::TypedResponse &&r) : QJsonRpc::TypedResponse(std::move(r)) { }

    void sendResponse(const RType &r) { sendSuccessfullResponse(r); }
    auto sendResponse()
    {
        if constexpr (std::is_same_v<std::decay_t<RType>, std::nullptr_t>)
            sendSuccessfullResponse(nullptr);
        else
            Q_ASSERT(false);
    }
};

template<typename RType, typename PRType>
class LSPPartialResponse : public LSPResponse<RType>
{
    Q_DISABLE_COPY(LSPPartialResponse)
public:
    using PartialResponseType = PRType;

    LSPPartialResponse() = default;
    LSPPartialResponse(LSPPartialResponse &&o) noexcept = default;
    LSPPartialResponse &operator=(LSPPartialResponse &&o) noexcept = default;

    LSPPartialResponse(QJsonRpc::TypedResponse &&r) : LSPResponse<RType>(std::move(r)) { }

    void sendPartialResponse(const PRType &r)
    {
        // using Notifications::Progress here would require to split out the *RequestType aliases
        sendNotification("$/progress", r);
    }
};

} // namespace QLspSpecification
QT_END_NAMESPACE
#endif // QLANGUAGESERVERPRESPECTYPES_P_H
