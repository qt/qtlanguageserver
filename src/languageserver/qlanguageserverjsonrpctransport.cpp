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

#include "qlanguageserverjsonrpctransport_p.h"

#include <iostream>

QT_BEGIN_NAMESPACE

static const QByteArray s_contentLengthFieldName = "Content-Length";
static const QByteArray s_contentTypeFieldName = "Content-Type";
static const QByteArray s_fieldSeparator = ": ";
static const QByteArray s_headerSeparator = "\r\n";
static const QByteArray s_headerEnd = "\r\n\r\n";
static const QByteArray s_utf8 = "utf-8";
static const QByteArray s_brokenUtf8 = "utf8";

void QLanguageServerJsonRpcTransport::sendMessage(const QJsonDocument &packet)
{
    const QByteArray content = packet.toJson(QJsonDocument::Compact);
    if (auto handler = dataHandler()) {
        // send all data in one go, this way if handler is threadsafe the whole sendMessage is
        // threadsafe
        QByteArray msg;
        msg.append(s_contentLengthFieldName);
        msg.append(s_fieldSeparator);
        msg.append(QByteArray::number(content.length()));
        msg.append(s_headerSeparator);
        msg.append(s_headerSeparator);
        msg.append(content);
        handler(msg);
    }
}

bool QLanguageServerJsonRpcTransport::hasMessage() const
{
    return m_contentStart >= 0 && m_contentSize >= 0
            && m_contentStart + m_contentSize <= m_currentPacket.length();
}

void QLanguageServerJsonRpcTransport::parseMessage()
{
    QJsonParseError error = { 0, QJsonParseError::NoError };
    const QByteArray content = m_currentPacket.mid(m_contentStart, m_contentSize);
    const QJsonDocument doc = QJsonDocument::fromJson(content, &error);

    m_currentPacket = m_currentPacket.mid(m_contentStart + m_contentSize);
    m_contentStart = m_contentSize = -1;
    parseHeader();

    if (auto handler = messageHandler())
        handler(doc, error);
}

void QLanguageServerJsonRpcTransport::receiveData(const QByteArray &data)
{
    m_currentPacket.append(data);
    parseHeader();
    while (hasMessage())
        parseMessage();
}

void QLanguageServerJsonRpcTransport::parseHeader()
{
    if (m_contentStart < 0) {
        m_contentStart = m_currentPacket.indexOf(s_headerEnd);
        if (m_contentStart < 0)
            return;

        m_contentStart += s_headerEnd.length();
    }

    if (m_contentSize < 0) {
        int fieldStart = 0;
        for (int fieldEnd = m_currentPacket.indexOf(s_headerSeparator);
             fieldEnd >= 0 && fieldEnd <= m_contentStart - s_headerEnd.length();
             fieldEnd = m_currentPacket.indexOf(s_headerSeparator, fieldStart)) {

            const QByteArray headerField = m_currentPacket.mid(fieldStart, fieldEnd - fieldStart);
            const int nameEnd = headerField.lastIndexOf(s_fieldSeparator);

            if (nameEnd < 0) {
                if (auto handler = diagnosticHandler()) {
                    handler(Warning,
                            QString::fromLatin1("Invalid header field: %1")
                                    .arg(QString::fromUtf8(headerField)));
                }
            } else {
                const QByteArray fieldName = headerField.left(nameEnd);
                const QByteArray fieldValue = headerField.mid(nameEnd + s_fieldSeparator.length());

                if (s_contentLengthFieldName.compare(fieldName, Qt::CaseInsensitive) == 0) {
                    bool ok = false;
                    const int size = fieldValue.toInt(&ok);
                    if (ok) {
                        m_contentSize = size;
                    } else if (auto handler = diagnosticHandler()) {
                        handler(Warning,
                                QString::fromLatin1("Invalid %1: %2")
                                        .arg(QString::fromUtf8(fieldName))
                                        .arg(QString::fromUtf8(fieldValue)));
                    }
                } else if (s_contentTypeFieldName.compare(fieldName, Qt::CaseInsensitive) == 0) {
                    if (fieldValue != s_utf8 && fieldValue != s_brokenUtf8) {
                        if (auto handler = diagnosticHandler()) {
                            handler(Warning,
                                    QString::fromLatin1("Invalid %1: %2")
                                            .arg(QString::fromUtf8(fieldName))
                                            .arg(QString::fromUtf8(fieldValue)));
                        }
                    }
                } else if (auto handler = diagnosticHandler()) {
                    handler(Warning,
                            QString::fromLatin1("Unknown header field: %1")
                                    .arg(QString::fromUtf8(fieldName)));
                }
            }

            fieldStart = fieldEnd + s_headerSeparator.length();
        }

        if (m_contentSize < 0) {
            m_currentPacket.clear();
            m_contentStart = -1;
            if (auto handler = diagnosticHandler())
                handler(Error, QString::fromLatin1("No valid Content-Length header found."));
        }
    }
}

QT_END_NAMESPACE
