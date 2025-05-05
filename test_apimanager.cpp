#include <QtTest>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include "mainwindow.h"
#include "apimanager.h"

/**
 * @brief Klasa testująca funkcjonalności ApiManager oraz MainWindow.
 */
class TestApiManager : public QObject {
    Q_OBJECT

private:
    QApplication *app; // Potrzebne do inicjalizacji Qt dla GUI

public:
    TestApiManager() {
        // Inicjalizacja QApplication, wymagane dla elementów GUI w MainWindow
        int argc = 0;
        char *argv[] = { nullptr };
        app = new QApplication(argc, argv);
    }

    ~TestApiManager() {
        delete app;
    }

private slots:
    /**
     * @brief Inicjalizacja przed każdym testem.
     */
    void initTestCase() {
        // Można tu dodać dodatkową inicjalizację, jeśli potrzebna
    }

    /**
     * @brief Sprzątanie po każdym teście.
     */
    void cleanupTestCase() {
        // Można tu dodać sprzątanie, jeśli potrzebne
    }

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

    /**
     * @brief Testuje zapis i odczyt danych do/z pliku JSON.
     */
    void testSaveAndReadFile() {
        QString json = "[{\"stationName\": \"Test Station\", \"id\": 1}]";
        QFile file("test_stacje.json");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out << json;
        file.close();
        QVERIFY(file.exists());

        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QString readData = file.readAll();
        file.close();
        QCOMPARE(readData, json);

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(readData.toUtf8(), &parseError);
        QVERIFY(parseError.error == QJsonParseError::NoError);
        QVERIFY(doc.isArray());
        QCOMPARE(doc.array().size(), 1);
        QFile::remove("test_stacje.json");
    }

    /**
     * @brief Testuje filtrowanie stacji po nazwie miasta.
     */
    void testFilterStationsByCity() {
        MainWindow mainWindow;
        QString json = R"([
            {"stationName": "Stacja Warszawa", "id": 1, "city": {"name": "Warszawa"}},
            {"stationName": "Stacja Kraków", "id": 2, "city": {"name": "Kraków"}}
        ])";
        mainWindow.showStationsInList(json);

        // Filtruj stacje dla miasta "Warszawa"
        mainWindow.filterStationsByCity("Warszawa");
        QCOMPARE(mainWindow.getStationListCount(), 1);

        // Filtruj bez ograniczeń (wszystkie stacje)
        mainWindow.filterStationsByCity("");
        QCOMPARE(mainWindow.getStationListCount(), 2);

        // Przetwarzanie pętli zdarzeń, aby upewnić się, że GUI się zaktualizowało
        QCoreApplication::processEvents();
    }

    /**
     * @brief Testuje obsługę pustej odpowiedzi API.
     */
    void testEmptyApiResponse() {
        MainWindow mainWindow;
        mainWindow.showStationsInList("");
        QCOMPARE(mainWindow.getStationListCount(), 0);

        // Przetwarzanie pętli zdarzeń
        QCoreApplication::processEvents();
    }

    /**
     * @brief Testuje analizę danych dla parametru PM10.
     */
    void testAnalyzePM10() {
        MainWindow mainWindow;
        QString json = R"({"key": "PM10", "values": [
            {"date": "2023-10-01", "value": 20.0},
            {"date": "2023-10-02", "value": 30.0},
            {"date": "2023-10-03", "value": 25.0}
        ]})";
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
        QJsonArray values = doc.object()["values"].toArray();
        mainWindow.setTestData("PM10", values);

        // Symulacja kliknięcia przycisku analizy
        mainWindow.on_analyzeButton_clicked();

        // Przetwarzanie pętli zdarzeń
        QCoreApplication::processEvents();
    }
};

//QTEST_APPLESS_MAIN(TestApiManager)
