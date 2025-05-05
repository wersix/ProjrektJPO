#include <QtTest>
class TestApiManager : public QObject {
    Q_OBJECT
private slots:
    void testParseJson() {
        QString json = "[{\"stationName\": \"Test Station\", \"id\": 1}]";
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QVERIFY(doc.isArray());
        QCOMPARE(doc.array().size(), 1);
        QCOMPARE(doc.array()[0].toObject()["stationName"].toString(), "Test Station");
    }
};
QTEST_MAIN(TestApiManager)
