#include <QtTest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

/**
 * @brief Klasa testująca funkcjonalności ApiManager.
 */
class TestApiManager : public QObject {
    Q_OBJECT
private slots:
    /**
     * @brief Testuje parsowanie poprawnego JSON z listą stacji.
     */
    void testParseJson() {
        QString json = "[{\"stationName\": \"Test Station\", \"id\": 1}]";
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QVERIFY(doc.isArray());
        QCOMPARE(doc.array().size(), 1);
        QCOMPARE(doc.array()[0].toObject()["stationName"].toString(), "Test Station");
    }

    /**
     * @brief Testuje parsowanie nieprawidłowego JSON.
     */
    void testParseInvalidJson() {
        QString json = "{invalid}";
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
        QVERIFY(parseError.error != QJsonParseError::NoError);
    }

    /**
     * @brief Testuje parsowanie danych pomiarowych.
     */
    void testParseMeasurementJson() {
        QString json = "{\"key\": \"PM10\", \"values\": [{\"date\": \"2023-10-01\", \"value\": 25.0}]}";
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QVERIFY(doc.isObject());
        QJsonObject obj = doc.object();
        QCOMPARE(obj["key"].toString(), "PM10");
        QJsonArray values = obj["values"].toArray();
        QCOMPARE(values.size(), 1);
        QCOMPARE(values[0].toObject()["value"].toDouble(), 25.0);
    }
};

//QTEST_MAIN(TestApiManager)
