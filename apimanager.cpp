#include "apimanager.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

ApiManager::ApiManager(QObject *parent)
    : QObject(parent),
    manager(new QNetworkAccessManager(this))
{
    connect(manager, &QNetworkAccessManager::finished,
            this, &ApiManager::onReplyFinished);
}

void ApiManager::getAirStations()
{
    QUrl url("https://api.gios.gov.pl/pjp-api/rest/station/findAll");
    QNetworkRequest request(url);
    manager->get(request);
}

/*
void ApiManager::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Błąd pobierania:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QString data = reply->readAll();

    // Zapis do pliku JSON
    QFile file("stacje.json");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << data;
        file.close();
        qDebug() << "Dane zapisane do stacje.json";
    } else {
        qDebug() << "Nie udało się zapisać pliku";
    }

    emit apiReplyReceived(data);  // wyślij do GUI
    reply->deleteLater();
}
*/
//Zamieniamy treść tej funkcji z powodu jej nadpisywania danych pomiarowych do pliku stacje.json, co usuwa nasze dane o stacjach
void ApiManager::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Błąd pobierania:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QString data = reply->readAll();

    // Sprawdź typ odpowiedzi
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Nie udało się sparsować JSON:" << parseError.errorString();
        reply->deleteLater();
        return;
    }

    QString filename;

    // Jeśli to lista sensorów
    if (doc.isArray() && !data.contains("stationName")) {
        QJsonArray sensors = doc.array();

        for (const QJsonValue &val : sensors) {
            QJsonObject sensor = val.toObject();
            int sensorId = sensor["id"].toInt();

            QString url = QString("https://api.gios.gov.pl/pjp-api/rest/data/getData/%1").arg(sensorId);
            QUrl qurl(url);
            QNetworkRequest request(qurl);
            manager->get(request);   // odpowiedź wróci znowu tutaj
        }

        emit sensorCountReceived(sensors.size());
        reply->deleteLater();
        return;
    }

    // Jeśli to lista stacji
    else if (doc.isArray()) {
        filename = "stacje.json";
    }

    // Jeśli to dane pomiarowe
    else if (doc.isObject() && doc.object().contains("values")) {
        QJsonObject obj = doc.object();
        QString paramCode = obj["key"].toString(); // np. "PM10"
        QJsonArray values = obj["values"].toArray();
        int stationId = obj.value("id").toInt();   // może nie działać zawsze, ale próbujemy

        if (stationId == 0) {
            // Awaryjnie: użyj timestamp, jeśli brak ID
            filename = QString("pomiar_%1.json").arg(QDateTime::currentSecsSinceEpoch());
        } else {
            filename = QString("pomiar_stacja_%1_%2.json").arg(stationId).arg(paramCode);
        }
    }

    // Nieznany format
    else {
        qDebug() << "Nieznany typ danych z API.";
        reply->deleteLater();
        return;
    }

    // Zapisz dane do pliku
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << data;
        file.close();
        qDebug() << "Dane zapisane do:" << filename;
    } else {
        qDebug() << "Nie udało się zapisać pliku:" << filename;
    }

    emit apiReplyReceived(data);  // wyślij dane dalej do GUI
    reply->deleteLater();
}

void ApiManager::readSavedStations()
{
    QFile file("stacje.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Nie można otworzyć pliku stacje.json";
        return;
    }

    QString data = file.readAll();
    file.close();

    // Spróbuj sparsować JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "Błąd parsowania JSON:" << parseError.errorString();
        return;
    }

    if (!doc.isArray()) {
        qDebug() << "Plik JSON nie zawiera tablicy";
        return;
    }

    QJsonArray stations = doc.array();
    qDebug() << "Liczba stacji pomiarowych w pliku:" << stations.size();

    // Przykład, wypisanie kilku nazw
    for (int i = 0; i < std::min(5, static_cast<int>(stations.size())); ++i) {
        QJsonObject station = stations[i].toObject();
        QString name = station["stationName"].toString();
        qDebug() << "Stacja:" << name;
    }
}
void ApiManager::getMeasurementsForStation(int stationId)
{
    QString urlString = QString("https://api.gios.gov.pl/pjp-api/rest/data/getData/%1").arg(stationId);
    QUrl url(urlString);
    QNetworkRequest request(url);
    manager->get(request);
}
void ApiManager::getSensorsForStation(int stationId)
{
    QString url = QString("https://api.gios.gov.pl/pjp-api/rest/station/sensors/%1").arg(stationId);
    QUrl qurl(url);
    QNetworkRequest request(qurl);
    manager->get(request);
}
