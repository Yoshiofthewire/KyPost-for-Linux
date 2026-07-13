#include "net/NtfySubscriber.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

namespace {

// Minimal raw QTcpServer harness that streams a chunked-transfer-encoded
// response body across multiple separate write() calls, with connections
// left open until the test closes them. FakeRelayServer.h (Task 15) only
// hands back one complete response in a single write and isn't a fit for
// exercising NtfySubscriber::onReadyRead's partial-line-buffering path, so
// this is test-local rather than a FakeRelayServer.h change.
//
// Two framing choices were verified empirically against this Qt build
// before settling here (see task-24-report.md):
// - Transfer-Encoding: chunked, not a Content-Length-less "read until
//   close" response -- the latter was observed to make
//   QNetworkAccessManager treat the reply as already finished (0-length
//   body) immediately after headers arrived, before any body byte had been
//   written.
// - Every write is broadcast to *every* currently-open connection, not
//   just the most recently accepted one -- QNetworkAccessManager was
//   observed to non-deterministically open a second physical TCP
//   connection for a single logical GET against a fresh loopback host
//   (both sending an identical request line), with only one of the two
//   actually backing the live QNetworkReply. Broadcasting sidesteps having
//   to guess which physical socket Qt picked.
class NtfyStreamServer : public QObject
{
public:
    NtfyStreamServer()
    {
        m_server.listen(QHostAddress::LocalHost);
        connect(&m_server, &QTcpServer::newConnection, this, &NtfyStreamServer::onNewConnection);
    }

    quint16 port() const { return m_server.serverPort(); }

    // Number of connections whose request headers have actually been
    // received (and responded to) -- not merely accepted at the TCP level.
    // A freshly-accepted socket may not have delivered its request bytes
    // yet, so callers must wait on this, not on raw accept count, before
    // asserting on request text or writing a response body.
    int readyRequestCount() const
    {
        int n = 0;
        for (bool sent : m_headersSent) {
            if (sent)
                ++n;
        }
        return n;
    }

    // True if any ready (headers-received) connection at index >= fromIndex
    // has sent a request whose raw text contains `needle`.
    bool anyRequestFromContains(int fromIndex, const QByteArray& needle) const
    {
        for (int i = fromIndex; i < m_requests.size(); ++i) {
            if (m_requests.at(i).contains(needle))
                return true;
        }
        return false;
    }

    // Spins the event loop (no fixed sleep) until at least `count`
    // connections have fully delivered their request headers.
    bool waitForAtLeastConnections(int count, int timeoutMs = 2000)
    {
        QElapsedTimer timer;
        timer.start();
        while (readyRequestCount() < count) {
            if (timer.hasExpired(timeoutMs))
                return false;
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        return true;
    }

    // Wraps data as one HTTP chunk and writes it to every currently-open
    // connection.
    void broadcast(const QByteArray& data)
    {
        const QByteArray framed = QByteArray::number(data.size(), 16) + "\r\n" + data + "\r\n";
        for (QTcpSocket* socket : std::as_const(m_sockets)) {
            if (socket->state() == QAbstractSocket::ConnectedState) {
                socket->write(framed);
                socket->flush();
            }
        }
    }

    // Drops every open connection mid-stream without a terminating chunk,
    // the way a real long-poll connection's server-side timeout or network
    // drop would.
    void closeAll()
    {
        for (QTcpSocket* socket : std::as_const(m_sockets)) {
            if (socket->state() == QAbstractSocket::ConnectedState)
                socket->disconnectFromHost();
        }
    }

private:
    void onNewConnection()
    {
        while (QTcpSocket* socket = m_server.nextPendingConnection()) {
            const int index = m_sockets.size();
            m_sockets.append(socket);
            m_requests.append(QByteArray());
            m_headersSent.append(false);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, index]() {
                m_requests[index] += socket->readAll();
                if (!m_headersSent[index] && m_requests[index].contains("\r\n\r\n")) {
                    m_headersSent[index] = true;
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/x-ndjson\r\n"
                                   "Transfer-Encoding: chunked\r\n\r\n");
                    socket->flush();
                }
            });
        }
    }

    QTcpServer m_server;
    QList<QTcpSocket*> m_sockets;
    QList<QByteArray> m_requests;
    QList<bool> m_headersSent;
};

} // namespace

class NtfySubscriberTest : public QObject
{
    Q_OBJECT

private slots:
    void singleCompleteMessageEmitsOnce();
    void keepaliveLineEmitsNothing();
    void messageSplitAcrossTwoWritesStillParses();
    void twoMessagesInOneBatchEmitTwiceInOrder();
    void connectionCloseTriggersReconnectWithLastSince();
};

void NtfySubscriberTest::singleCompleteMessageEmitsOnce()
{
    NtfyStreamServer server;
    QNetworkAccessManager manager;
    NtfySubscriber subscriber(manager, QStringLiteral("http://127.0.0.1:%1").arg(server.port()),
                               QStringLiteral("mytopic"));
    QSignalSpy messageSpy(&subscriber, &NtfySubscriber::messageReceived);

    subscriber.start(0);
    QVERIFY(server.waitForAtLeastConnections(1));

    server.broadcast(
        QByteArray(
            R"({"id":"a1","time":1751928000,"event":"message","topic":"mytopic","title":"Hi","message":"Hello"})")
        + "\n");

    QVERIFY(messageSpy.wait());
    QCOMPARE(messageSpy.size(), 1);
    const QJsonObject data = messageSpy.at(0).at(0).toJsonObject();
    QCOMPARE(data.value(QStringLiteral("title")).toString(), QStringLiteral("Hi"));
    QCOMPARE(data.value(QStringLiteral("message")).toString(), QStringLiteral("Hello"));
}

void NtfySubscriberTest::keepaliveLineEmitsNothing()
{
    NtfyStreamServer server;
    QNetworkAccessManager manager;
    NtfySubscriber subscriber(manager, QStringLiteral("http://127.0.0.1:%1").arg(server.port()),
                               QStringLiteral("mytopic"));
    QSignalSpy messageSpy(&subscriber, &NtfySubscriber::messageReceived);
    QSignalSpy lostSpy(&subscriber, &NtfySubscriber::connectionLost);

    subscriber.start(0);
    QVERIFY(server.waitForAtLeastConnections(1));

    server.broadcast(QByteArray(R"({"id":"a1","time":1751928000,"event":"keepalive"})") + "\n");
    server.closeAll();

    // connectionLost firing is the deterministic signal that the keepalive
    // line has already been read and processed (or dropped) by the time we
    // assert -- no fixed sleep needed to prove the absence of an emission.
    QVERIFY(lostSpy.wait());
    QCOMPARE(messageSpy.size(), 0);
}

void NtfySubscriberTest::messageSplitAcrossTwoWritesStillParses()
{
    NtfyStreamServer server;
    QNetworkAccessManager manager;
    NtfySubscriber subscriber(manager, QStringLiteral("http://127.0.0.1:%1").arg(server.port()),
                               QStringLiteral("mytopic"));
    QSignalSpy messageSpy(&subscriber, &NtfySubscriber::messageReceived);

    subscriber.start(0);
    QVERIFY(server.waitForAtLeastConnections(1));

    const QByteArray line =
        R"({"id":"a1","time":1751928000,"event":"message","topic":"mytopic","title":"Hi","message":"Hello"})";
    server.broadcast(line.left(20));
    QTest::qWait(50); // let onReadyRead run on the first half before sending the second
    QCOMPARE(messageSpy.size(), 0);

    server.broadcast(line.mid(20) + "\n");
    QVERIFY(messageSpy.wait());
    QCOMPARE(messageSpy.size(), 1);
    QCOMPARE(messageSpy.at(0).at(0).toJsonObject().value(QStringLiteral("title")).toString(), QStringLiteral("Hi"));
}

void NtfySubscriberTest::twoMessagesInOneBatchEmitTwiceInOrder()
{
    NtfyStreamServer server;
    QNetworkAccessManager manager;
    NtfySubscriber subscriber(manager, QStringLiteral("http://127.0.0.1:%1").arg(server.port()),
                               QStringLiteral("mytopic"));
    QSignalSpy messageSpy(&subscriber, &NtfySubscriber::messageReceived);

    subscriber.start(0);
    QVERIFY(server.waitForAtLeastConnections(1));

    const QByteArray first =
        R"({"id":"a1","time":1751928000,"event":"message","topic":"mytopic","title":"One","message":"First"})";
    const QByteArray second =
        R"({"id":"a2","time":1751928010,"event":"message","topic":"mytopic","title":"Two","message":"Second"})";
    server.broadcast(first + "\n" + second + "\n");

    QVERIFY(messageSpy.wait());
    QCOMPARE(messageSpy.size(), 2);
    QCOMPARE(messageSpy.at(0).at(0).toJsonObject().value(QStringLiteral("title")).toString(), QStringLiteral("One"));
    QCOMPARE(messageSpy.at(1).at(0).toJsonObject().value(QStringLiteral("title")).toString(), QStringLiteral("Two"));
}

void NtfySubscriberTest::connectionCloseTriggersReconnectWithLastSince()
{
    NtfyStreamServer server;
    QNetworkAccessManager manager;
    NtfySubscriber subscriber(manager, QStringLiteral("http://127.0.0.1:%1").arg(server.port()),
                               QStringLiteral("mytopic"), nullptr, /*reconnectDelayMs=*/10);
    QSignalSpy messageSpy(&subscriber, &NtfySubscriber::messageReceived);
    QSignalSpy lostSpy(&subscriber, &NtfySubscriber::connectionLost);

    subscriber.start(0);
    QVERIFY(server.waitForAtLeastConnections(1));
    QVERIFY(server.anyRequestFromContains(0, "since=0"));

    server.broadcast(
        QByteArray(
            R"({"id":"a1","time":1751928000,"event":"message","topic":"mytopic","title":"Hi","message":"Hello"})")
        + "\n");
    QVERIFY(messageSpy.wait());

    const int connectionsBeforeReconnect = server.readyRequestCount();
    server.closeAll();
    QVERIFY(lostSpy.wait());

    QVERIFY(server.waitForAtLeastConnections(connectionsBeforeReconnect + 1));
    QVERIFY(server.anyRequestFromContains(connectionsBeforeReconnect, "since=1751928000"));
}

QTEST_GUILESS_MAIN(NtfySubscriberTest)
#include "NtfySubscriberTest.moc"
