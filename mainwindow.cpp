#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <QFile>
#include <QTextStream>
#include <QListWidgetItem>
#include <QTimer>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <limits>
#include <QFileDialog>
#include <QtConcurrent>
#include <QWebEngineView>

/**
 * @brief Konstruktor okna głównego.
 * @details Inicjalizuje interfejs, ustawia domyślne daty i konfiguruje połączenia sygnałów.
 * @param parent Wskaźnik na nadrzędny widget.
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("Dane o pogodzie");

    apiManager = &ApiManager::instance();
    connect(apiManager, &ApiManager::sensorCountReceived, this, [=](int count){
        totalSensorsExpected = count;
    });
    connect(apiManager, &ApiManager::apiReplyReceived, this, [=](const QString &json) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError) {
            QMessageBox::warning(this, "Błąd", "Nieprawidłowy format JSON: " + err.errorString());
            return;
        }

        if (doc.isArray() && json.contains("stationName")) {
            showStationsInList(json);
        }
        else if (doc.isObject() && doc.object().contains("values")) {
            QJsonObject obj = doc.object();
            QString paramCode = obj["key"].toString();
            QJsonArray values = obj["values"].toArray();

            if (ui->paramListWidget->findItems(paramCode, Qt::MatchExactly).isEmpty()) {
                ui->paramListWidget->addItem(paramCode);
            }

            QString result = QString("• %1:\n").arg(paramCode);
            int count = 0;

            for (const QJsonValue &val : values) {
                QJsonObject v = val.toObject();
                QString date = v["date"].toString();
                if (!v["value"].isNull()) {
                    double value = v["value"].toDouble();
                    result += QString("  %1 → %2 µg/m³\n").arg(date, QString::number(value));
                    if (++count >= 3) break;
                }
            }

            if (count == 0)
                result += "  brak danych\n";

            measurementResults << result;
            sensorDataMap[paramCode] = values;
            sensorsReceived++;
            lastMeasurementJson = json;

            if (sensorsReceived == totalSensorsExpected && totalSensorsExpected > 0) {
                QString fullText = "Dane pomiarowe ze stacji:\n\n" + measurementResults.join("\n");
                QMessageBox::information(this, "Dane pomiarowe", fullText);
                measurementResults.clear();
            }
        } else if (doc.isArray()) {
            ui->paramListWidget->clear();
            QJsonArray sensors = doc.array();
            for (const QJsonValue &val : sensors) {
                QJsonObject sensor = val.toObject();
                QString paramCode = sensor["param"]["paramCode"].toString();
                if (!paramCode.isEmpty() && ui->paramListWidget->findItems(paramCode, Qt::MatchExactly).isEmpty()) {
                    ui->paramListWidget->addItem(paramCode);
                }
            }
        } else {
            qDebug() << "Nieznany format danych JSON";
        }
    });

    connect(ui->drawButton, &QPushButton::clicked, this, &MainWindow::on_drawButton_clicked);
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::on_refreshButton_clicked);

    ui->startDateTimeEdit->setDateTime(QDateTime(QDate::currentDate().addDays(-7), QTime::currentTime()));
    ui->endDateTimeEdit->setDateTime(QDateTime(QDate::currentDate(), QTime::currentTime()));

    // Dodaj pole do filtrowania stacji
    cityFilterLineEdit = new QLineEdit(this);
    cityFilterLineEdit->setPlaceholderText("Filtruj po mieście...");
    cityFilterLineEdit->setGeometry(0, 430, 331, 30);
    connect(cityFilterLineEdit, &QLineEdit::textChanged, this, [=](const QString &text) {
        filterStationsByCity(text);
    });

    // Dodaj przycisk do mapy
    QPushButton *mapButton = new QPushButton("Pokaż mapę", this);
    mapButton->setGeometry(340, 360, 191, 41);
    connect(mapButton, &QPushButton::clicked, this, &MainWindow::showMap);
}

/**
 * @brief Destruktor okna głównego.
 * @details Zwalnia zasoby interfejsu.
 */
MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief Obsługuje kliknięcie przycisku pobierania danych.
 */
void MainWindow::on_pushButton_clicked()
{
    apiManager->getAirStations();
}

/**
 * @brief Wyświetla stacje w liście.
 * @param json Dane JSON z listą stacji.
 */
void MainWindow::showStationsInList(const QString &json)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "Błąd", "Nieprawidłowy format JSON.");
        return;
    }

    if (!doc.isArray()) {
        QMessageBox::warning(this, "Błąd", "Plik nie zawiera listy stacji.");
        return;
    }

    stationArray = doc.array();
    filterStationsByCity(cityFilterLineEdit->text());
}

/**
 * @brief Obsługuje kliknięcie na stację w liście.
 * @param item Wybrany element listy.
 */
void MainWindow::on_stationListWidget_itemClicked(QListWidgetItem *item)
{
    int index = ui->stationListWidget->row(item);

    if (index >= 0 && index < stationArray.size()) {
        QJsonObject station = stationArray[index].toObject();

        QString info;
        info += "Nazwa: " + station["stationName"].toString() + "\n";
        info += "ID: " + QString::number(station["id"].toInt()) + "\n";
        lastStationId = station["id"].toInt();

        QJsonObject city = station["city"].toObject();
        info += "Miasto: " + city["name"].toString() + "\n";

        QJsonObject commune = city["commune"].toObject();
        info += "Województwo: " + commune["provinceName"].toString() + "\n";
        info += "Powiat: " + commune["districtName"].toString() + "\n";

        QMessageBox::information(this, "Szczegóły stacji", info);

        measurementResults.clear();
        sensorsReceived = 0;
        totalSensorsExpected = 0;
        drawnCharts.clear();
        apiManager->getSensorsForStation(lastStationId);
    }
}

/**
 * @brief Obsługuje wczytywanie zapisanych danych stacji.
 */
void MainWindow::on_pushButton_2_clicked()
{
    QFile file("stacje.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Błąd", "Nie udało się otworzyć pliku.");
        return;
    }

    QString data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "Błąd", "Nie udało się sparsować pliku JSON.");
        return;
    }

    if (!doc.isArray()) {
        QMessageBox::warning(this, "Błąd", "Plik nie zawiera listy stacji.");
        return;
    }
    showStationsInList(data);
}

/**
 * @brief Rysuje wykres dla wybranych parametrów.
 */
void MainWindow::drawChart()
{
    QList<QListWidgetItem*> selectedItems = ui->paramListWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Brak wyboru", "Wybierz przynajmniej jeden parametr.");
        return;
    }

    QChart *chart = new QChart();
    chart->setTitle("Wykres parametrów pomiarowych");

    QDateTime startDate = ui->startDateTimeEdit->dateTime();
    QDateTime endDate = ui->endDateTimeEdit->dateTime();

    QList<QColor> colors = {Qt::red, Qt::blue, Qt::green, Qt::magenta, Qt::cyan, Qt::darkYellow, Qt::gray};
    int colorIndex = 0;

    for (QListWidgetItem* item : selectedItems) {
        QString paramCode = item->text();
        if (!sensorDataMap.contains(paramCode)) {
            continue;
        }

        QLineSeries *series = new QLineSeries();
        series->setName(paramCode);
        series->setColor(colors[colorIndex % colors.size()]);
        colorIndex++;

        QJsonArray values = sensorDataMap[paramCode];
        if (values.isEmpty()) {
            qDebug() << "Brak danych dla parametru:" << paramCode;
            continue;
        }

        for (const QJsonValue &val : values) {
            QJsonObject v = val.toObject();
            if (!v["value"].isNull()) {
                QDateTime dt = QDateTime::fromString(v["date"].toString(), Qt::ISODate);
                double value = v["value"].toDouble();

                if (dt.isValid() && dt >= startDate && dt <= endDate) {
                    series->append(dt.toMSecsSinceEpoch(), value);
                }
            }
        }

        chart->addSeries(series);
    }

    if (chart->series().isEmpty()) {
        QMessageBox::warning(this, "Brak danych", "Brak danych do wyświetlenia dla wybranych parametrów.");
        delete chart;
        return;
    }

    QDateTimeAxis *axisX = new QDateTimeAxis;
    axisX->setFormat("dd.MM HH:mm");
    axisX->setTitleText("Data");
    chart->addAxis(axisX, Qt::AlignBottom);

    QValueAxis *axisY = new QValueAxis;
    axisY->setTitleText("Stężenie (µg/m³)");
    chart->addAxis(axisY, Qt::AlignLeft);

    for (QAbstractSeries* series : chart->series()) {
        series->attachAxis(axisX);
        series->attachAxis(axisY);
    }

    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    for (QAbstractSeries* series : chart->series()) {
        QLineSeries *lineSeries = qobject_cast<QLineSeries*>(series);
        for (const QPointF &point : lineSeries->points()) {
            minY = qMin(minY, point.y());
            maxY = qMax(maxY, point.y());
        }
    }
    axisY->setRange(minY * 0.9, maxY * 1.1);

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    currentChartView = new QChartView(chart);
    currentChartView->setRenderHint(QPainter::Antialiasing);

    ChartWindow *chartWindow = new ChartWindow("Wykres parametrów");
    chartWindow->setCentralWidget(currentChartView);
    chartWindow->resize(800, 600);
    chartWindow->setWindowTitle("Wykres parametrów");
    chartWindow->setOnCloseCallback([this](const QString &paramCode) {
        openCharts.remove(paramCode);
    });
    openCharts["Wykres parametrów"] = chartWindow;
    chartWindow->show();
}

/**
 * @brief Obsługuje rysowanie wykresu po kliknięciu przycisku.
 */
void MainWindow::on_drawButton_clicked()
{
    QList<QListWidgetItem*> selectedItems = ui->paramListWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Brak wyboru", "Wybierz przynajmniej jeden parametr.");
        return;
    }

    QDateTime startDate = ui->startDateTimeEdit->dateTime();
    QDateTime endDate = ui->endDateTimeEdit->dateTime();
    if (startDate >= endDate) {
        QMessageBox::warning(this, "Błędna data", "Data początkowa musi być wcześniejsza niż końcowa.");
        return;
    }

    drawChart();
}

/**
 * @brief Obsługuje eksport danych pomiarowych.
 */
void MainWindow::on_exportButton_clicked()
{
    if (lastStationId == -1) {
        QMessageBox::warning(this, "Błąd", "Nie wybrano stacji do eksportu danych.");
        return;
    }

    if (lastMeasurementJson.isEmpty()) {
        QMessageBox::warning(this, "Brak danych", "Brak danych do zapisania.");
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(lastMeasurementJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, "Błąd", "Niepoprawny format danych JSON.");
        return;
    }

    QJsonObject obj = doc.object();
    QString param = obj.value("key").toString();
    QJsonArray values = obj.value("values").toArray();

    QString jsonFilename = QString("pomiar_%1_stacja_%2.json").arg(param).arg(lastStationId);
    QFile jsonFile(jsonFilename);
    if (jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&jsonFile);
        out << lastMeasurementJson;
        jsonFile.close();
    }

    QString csvFilename = QString("pomiar_%1_stacja_%2.csv").arg(param).arg(lastStationId);
    QFile csvFile(csvFilename);
    if (csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&csvFile);
        out << "Data;Wartość\n";
        for (const QJsonValue &v : values) {
            QJsonObject o = v.toObject();
            QString date = o.value("date").toString();
            QJsonValue val = o.value("value");
            if (!val.isNull()) {
                out << QString("%1;%2\n").arg(date, QString::number(val.toDouble()));
            }
        }
        csvFile.close();
    }

    QMessageBox::information(this, "Zapisano", "Dane zapisano do:\n" + jsonFilename + "\ni\n" + csvFilename);
}

/**
 * @brief Obsługuje analizę danych pomiarowych.
 */
void MainWindow::on_analyzeButton_clicked()
{
    QList<QListWidgetItem*> selectedItems = ui->paramListWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Błąd", "Wybierz przynajmniej jeden parametr do analizy.");
        return;
    }

    QString analysis;
    for (QListWidgetItem* item : selectedItems) {
        QString selectedParam = item->text();
        if (!sensorDataMap.contains(selectedParam)) {
            analysis += QString("Brak danych dla parametru %1.\n\n").arg(selectedParam);
            continue;
        }

        QJsonArray data = sensorDataMap[selectedParam];
        if (data.isEmpty()) {
            analysis += QString("Brak pomiarów dla parametru %1.\n\n").arg(selectedParam);
            continue;
        }

        double minValue = std::numeric_limits<double>::max();
        double maxValue = std::numeric_limits<double>::lowest();
        double sum = 0.0;
        int count = 0;
        int exceedances = 0;

        for (const QJsonValue &val : data) {
            QJsonObject v = val.toObject();
            if (!v["value"].isNull()) {
                double value = v["value"].toDouble();
                if (value < minValue) minValue = value;
                if (value > maxValue) maxValue = value;
                sum += value;
                ++count;

                if (selectedParam == "PM10" && value > 25.0) {
                    ++exceedances;
                }
            }
        }

        if (count == 0) {
            analysis += QString("Brak dostępnych danych pomiarowych dla %1.\n\n").arg(selectedParam);
            continue;
        }

        double avg = sum / count;
        double exceedancePercent = (count > 0) ? (100.0 * exceedances / count) : 0.0;

        analysis += QString(
                        "Analiza danych dla parametru: %1\n"
                        "Liczba pomiarów: %2\n"
                        "Wartość minimalna: %3 µg/m³\n"
                        "Wartość maksymalna: %4 µg/m³\n"
                        "Średnia wartość: %5 µg/m³\n"
                        ).arg(selectedParam)
                        .arg(count)
                        .arg(QString::number(minValue, 'f', 2))
                        .arg(QString::number(maxValue, 'f', 2))
                        .arg(QString::number(avg, 'f', 2));

        if (selectedParam == "PM10") {
            analysis += QString(
                            "Przekroczenia progu 25 µg/m³: %1 razy\n"
                            "Procent przekroczeń: %2%\n"
                            ).arg(exceedances)
                            .arg(QString::number(exceedancePercent, 'f', 2));
        }

        // Obliczanie trendu
        auto calculateTrend = [](const QJsonArray &data) {
            double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
            int n = 0;
            for (int i = 0; i < data.size(); ++i) {
                QJsonObject v = data[i].toObject();
                if (!v["value"].isNull()) {
                    double y = v["value"].toDouble();
                    sumX += i;
                    sumY += y;
                    sumXY += i * y;
                    sumXX += i * i;
                    n++;
                }
            }
            if (n < 2) return 0.0;
            double slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);
            return slope;
        };

        double trend = calculateTrend(data);
        analysis += QString("Trend: %1\n\n").arg(trend > 0 ? "Wzrost" : trend < 0 ? "Spadek" : "Stabilny");
    }

    QMessageBox::information(this, "Analiza danych", analysis);
}

/**
 * @brief Obsługuje eksport wykresu do pliku.
 */
void MainWindow::on_exportChartButton_clicked()
{
    if (!currentChartView) {
        QMessageBox::warning(this, "Błąd", "Nie ma otwartego wykresu do eksportu.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Zapisz wykres jako...",
                                                    "wykres_parametrow.png",
                                                    "Obraz PNG (*.png)");
    if (fileName.isEmpty())
        return;

    QPixmap pixmap(currentChartView->size());
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    currentChartView->render(&painter);
    painter.end();

    if (pixmap.save(fileName)) {
        QMessageBox::information(this, "Sukces", "Wykres zapisano do pliku:\n" + fileName);
    } else {
        QMessageBox::warning(this, "Błąd", "Nie udało się zapisać pliku.");
    }
}

/**
 * @brief Obsługuje odświeżanie danych pomiarowych.
 */
void MainWindow::on_refreshButton_clicked()
{
    if (lastStationId == -1) {
        QMessageBox::warning(this, "Błąd", "Nie wybrano stacji do odświeżenia.");
        return;
    }

    measurementResults.clear();
    sensorsReceived = 0;
    totalSensorsExpected = 0;
    sensorDataMap.clear();
    ui->paramListWidget->clear();
    drawnCharts.clear();
    currentChartView = nullptr;

    QMessageBox::information(this, "Odświeżanie", "Rozpoczęto odświeżanie danych dla wybranej stacji.");
    apiManager->getSensorsForStation(lastStationId);
}

/**
 * @brief Filtruje stacje według nazwy miasta.
 * @param cityName Nazwa miasta do filtrowania.
 */
void MainWindow::filterStationsByCity(const QString &cityName)
{
    ui->stationListWidget->clear();
    = 0;
    for (const QJsonValue &val : stationArray) {
        QJsonObject obj = val.toObject();
        QJsonObject city = obj["city"].toObject();
        if (city["name"].toString().contains(cityName, Qt::CaseInsensitive)) {
            ui->stationListWidget->addItem(obj["stationName"].toString());
        }
    }
}

/**
 * @brief Wyświetla mapę z lokalizacjami stacji.
 */
void MainWindow::showMap()
{
    QWebEngineView *mapView = new QWebEngineView(this);
    QString html = "<html><body><iframe width='800' height='600' src='https://www.openstreetmap.org/export/embed.html?bbox=14.0,49.0,24.0,54.0&layer=mapnik'></iframe></body></html>";
    mapView->setHtml(html);
    QMainWindow *mapWindow = new QMainWindow(this);
    mapWindow->setCentralWidget(mapView);
    mapWindow->resize(800, 600);
    mapWindow->setWindowTitle("Mapa stacji");
    mapWindow->show();
}
