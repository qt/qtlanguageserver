/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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

#ifndef QJSONRPCTRANSPORT_H
#define QJSONRPCTRANSPORT_H

#include <QtJsonRpc/qtjsonrpcglobal.h>
#include <QtCore/qjsondocument.h>
#include <functional>

QT_BEGIN_NAMESPACE

class Q_JSONRPC_EXPORT QJsonRpcTransport
{
    Q_DISABLE_COPY_MOVE(QJsonRpcTransport)
public:
    enum DiagnosticLevel { Warning, Error };

    using MessageHandler = std::function<void(const QJsonDocument &, const QJsonParseError &)>;
    using DataHandler = std::function<void(const QByteArray &)>;
    using DiagnosticHandler = std::function<void(DiagnosticLevel, const QString &)>;

    QJsonRpcTransport() = default;
    virtual ~QJsonRpcTransport() = default;

    // Parse data and call messageHandler for any messages found in it.
    virtual void receiveData(const QByteArray &data) = 0;

    // serialize the message and call dataHandler for the resulting data.
    // Needs to be guarded by a mutex if called  by different threads
    virtual void sendMessage(const QJsonDocument &packet) = 0;

    void setMessageHandler(const MessageHandler &handler) { m_messageHandler = handler; }
    MessageHandler messageHandler() const { return m_messageHandler; }

    void setDataHandler(const DataHandler &handler) { m_dataHandler = handler; }
    DataHandler dataHandler() const { return m_dataHandler; }

    void setDiagnosticHandler(const DiagnosticHandler &handler) { m_diagnosticHandler = handler; }
    DiagnosticHandler diagnosticHandler() const { return m_diagnosticHandler; }

private:
    MessageHandler m_messageHandler;
    DataHandler m_dataHandler;
    DiagnosticHandler m_diagnosticHandler;
};

QT_END_NAMESPACE

#endif // QJSONRPCTRANSPORT_H
