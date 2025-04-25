#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "apimanager.h"
#include <QJsonArray>
#include <QListWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_clicked();       //slot do przycisku Pobierania Danych
    void on_stationListWidget_itemClicked(QListWidgetItem *item);
    void on_pushButton_2_clicked();     //slot do przycisku Wczytywania zapisanych danych stacji
    void on_drawButton_clicked();  // slot od dokładnego wykresu

    void on_exportButton_clicked();

private:
    Ui::MainWindow *ui;

    ApiManager *apiManager;
    void showStationsInList(const QString &json);
    QJsonArray stationArray;
    QStringList measurementResults;
    int totalSensorsExpected = 0;   //Dla więcej niz 3 czujniki
    int sensorsReceived = 0;        //Dla więcej niż 3 czujniki
    void drawChart(const QString &paramCode, const QJsonArray &values);
    QMap<QString, QJsonArray> sensorDataMap; // paramCode → dane

    QString lastMeasurementJson;
    int lastStationId = -1;
};
#endif // MAINWINDOW_H
