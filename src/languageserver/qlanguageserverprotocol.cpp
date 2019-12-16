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

#include <QtJsonRpc/private/qjsonrpcprotocol_p.h>
#include <QtLanguageServer/private/qlanguageserverprotocol_p.h>
#include <QtLanguageServer/private/qlanguageservergen_p_p.h>

#include <memory>

QT_BEGIN_NAMESPACE

using namespace QLspSpecification;

/*!
\internal
\class QLanguageServerProtocol
\brief Implements the language server protocol

QLanguageServerProtocol objects handles the language server
protocol both to send and receive (ask the client and reply to the
client).

To ask the client you use the requestXX or notifyXX methods.
To reply to client request you have to register your handlers via
registerXXRequestHandler, notifications can be handled connecting the
receivedXXNotification signals.

The method themselves are implemented in qlanguageservergen*
files which are generated form the specification.

You have to provide a function to send the data that the protocol
generates to the constructor, and you have to feed the data you
receive to it via the receivedData method.

Limitations: for the client use case (Creator),the handling of partial
results could be improved. A clean solution should handle the progress
notification, do some extra tracking and give the partial results back
to the call that did the request.
*/

QLanguageServerProtocol::QLanguageServerProtocol(const QJsonRpcTransport::DataHandler &sender)
    : ProtocolGen(std::make_unique<ProtocolGenPrivate>())
{
    transport()->setDataHandler(sender);
}

void QLanguageServerProtocol::receiveData(const QByteArray &data)
{
    transport()->receiveData(data);
}

QT_END_NAMESPACE
