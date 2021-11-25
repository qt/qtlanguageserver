/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtJsonRpc/private/qjsonrpctransport_p.h>
#include <QtJsonRpc/private/qjsonrpcprotocol_p.h>

#include <QtCore/qobject.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qstring.h>
#include <QtTest/qtest.h>

#include <queue>

QT_BEGIN_NAMESPACE

class EchoTransport : public QJsonRpcTransport
{
public:
    using EchoHandler = std::function<void(const QByteArray &)>;

    void sendMessage(const QJsonDocument &message) final;
    void receiveData(const QByteArray &bytes) final;

    void setEchoHandler(const EchoHandler &handler) { m_messageHandler = handler; }

private:
    EchoHandler m_messageHandler;
};

struct ScopedConnection
{
    Q_DISABLE_COPY_MOVE(ScopedConnection);
    ScopedConnection(QMetaObject::Connection connection);
    ~ScopedConnection();

    QMetaObject::Connection connection;
};

class SumHandler : public QJsonRpcProtocol::MessageHandler
{
public:
    void handleRequest(const QJsonRpcProtocol::Request &request,
                       const ResponseHandler &handler) final;

private:
    static QJsonRpcProtocol::Response plus(const QJsonRpcProtocol::Request &request);
};

class SubtractHandler : public QJsonRpcProtocol::MessageHandler
{
public:
    void handleRequest(const QJsonRpcProtocol::Request &request,
                       const ResponseHandler &handler) final;

private:
    static QJsonRpcProtocol::Response minus(const QJsonRpcProtocol::Request &request);
};

class UpdateHandler : public QJsonRpcProtocol::MessageHandler
{
public:
    void handleNotification(const QJsonRpcProtocol::Notification &notification) final;

    QJsonValue lastUpdate = QJsonValue::Undefined;
    int numUpdates = 0;
};

class GetDataHandler : public QJsonRpcProtocol::MessageHandler
{
public:
    void handleRequest(const QJsonRpcProtocol::Request &request,
                       const ResponseHandler &handler) final;
};

class tst_QJsonRpcProtocol : public QObject
{
    Q_OBJECT

public:
    tst_QJsonRpcProtocol();

private slots:
    void specRequests_data();
    void specRequests();

    void specNotifications_data();
    void specNotifications();

    void clientRequests_data();
    void clientRequests();

    void clientNotifications_data();
    void clientNotifications();

    void badResponses();

private:
    EchoTransport transport;
    QJsonRpcProtocol protocol;
};

tst_QJsonRpcProtocol::tst_QJsonRpcProtocol()
{
    protocol.setTransport(&transport);
    protocol.setMessageHandler("sum", new SumHandler);
    protocol.setMessageHandler("subtract", new SubtractHandler);
    protocol.setMessageHandler("update", new UpdateHandler);
    protocol.setMessageHandler("get_data", new GetDataHandler);
    protocol.setMessageHandler("notify_hello", new UpdateHandler);
}

void tst_QJsonRpcProtocol::specRequests_data()
{
    QTest::addColumn<QByteArray>("request");
    QTest::addColumn<QByteArray>("response");

    QTest::newRow("rpc call with positional parameters 1")
            << QByteArray(R"({"jsonrpc": "2.0", "method": "subtract",
                                                "params": [42, 23], "id": 1})")
            << QByteArray(R"({"jsonrpc": "2.0", "result": 19, "id": 1})");
    QTest::newRow("rpc call with positional parameters 2")
            << QByteArray(R"({"jsonrpc": "2.0", "method": "subtract",
                                                "params": [23, 42], "id": 2})")
            << QByteArray(R"({"jsonrpc": "2.0", "result": -19, "id": 2})");

    QTest::newRow("rpc call with named parameters 1")
            << QByteArray(R"({"jsonrpc": "2.0", "method": "subtract",
                                                "params": {"subtrahend": 23, "minuend": 42},
                                                "id": 3})")
            << QByteArray(R"({"jsonrpc": "2.0", "result": 19, "id": 3})");
    QTest::newRow("rpc call with named parameters 2")
            << QByteArray(R"({"jsonrpc": "2.0", "method": "subtract",
                                                "params": {"minuend": 42, "subtrahend": 23},
                                                "id": 4})")
            << QByteArray(R"({"jsonrpc": "2.0", "result": 19, "id": 4})");

    QTest::newRow("rpc call of non-existent method")
            << QByteArray(R"({"jsonrpc": "2.0", "method": "foobar", "id": "1"})")
            << QByteArray(R"({"jsonrpc": "2.0", "error": {"code": -32601,
                                                          "message": "Method not found"},
                                                "id": "1"})");

    QTest::newRow("rpc call with invalid JSON")
            << QByteArray(R"({"jsonrpc": "2.0", "method": "foobar, "params": "bar", "baz])")
            << QByteArray(R"({"jsonrpc": "2.0", "error": {"code": -32700, "message": "Parse error"},
                                                "id": null})");

    QTest::newRow("rpc call with invalid Request object")
            << QByteArray(R"({"jsonrpc": "2.0", "method": 1, "params": "bar"})")
            << QByteArray(R"({"jsonrpc": "2.0", "error": {"code": -32600,
                                                          "message": "Invalid Request"},
                                                "id": null})");

    QTest::newRow("rpc call Batch, invalid JSON")
            << QByteArray(R"([ {"jsonrpc": "2.0", "method": "sum", "params": [1,2,4], "id": "1"},
                               {"jsonrpc": "2.0", "method" ])")
            << QByteArray(R"({"jsonrpc": "2.0", "error": {"code": -32700, "message": "Parse error"},
                                                "id": null})");

    QTest::newRow("rpc call with an empty Array")
            << QByteArray("[]") << QByteArray(R"({"jsonrpc": "2.0", "error": {"code": -32600,
                                                          "message": "Invalid Request"},
                                                "id": null})");

    QTest::newRow("rpc call with an invalid Batch (but not empty)")
            << QByteArray("[1]") << QByteArray(R"([ {"jsonrpc": "2.0", "error": {"code": -32600,
                                                            "message": "Invalid Request"},
                                                  "id": null} ])");

    QTest::newRow("rpc call with invalid Batch")
            << QByteArray("[1,2,3]") << QByteArray(R"([ {"jsonrpc": "2.0", "error": {"code": -32600,
                                                            "message": "Invalid Request"},
                                                  "id": null},
                               {"jsonrpc": "2.0", "error": {"code": -32600,
                                                            "message": "Invalid Request"},
                                                  "id": null},
                               {"jsonrpc": "2.0", "error": {"code": -32600,
                                                            "message": "Invalid Request"},
                                                  "id": null} ])");

    QTest::newRow("rpc call Batch")
            << QByteArray(R"([ {"jsonrpc": "2.0", "method": "sum", "params": [1,2,4], "id": "1"},
                               {"jsonrpc": "2.0", "method": "notify_hello", "params": [7]},
                               {"jsonrpc": "2.0", "method": "subtract",
                                                  "params": [42,23], "id": "2"},
                               {"foo": "boo"},
                               {"jsonrpc": "2.0", "method": "foo.get",
                                                  "params": {"name": "myself"}, "id": "5"},
                               {"jsonrpc": "2.0", "method": "get_data", "id": "9"} ])")
            << QByteArray(R"([ {"jsonrpc": "2.0", "result": 7, "id": "1"},
                               {"jsonrpc": "2.0", "result": 19, "id": "2"},
                               {"jsonrpc": "2.0", "error": {"code": -32600,
                                                            "message":  "Invalid Request"},
                                                  "id": null},
                               {"jsonrpc": "2.0", "error": {"code": -32601,
                                                            "message": "Method not found"},
                                                  "id": "5"},
                               {"jsonrpc": "2.0", "result": ["hello", 5], "id": "9"} ])");
    QTest::newRow("rpc call Batch (all valid)")
            << QByteArray(R"([ {"jsonrpc": "2.0", "method": "sum", "params": [1,2,4], "id": "1"},
                               {"jsonrpc": "2.0", "method": "notify_hello", "params": [7]},
                               {"jsonrpc": "2.0", "method": "subtract",
                                                  "params": [42,23], "id": "2"},
                               {"jsonrpc": "2.0", "method": "foo.get",
                                                "params": {"name": "myself"}, "id": "5"},
                               {"jsonrpc": "2.0", "method": "get_data", "id": "9"} ])")
            << QByteArray(R"([ {"jsonrpc": "2.0", "result": 7, "id": "1"},
                               {"jsonrpc": "2.0", "result": 19, "id": "2"},
                               {"jsonrpc": "2.0", "error": {"code": -32601,
                                                  "message": "Method not found"}, "id": "5"},
                               {"jsonrpc": "2.0", "result": ["hello", 5], "id": "9"} ])");
}

static bool operator<(const QJsonValue &a, const QJsonValue &b)
{
    if (a.type() != b.type())
        return a.type() < b.type();

    QJsonValue idA = a.isObject() ? a.toObject().value("id") : QJsonValue::Undefined;
    QJsonValue idB = b.isObject() ? b.toObject().value("id") : QJsonValue::Undefined;

    if (idA.type() != idB.type())
        return idA.type() < idB.type();

    switch (idA.type()) {
    // If you have multiple null ids, that won't work. But we don't, for now.
    case QJsonValue::Null:
        return false;

    case QJsonValue::Bool:
        return idA.toBool() < idB.toBool();
    case QJsonValue::Double:
        return idA.toDouble() < idB.toDouble();
    case QJsonValue::String:
        return idA.toString() < idB.toString();
    case QJsonValue::Array:
        return false;
    case QJsonValue::Object:
        return false;
    case QJsonValue::Undefined:
        return false;
    }
    return false;
}

void swap(QJsonValueRef a, QJsonValueRef b)
{
    // Note: Do not assign a QJsonValueRef to another one. It does not do what you think it does.
    QJsonValue x = a;
    QJsonValue y = b;
    a = std::move(y);
    b = std::move(x);
}

static QJsonDocument sortDocument(const QJsonDocument &doc)
{
    if (doc.isArray()) {
        QJsonArray array = doc.array();
        std::sort(array.begin(), array.end());
        return QJsonDocument(array);
    }
    return doc;
}

static bool operator<(const QJsonRpcProtocol::Response &a, const QJsonRpcProtocol::Response &b)
{
    return a.id < b.id;
}

void tst_QJsonRpcProtocol::specRequests()
{
    QFETCH(QByteArray, request);
    QFETCH(QByteArray, response);

    int responses = 0;
    transport.setEchoHandler([&](const QByteArray &received) {
        QJsonDocument document = sortDocument(QJsonDocument::fromJson(response));
        QJsonDocument actual = sortDocument(QJsonDocument::fromJson(received));
        QCOMPARE(actual, document);
        ++responses;
    });

    transport.receiveData(request);
    QTRY_COMPARE(responses, 1);

    transport.setEchoHandler(nullptr);
}

void tst_QJsonRpcProtocol::specNotifications_data()
{
    QTest::addColumn<QByteArray>("notification");
    QTest::addColumn<QString>("method");
    QTest::addColumn<QByteArray>("params");

    QTest::newRow("notification 1")
            << QByteArray(R"({"jsonrpc": "2.0", "method": "update", "params": [1,2,3,4,5]})")
            << "update" << QByteArray("[1,2,3,4,5]");

    QTest::newRow("notification 2")
            << QByteArray(R"({"jsonrpc": "2.0", "method": "foobar"})") << QString() << QByteArray();

    // Silently ignore notify_sum, but trigger notify_hello.
    QTest::newRow("rpc call Batch (all notifications)")
            << QByteArray(R"([ {"jsonrpc": "2.0", "method": "notify_sum", "params": [1,2,4]},
                               {"jsonrpc": "2.0", "method": "notify_hello", "params": [7]}])")
            << "notify_hello" << QByteArray("[7]");
}

void tst_QJsonRpcProtocol::specNotifications()
{
    QFETCH(QByteArray, notification);
    QFETCH(QString, method);
    QFETCH(QByteArray, params);

    transport.setEchoHandler([](const QByteArray &received) { QFAIL(received); });

    if (!method.isEmpty()) {
        const UpdateHandler *handler =
                static_cast<UpdateHandler *>(protocol.messageHandler(method));
        QVERIFY(handler != nullptr);
        const int prevUpdates = handler->numUpdates;
        transport.receiveData(notification);
        QTRY_COMPARE(handler->numUpdates, prevUpdates + 1);

        auto doc = QJsonDocument::fromJson(params);
        if (doc.isArray())
            QCOMPARE(handler->lastUpdate, doc.array());
        else if (doc.isObject())
            QCOMPARE(handler->lastUpdate, doc.object());
        else
            QFAIL("Use arrays or objects as parameters");
    } else {
        transport.receiveData(notification);
    }

    QTest::qWait(20); // Check that nothing is sent back.

    transport.setEchoHandler(nullptr);
}

void tst_QJsonRpcProtocol::clientRequests_data()
{
    return specRequests_data();
}

void tst_QJsonRpcProtocol::clientRequests()
{
    QFETCH(QByteArray, request);
    QFETCH(QByteArray, response);

    int requests = 0;

    transport.setEchoHandler([&](const QByteArray &data) {
        QJsonDocument document = sortDocument(QJsonDocument::fromJson(request));
        QJsonDocument actual = sortDocument(QJsonDocument::fromJson(data));
        QCOMPARE(actual, document);
        ++requests;
        transport.receiveData(response);
    });

    auto verifyObject = [&](const QJsonRpcProtocol::Response &actual,
                            const QJsonObject &expectedObject) {
        QCOMPARE(expectedObject.value("id"), actual.id);
        if (expectedObject.contains("error")) {
            const QJsonObject expectedError = expectedObject.value("error").toObject();
            QCOMPARE(actual.errorCode, expectedError.value("code"));
            QCOMPARE(actual.errorMessage, expectedError.value("message").toString());
            QCOMPARE(actual.data, expectedError.value("data"));
        } else {
            QCOMPARE(actual.data, expectedObject.value("result"));
        }
    };

    const QJsonDocument doc = QJsonDocument::fromJson(request);
    const QJsonDocument expected = QJsonDocument::fromJson(response);
    if (doc.isObject()) {
        QVERIFY(expected.isObject());
        const QJsonObject object = doc.object();
        QJsonRpcProtocol::Request message;
        message.id = object.value("id");
        QVERIFY(object.contains("method"));
        if (!object.value("method").isString())
            QSKIP("Cannot send invalid non-string method through QJsonRpcProtocol.");
        message.method = object.value("method").toString();
        message.params = object.value("params");
        protocol.sendRequest(message, [&](const QJsonRpcProtocol::Response &result) {
            verifyObject(result, expected.object());
        });
        QCOMPARE(requests, 1);
    } else if (doc.isArray()) {
        const QJsonArray array = doc.array();
        QJsonRpcProtocol::Batch batch;
        for (const QJsonValue &value : array) {
            if (!value.isObject())
                QSKIP("Cannot send non-object requests through QJsonRpcProtocol.");
            const QJsonObject object = value.toObject();
            if (!object.value("method").isString())
                QSKIP("Cannot send invalid non-string method through QJsonRpcProtocol.");

            if (object.contains("id")) {
                QJsonRpcProtocol::Request message;
                message.id = object.value("id");
                message.method = object.value("method").toString();
                message.params = object.value("params");
                batch.addRequest(message);
            } else {
                QJsonRpcProtocol::Notification message;
                message.method = object.value("method").toString();
                message.params = object.value("params");
                batch.addNotification(message);
            }
        }

        QVector<QJsonRpcProtocol::Response> responses;
        protocol.sendBatch(std::move(batch), [&](const QJsonRpcProtocol::Response &response) {
            responses.append(response);
        });

        if (array.isEmpty()) {
            // QJsonRpcProtocol ignores empty batches, so we don't get the expected error.
            QCOMPARE(requests, 0);
        } else {
            QVERIFY(expected.isArray());
            std::sort(responses.begin(), responses.end());
            QJsonArray sorted = sortDocument(expected).array();
            QCOMPARE(responses.size(), sorted.size());
            for (int i = 0; i < responses.size(); ++i)
                verifyObject(responses.at(i), sorted.at(i).toObject());
            QCOMPARE(requests, 1);
        }
    } else {
        QSKIP("Cannot send malformed JSON through QJsonRpcProtocol.");
    }

    transport.setEchoHandler(nullptr);
}

void tst_QJsonRpcProtocol::clientNotifications_data()
{
    return specNotifications_data();
}

void tst_QJsonRpcProtocol::clientNotifications()
{
    QFETCH(QByteArray, notification);
    QFETCH(QString, method);
    QFETCH(QByteArray, params);

    Q_UNUSED(method);
    Q_UNUSED(params);

    int notifications = 0;
    transport.setEchoHandler([&](const QByteArray &data) {
        QJsonDocument document = QJsonDocument::fromJson(notification);
        QJsonDocument actual = QJsonDocument::fromJson(data);
        QCOMPARE(actual, document);
        ++notifications;
    });

    QJsonDocument doc = QJsonDocument::fromJson(notification);
    if (doc.isObject()) {
        const QJsonObject object = doc.object();
        QJsonRpcProtocol::Notification message;
        message.method = object.value("method").toString();
        message.params = object.value("params");
        protocol.sendNotification(message);
    } else if (doc.isArray()) {
        const QJsonArray array = doc.array();
        QJsonRpcProtocol::Batch batch;
        for (const QJsonValue &value : array) {
            QVERIFY(value.isObject());
            const QJsonObject object = value.toObject();
            QJsonRpcProtocol::Notification message;
            message.method = object.value("method").toString();
            message.params = object.value("params");
            batch.addNotification(message);
        }
        protocol.sendBatch(std::move(batch), [](const QJsonRpcProtocol::Response &) {
            QFAIL("Response received for notification");
        });
    } else {
        QFAIL("malformed JSON");
    }

    QTRY_COMPARE(notifications, 1);
    transport.setEchoHandler(nullptr);
}

void tst_QJsonRpcProtocol::badResponses()
{
    int invalids = 0;
    int errors = 0;

    protocol.setInvalidResponseHandler([&](const QJsonRpcProtocol::Response &response) {
        ++invalids;
        QCOMPARE(response.id, 1);
        QCOMPARE(response.errorCode, -32601);
        QCOMPARE(response.errorMessage, "Method not found");
        QVERIFY(response.data.isUndefined());
    });

    protocol.setProtocolErrorHandler([&](const QJsonRpcProtocol::Response &response) {
        ++errors;
        QCOMPARE(response.errorCode, -32700);
        QCOMPARE(response.errorMessage, "Parse error");
        QVERIFY(response.id.isNull());
        QVERIFY(response.data.isUndefined());
    });

    transport.receiveData("{\"jsonrpc\": \"2.0\", \"error\": "
                          "{\"code\": -32700, \"message\": \"Parse error\"}, \"id\": null}");

    QTRY_COMPARE(errors, 1);
    QCOMPARE(invalids, 0);

    transport.receiveData("{\"jsonrpc\": \"2.0\", \"error\": "
                          "{\"code\": -32601, \"message\": \"Method not found\"}, \"id\": 1}");

    QTRY_COMPARE(invalids, 1);
    QCOMPARE(errors, 1);

    protocol.setInvalidResponseHandler(nullptr);
    protocol.setProtocolErrorHandler(nullptr);
}

void SumHandler::handleRequest(const QJsonRpcProtocol::Request &request,
                               const ResponseHandler &handler)
{
    if (request.id.toInt() & 0x1)
        handler(plus(request));
    else
        asynchronous(&SumHandler::plus, request, handler);
}

QJsonRpcProtocol::Response SumHandler::plus(const QJsonRpcProtocol::Request &request)
{
    if (request.params.isArray()) {
        const QJsonArray array = request.params.toArray();

        if (array.count() == 0)
            return error(QJsonRpcProtocol::ErrorCode::InvalidParams);

        double sum = 0;
        for (const QJsonValue &value : array) {
            if (!value.isDouble())
                return error(QJsonRpcProtocol::ErrorCode::InvalidParams);
            sum += value.toDouble();
        }
        return result(sum);
    }
    return error(QJsonRpcProtocol::ErrorCode::InvalidParams);
}

void SubtractHandler::handleRequest(const QJsonRpcProtocol::Request &request,
                                    const ResponseHandler &handler)
{
    if (request.id.toInt() & 0x1)
        handler(minus(request));
    else
        asynchronous(&SubtractHandler::minus, request, handler);
}

QJsonRpcProtocol::Response SubtractHandler::minus(const QJsonRpcProtocol::Request &request)
{
    QJsonValue minuend;
    QJsonValue subtrahend;
    if (request.params.isArray()) {
        const QJsonArray array = request.params.toArray();

        if (array.count() != 2)
            return error(QJsonRpcProtocol::ErrorCode::InvalidParams);

        minuend = array.at(0);
        subtrahend = array.at(1);
    } else if (request.params.isObject()) {
        const QJsonObject object = request.params.toObject();
        minuend = object.value("minuend");
        subtrahend = object.value("subtrahend");
    } else {
        return error(QJsonRpcProtocol::ErrorCode::InvalidParams);
    }

    if (!minuend.isDouble() || !subtrahend.isDouble())
        return error(QJsonRpcProtocol::ErrorCode::InvalidParams);

    return result(minuend.toDouble() - subtrahend.toDouble());
}

void UpdateHandler::handleNotification(const QJsonRpcProtocol::Notification &notification)
{
    lastUpdate = notification.params;
    ++numUpdates;
}

void GetDataHandler::handleRequest(const QJsonRpcProtocol::Request &request,
                                   const ResponseHandler &handler)
{
    asynchronous(
            [](const QJsonRpcProtocol::Request &request) {
                if (!request.params.isUndefined())
                    return error(QJsonRpcProtocol::ErrorCode::InvalidParams);

                QJsonRpcProtocol::Response response;
                QJsonArray array;
                array.append("hello");
                array.append(5);
                response.data = array;
                return response;
            },
            request, handler);
}

void EchoTransport::sendMessage(const QJsonDocument &message)
{
    if (m_messageHandler)
        m_messageHandler(message.toJson(QJsonDocument::Compact));
}

void EchoTransport::receiveData(const QByteArray &bytes)
{
    QJsonParseError error = QJsonParseError();
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &error);
    messageHandler()(doc, error);
}

ScopedConnection::ScopedConnection(QMetaObject::Connection connection)
    : connection(std::move(connection))
{
}

ScopedConnection::~ScopedConnection()
{
    QObject::disconnect(connection);
}

QT_END_NAMESPACE

QTEST_MAIN(tst_QJsonRpcProtocol)

#include "tst_qjsonrpcprotocol.moc"
