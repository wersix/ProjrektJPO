#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "apimanager.h"
#include <QJsonArray>
#include <QListWidgetItem>
#include <QSet>
#include <QtCharts/QChartView>
#include <QLineEdit>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @brief Główna klasa okna aplikacji do monitorowania jakości powietrza.
 * @details Odpowiada za interfejs graficzny, obsługę danych z API GIOŚ oraz wyświetlanie wykresów i analiz.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Konstruktor okna głównego.
     * @param parent Wskaźnik na nadrzędny widget.
     */
    explicit MainWindow(QWidget *parent = nullptr);
    /**
     * @brief Destruktor okna głównego.
     */
    ~MainWindow();

private slots:
    /**
     * @brief Slot dla przycisku pobierania danych o stacjach.
     */
    void on_pushButton_clicked();
    /**
     * @brief Slot dla kliknięcia na element listy stacji.
     * @param item Wskaźnik na wybrany element listy.
     */
    void on_stationListWidget_itemClicked(QListWidgetItem *item);
    /**
     * @brief Slot dla przycisku wczytywania zapisanych danych stacji.
     */
    void on_pushButton_2_clicked();
    /**
     * @brief Slot dla przycisku rysowania wykresu.
     */
    void on_drawButton_clicked();
    /**
     * @brief Slot dla przycisku eksportu danych.
     */
    void on_exportButton_clicked();
    /**
     * @brief Slot dla przycisku analizy danych.
     */
    void on_analyzeButton_clicked();
    /**
     * @brief Slot dla przycisku eksportu wykresu.
     */
    void on_exportChartButton_clicked();
    /**
     * @brief Slot dla przycisku odświeżania danych pomiarowych.
     */
    void on_refreshButton_clicked();

private:
    Ui::MainWindow *ui;
    ApiManager *apiManager;

    /**
     * @brief Wyświetla listę stacji w widżecie listy.
     * @param json Dane JSON zawierające listę stacji.
     */
    void showStationsInList(const QString &json);
    /**
     * @brief Rysuje wykres dla wybranych parametrów.
     */
    void drawChart();
    /**
     * @brief Filtruje stacje według nazwy miasta.
     * @param cityName Nazwa miasta do filtrowania.
     */
    void filterStationsByCity(const QString &cityName);
    /**
     * @brief Wyświetla mapę z lokalizacjami stacji.
     */
    void showMap();

    QJsonArray stationArray;
    QStringList measurementResults;
    int totalSensorsExpected = 0;   ///< Oczekiwana liczba czujników.
    int sensorsReceived = 0;        ///< Liczba odebranych czujników.
    QMap<QString, QJsonArray> sensorDataMap; ///< Mapa danych czujników (paramCode → dane).
    QString lastMeasurementJson;    ///< Ostatnie dane pomiarowe w formacie JSON.
    int lastStationId = -1;         ///< ID ostatnio wybranej stacji.
    QSet<QString> drawnCharts;      ///< Zbiór narysowanych wykresów.
    QMap<QString, QMainWindow*> openCharts; ///< Mapa otwartych okien wykresów.
    QChartView* currentChartView = nullptr; ///< Aktualny widok wykresu.
    QLineEdit* cityFilterLineEdit = nullptr; ///< Pole do filtrowania stacji po mieście.
};

/**
 * @brief Klasa okna wykresu.
 * @details Odpowiada za wyświetlanie wykresów parametrów pomiarowych.
 */
class ChartWindow : public QMainWindow
{
    Q_OBJECT
public:
    /**
     * @brief Konstruktor okna wykresu.
     * @param paramCode Kod parametru (np. PM10).
     * @param parent Wskaźnik na nadrzędny widget.
     */
    explicit ChartWindow(const QString &paramCode, QWidget *parent = nullptr)
        : QMainWindow(parent), m_paramCode(paramCode) {}

    /**
     * @brief Ustawia callback wywoływany przy zamykaniu okna.
     * @param callback Funkcja do wywołania.
     */
    void setOnCloseCallback(std::function<void(const QString &)> callback) {
        m_onClose = callback;
    }

protected:
    /**
     * @brief Obsługuje zdarzenie zamknięcia okna.
     * @param event Wskaźnik na zdarzenie zamknięcia.
     */
    void closeEvent(QCloseEvent *event) override {
        if (m_onClose) m_onClose(m_paramCode);
        QMainWindow::closeEvent(event);
    }

private:
    QString m_paramCode; ///< Kod parametru wykresu.
    std::function<void(const QString &)> m_onClose; ///< Callback dla zamknięcia.
};

#endif // MAINWINDOW_H
