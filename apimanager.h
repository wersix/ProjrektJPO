#ifndef APIMANAGER_H
#define APIMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ApiManager : public QObject
{
    Q_OBJECT
public:
    explicit ApiManager(QObject *parent = nullptr);

    void getAirStations();  // Funkcja do pobrania stacji pomiarowych
    void readSavedStations();
    void getMeasurementsForStation(int stationId);
    void getSensorsForStation(int stationId);
    void drawChart(const QString &paramName, const QJsonArray &values);

signals:
    void apiReplyReceived(const QString &reply);
    void sensorCountReceived(int count);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *manager;
};

#endif // APIMANAGER_H
