#include "net/NetworkError.h"

#include <QTest>

class NetworkErrorTest : public QObject
{
    Q_OBJECT

private slots:
    void mapsStatusCodesToExpectedErrors();
};

void NetworkErrorTest::mapsStatusCodesToExpectedErrors()
{
    const QVector<QPair<int, std::optional<NetworkError>>> cases = {
        {200, std::nullopt},
        {299, std::nullopt},
        {300, NetworkError::Server},
        {401, NetworkError::Unauthorized},
        {403, NetworkError::Unauthorized},
        {409, NetworkError::Conflict},
        {429, NetworkError::RateLimited},
        {503, NetworkError::ServiceUnavailable},
        {500, NetworkError::Server},
        {404, NetworkError::Server},
    };

    for (const auto& [statusCode, expected] : cases) {
        const std::optional<NetworkError> actual = networkErrorFromStatusCode(statusCode);
        QCOMPARE(actual.has_value(), expected.has_value());
        if (expected.has_value())
            QCOMPARE(*actual, *expected);
    }
}

QTEST_APPLESS_MAIN(NetworkErrorTest)
#include "NetworkErrorTest.moc"
