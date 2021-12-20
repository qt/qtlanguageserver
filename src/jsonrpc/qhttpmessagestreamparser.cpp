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

#include "qhttpmessagestreamparser_p.h"

#include <QtCore/QtGlobal>

QT_BEGIN_NAMESPACE

/*!
 * \class QHttpMessageStreamParser
 * \brief Decodes a stream of headers and payloads encoded according to rfc2616 (HTTP/1.1)
 *
 * It complains about invalid sequences, but is quite permissive in accepting them
 */

QHttpMessageStreamParser::QHttpMessageStreamParser(
        std::function<void(const QByteArray &, const QByteArray &)> headerHandler,
        std::function<void(const QByteArray &body)> bodyHandler,
        std::function<void(QtMsgType error, QString msg)> errorHandler)
    : m_headerHandler(std::move(headerHandler)),
      m_bodyHandler(std::move(bodyHandler)),
      m_errorHandler(std::move(errorHandler))
{
}

bool QHttpMessageStreamParser::receiveEof()
{
    if (m_state != State::PreHeader) {
        errorMessage(QtWarningMsg, u"Partial message at end of file"_qs);
        return false;
    }
    return true;
}

void QHttpMessageStreamParser::receiveData(QByteArray data)
{
    const char lf = '\n';
    const char cr = '\r';
    const char colon = ':';
    const char space = ' ';
    const char tab = '\t';
    qsizetype dataPos = 0;
    bool didAdvance = false;
    auto advance = [&]() {
        data = data.mid(dataPos);
        dataPos = 0;
        didAdvance = true;
    };
    while (dataPos < data.size()) {
        switch (m_state) {
        case State::PreHeader:
            switch (data.at(dataPos)) {
            case lf:
                errorMessage(QtWarningMsg,
                             QStringLiteral("Unexpected newline without preceding carriage "
                                            "return at start of headers")
                                     .arg(QString::fromUtf8(m_currentHeaderField)));
                m_state = State::AfterCrLf;
                ++dataPos;
                continue;
            case cr:
                m_state = State::AfterCr;
                ++dataPos;
                continue;
            case tab:
            case space:
                errorMessage(QtWarningMsg,
                             u"Unexpected space at start of headers, skipping"_qs.arg(
                                     QString::fromUtf8(m_currentHeaderField)));
                while (dataPos < data.size()) {
                    char c = data.at(++dataPos);
                    if (c != space && c != tab) {
                        advance();
                        m_state = State::InHeaderField;
                        break;
                    }
                }
                break;
            default:
                m_state = State::InHeaderField;
                break;
            }
            Q_ASSERT(m_currentHeaderField.isEmpty() && m_currentHeaderValue.isEmpty());
            break;
        case State::InHeaderField: {
            didAdvance = false;
            while (!didAdvance) {
                char c = data.at(dataPos);
                switch (c) {
                case lf:
                    m_currentHeaderField.append(data.mid(0, dataPos));
                    errorMessage(
                            QtWarningMsg,
                            u"Unexpected carriage return without newline in unterminated header %1"_qs
                                    .arg(QString::fromUtf8(m_currentHeaderField)));

                    m_state = State::AfterCrLf;
                    advance();
                    ++dataPos;
                    break;
                case cr:
                    m_state = State::AfterCr;
                    m_currentHeaderField.append(data.mid(0, dataPos));
                    errorMessage(QtWarningMsg,
                                 u"Newline before colon in header %1"_qs.arg(
                                         QString::fromUtf8(m_currentHeaderField)));
                    advance();
                    ++dataPos;
                    break;
                case colon:
                    m_currentHeaderField.append(data.mid(0, dataPos));
                    m_state = State::HeaderValueSpace;
                    ++dataPos;
                    advance();
                    break;
                case space:
                case tab:
                    errorMessage(QtWarningMsg, u"Space in header field name"_qs);
                    Q_FALLTHROUGH();
                default:
                    if (++dataPos == data.size()) {
                        m_currentHeaderField.append(data);
                        return;
                    }
                    break;
                }
            }
        } break;
        case State::HeaderValueSpace:
            while (dataPos < data.size()) {
                char c = data.at(dataPos);
                if (c != space && c != tab) {
                    advance();
                    m_state = State::InHeaderValue;
                    m_currentHeaderValue.clear();
                    break;
                }
                ++dataPos;
            }
            break;
        case State::InHeaderValue: {
            didAdvance = false;
            while (!didAdvance) {
                char c = data.at(dataPos);
                switch (c) {
                case lf:
                    m_currentHeaderValue.append(data.mid(0, dataPos));
                    errorMessage(QtWarningMsg,
                                 QStringLiteral("Unexpected newline without preceding "
                                                "carriage return in header %1")
                                         .arg(QString::fromUtf8(m_currentHeaderField)));

                    m_state = State::AfterCrLf;
                    advance();
                    ++dataPos;
                    break;
                case cr:
                    m_currentHeaderValue.append(data.mid(0, dataPos));
                    m_state = State::AfterCr;
                    advance();
                    ++dataPos;
                    break;
                default:
                    if (++dataPos == data.size()) {
                        m_currentHeaderValue.append(data);
                        return;
                    }
                    break;
                }
            }
        } break;
        case State::AfterCr: {
            char c = data.at(dataPos);
            switch (c) {
            case lf:
                m_state = State::AfterCrLf;
                ++dataPos;
                break;
            case cr:
                errorMessage(QtWarningMsg,
                             QStringLiteral("Double carriage return encountred, interpreting it as "
                                            "header end after header %1")
                                     .arg(QString::fromUtf8(m_currentHeaderField)));
                m_currentPacket.clear();
                ++dataPos;
                advance();
                m_state = State::InBody;
                callHasHeader();
                break;
            case space:
            case tab:
                errorMessage(
                        QtWarningMsg,
                        u"Unexpected carriage return without following newline in header %1"_qs.arg(
                                QString::fromUtf8(m_currentHeaderField)));
                m_state = State::InHeaderValue;
                // m_currentHeaderValue.append(data.mid(0,dataPos)) to preserve the (non
                // significant) newlines in header value
                advance();
                break;
            default:
                errorMessage(
                        QtWarningMsg,
                        u"Unexpected carriage return without following newline in header %1"_qs.arg(
                                QString::fromUtf8(m_currentHeaderField)));
                m_state = State::InHeaderField;
                advance();
                callHasHeader();
                break;
            }
        } break;
        case State::AfterCrLf: {
            char c = data.at(dataPos);
            switch (c) {
            case lf:
                errorMessage(QtWarningMsg,
                             u"Newline without carriage return in header %1"_qs.arg(
                                     QString::fromUtf8(m_currentHeaderField)));
                // avoid seeing it as end of headers?
                m_state = State::AfterCrLfCr;
                break;
            case cr:
                m_state = State::AfterCrLfCr;
                ++dataPos;
                break;
            case space:
            case tab:
                m_state = State::InHeaderValue;
                // m_currentHeaderValue.append(data.mid(0,dataPos)) to preserve the (non
                // significant) newlines in header value
                advance();
                break;
            default:
                m_state = State::InHeaderField;
                advance();
                callHasHeader();
                break;
            }
        } break;
        case State::AfterCrLfCr: {
            char c = data.at(dataPos);
            switch (c) {
            case lf:
                m_currentPacket.clear();
                ++dataPos;
                advance();
                m_state = State::InBody;
                callHasHeader();
                break;
            default:
                errorMessage(
                        QtWarningMsg,
                        u"crlfcr without final lf encountred, ignoring it (non clear terminator)"_qs);
                m_state = State::InHeaderField;
                advance();
                callHasHeader();
                break;
            }
        } break;
        case State::InBody: {
            if (m_contentSize == -1) {
                errorMessage(QtWarningMsg, u"missing valid Content-Length header"_qs);
                m_state = State::PreHeader;
                continue;
            }
            qint64 missing = m_contentSize - m_currentPacket.size();
            if (missing > 0) {
                dataPos = qMin(qsizetype(missing), data.size());
                m_currentPacket.append(data.mid(0, dataPos));
                advance();
            }
            if (m_currentPacket.size() >= m_contentSize) {
                m_state = State::PreHeader;
                callHasBody();
            }
        } break;
        }
    }
    if (m_state == State::InBody && (m_contentSize == -1 || m_contentSize == 0)) {
        // nothing to read, but emit empty body...
        m_state = State::PreHeader;
        if (m_contentSize == -1)
            errorMessage(QtWarningMsg, u"missing valid Content-Length header"_qs);
        callHasBody();
    }
}

void QHttpMessageStreamParser::callHasHeader()
{
    static const QByteArray s_contentLengthFieldName = "Content-Length";
    if (m_currentHeaderField.isEmpty() && m_currentHeaderValue.isEmpty())
        return;
    QByteArray field = m_currentHeaderField;
    QByteArray value = m_currentHeaderValue;
    m_currentHeaderField.clear();
    m_currentHeaderValue.clear();
    if (s_contentLengthFieldName.compare(field, Qt::CaseInsensitive) == 0) {
        bool ok = false;
        const int size = value.toInt(&ok);
        if (ok) {
            m_contentSize = size;
        } else {
            errorMessage(
                    QtWarningMsg,
                    u"Invalid %1: %2"_qs.arg(QString::fromUtf8(field), QString::fromUtf8(value)));
        }
    }
    if (m_headerHandler)
        m_headerHandler(field, value);
}

void QHttpMessageStreamParser::callHasBody()
{
    QByteArray body = m_currentPacket;
    m_currentPacket.clear();
    m_contentSize = -1;
    if (m_bodyHandler)
        m_bodyHandler(body);
}

void QHttpMessageStreamParser::errorMessage(QtMsgType error, QString msg)
{
    if (m_errorHandler)
        m_errorHandler(error, msg);
}

QT_END_NAMESPACE
