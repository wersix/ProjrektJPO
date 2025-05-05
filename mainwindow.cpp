#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <QFile>
#include <QTextStream> // do zapisu stacji w pliku
#include <QListWidgetItem>
#include <QTimer>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <limits>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("Dane o pogodzie");

    apiManager = new ApiManager(this);
    /*
    connect(apiManager, &ApiManager::apiReplyReceived, this, [=](const QString &json){
        showStationsInList(json);  // dodajemy dane do GUI
    });
    */
    connect(apiManager, &ApiManager::sensorCountReceived, this, [=](int count){
        totalSensorsExpected = count;
    });
    connect(apiManager, &ApiManager::apiReplyReceived, this, [=](const QString &json) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError)
            return;

        if (doc.isArray()) {
            showStationsInList(json);  // lista stacji
        }
        else if (doc.isObject() && doc.object().contains("values")) {
            QJsonObject obj = doc.object();
            QString paramCode = obj["key"].toString();  // np. PM10
            QJsonArray values = obj["values"].toArray();

        if (ui->paramListWidget->findItems(paramCode, Qt::MatchExactly).isEmpty()) {
            ui->paramListWidget->addItem(paramCode);
        }
        // To było do ComboBox
            /*
            if (ui->sensorComboBox->findText(paramCode) == -1) {
                ui->sensorComboBox->addItem(paramCode);
            } */// Do rysowania wykresu
            qDebug() << "Dodano dane do mapy dla czujnika:" << paramCode;       // TEST do wykresu
            qDebug() << "Liczba pomiarów:" << values.size();                    // TEST do wykresu

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
            //drawChart(paramCode, values);
            sensorDataMap[paramCode] = values;
            sensorsReceived++;
            lastMeasurementJson = json; // zapamiętaj dane do eksportu

            if (sensorsReceived == totalSensorsExpected && totalSensorsExpected > 0) {
                QString fullText = "Dane pomiarowe ze stacji:\n\n" + measurementResults.join("\n");
                QMessageBox::information(this, "Dane pomiarowe", fullText);
                measurementResults.clear();
            }
        } else {
            qDebug() << "Nieznany format danych JSON";  //TEST (głównie, bo pomiary PM10 nie działąły i co się dokładnie dzieje
        }
    });

    //NIE POTRZEBA connect(ui...) BO Qt SAM JUŻ TO ROBI
    /*
    connect(ui->stationListWidget, &QListWidget::itemClicked,
            this, &MainWindow::on_stationListWidget_itemClicked);
    qDebug() << "Podpięto sygnał apiReplyReceived";
    */
    connect(ui->drawButton, &QPushButton::clicked,
            this, &MainWindow::on_drawButton_clicked);
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::on_refreshButton_clicked);

    ui->startDateTimeEdit->setDateTime(QDateTime(QDate::currentDate().addDays(-7), QTime::currentTime()));       // domyślnie 7 dni wstecz
    ui->endDateTimeEdit->setDateTime(QDateTime(QDate::currentDate(), QTime::currentTime()));                     // domyślnie dziś

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{

    //QMessageBox :: information(this, "Inf.", "Kliknięto przycisk");
    apiManager->getAirStations();
}

void MainWindow::showStationsInList(const QString &json)
{
    static int counter = 0;
    ++counter;
    qDebug() << "Funkcja showStationsInList została wywołana — ile = " << counter;  // TEST na wywołanie listy z pliku json

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

    QJsonArray stations = doc.array();
    stationArray = stations;  // zapamiętaj całą tablicę stacji

    ui->stationListWidget->clear();  // wyczyść listę

    for (const QJsonValue &val : stations) {
        QJsonObject obj = val.toObject();
        QString name = obj["stationName"].toString();
        if (!name.isEmpty())
            ui->stationListWidget->addItem(name);
    }
}
void MainWindow::on_stationListWidget_itemClicked(QListWidgetItem *item)
{
    //qDebug() << "Kliknięto stację:" << item->text();
    static int clickCount = 0;
    ++clickCount;
    qDebug() << "Kliknięcie " << clickCount << "na stację:" << item->text();

    int index = ui->stationListWidget->row(item);

    if (index >= 0 && index < stationArray.size()) {
        ui->paramListWidget->clear();
        // ui->sensorComboBox->addItem("TestPM10");  // testowy wpis
        QJsonObject station = stationArray[index].toObject();
        /*
        // W on_stationListWidget_itemClicked, po pobraniu danych czujników sensors to lista czujników z API
        for (const auto &sensor : sensors) {
            QString paramCode = sensor["param"]["paramCode"].toString(); // Przykład: PM10
            if (ui->paramListWidget->findItems(paramCode, Qt::MatchExactly).isEmpty()) {
                ui->paramListWidget->addItem(paramCode);
            }
        }
        */

        QString info;
        info += "Nazwa: " + station["stationName"].toString() + "\n";
        info += "ID: " + QString::number(station["id"].toInt()) + "\n";
        lastStationId = station["id"].toInt();      //To tylko do zapisu nr id stacji po wciśnięciu button zapisu pomiarów

        QJsonObject city = station["city"].toObject();
        info += "Miasto: " + city["name"].toString() + "\n";

        QJsonObject commune = city["commune"].toObject();
        info += "Województwo: " + commune["provinceName"].toString() + "\n";
        info += "Powiat: " + commune["districtName"].toString() + "\n";

        QMessageBox::information(this, "Szczegóły stacji", info);

        int stationId = station["id"].toInt();
        //apiManager->getMeasurementsForStation(stationId);
        measurementResults.clear();
        sensorsReceived = 0;  // zaczynamy od zera              || Dla wiecej niz 3 czujniki
        totalSensorsExpected = 0;  // jeszcze nie wiemy         || Dla wiecej niz 3 czujniki
        //isChartDrawn = false;        // lepszy sposób poniżej
        drawnCharts.clear();
        apiManager->getSensorsForStation(stationId);
        connect(apiManager, &ApiManager::apiReplyReceived, this, [=](const QString &json) {
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
            if (err.error != QJsonParseError::NoError)
                return;

            if (doc.isArray()) {
                ui->paramListWidget->clear();
                QJsonArray sensors = doc.array();
                for (const QJsonValue &val : sensors) {
                    QJsonObject sensor = val.toObject();
                    QString paramCode = sensor["param"]["paramCode"].toString(); // np. PM10
                    if (!paramCode.isEmpty() && ui->paramListWidget->findItems(paramCode, Qt::MatchExactly).isEmpty()) {
                        ui->paramListWidget->addItem(paramCode);
                    }
                }
            }
        });
    }
}

/*
void MainWindow::on_pushButton_2_clicked()
{
    QFile file("stacje.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Błąd", "Nie udało się otworzyć pliku.");
        return;
    }

    QString data = file.readAll();
    file.close();

    showStationsInList(data);  // pokazujemy dane z zapisanego pliku
}
*/
//Trzeba było użyć innego rozwiązania dla tego przycisku, ponieważ chciał pokazać pomiary, a nie liste stacji z GUI
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
        QMessageBox::warning(this, "Błąd", "Plik nie zawiera listy stacji (może zawierać dane pomiarowe).");
        return;
    }
    //isChartDrawn = false;
    showStationsInList(data);
}
/* do ComboBox
void MainWindow::drawChart(const QString &paramCode, const QJsonArray &values)
{
    qDebug() << "Rysuję wykres tylko raz dla:" << paramCode;

    QLineSeries *series = new QLineSeries();
    series->setName(paramCode);
    series->setColor(Qt::blue);

    QDateTimeAxis *axisX = new QDateTimeAxis;
    axisX->setFormat("dd.MM HH:mm");
    axisX->setTitleText("Data");

    QValueAxis *axisY = new QValueAxis;
    axisY->setTitleText("Stężenie (µg/m³)");

    //Pobieranie dat z interfejsu
    QDateTime startDate = ui->startDateTimeEdit->dateTime();
    QDateTime endDate = ui->endDateTimeEdit->dateTime();

    for (const QJsonValue &val : values) {
        QJsonObject v = val.toObject();
        if (!v["value"].isNull()) {
            QDateTime dt = QDateTime::fromString(v["date"].toString(), Qt::ISODate);
            double value = v["value"].toDouble();

            // Filtrowanie po dacie
            if (dt.isValid() && dt >= startDate && dt <= endDate) {
                series->append(dt.toMSecsSinceEpoch(), value);
            }
        }
    }

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    /*Zapis robił się tutaj automatycznie, teraz jest do tego osobny przycisk
    // Zapis wykresu jako pliku PNG
    QPixmap pixmap(chartView->size());
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    chartView->render(&painter);
    painter.end();
    QString fileName = QString("wykres_%1.png").arg(paramCode);
    pixmap.save(fileName);
    qDebug() << "Wykres zapisany do pliku:" << fileName;
    */
/*
    if (openCharts.contains(paramCode)) {
        openCharts[paramCode]->raise();
        openCharts[paramCode]->activateWindow();
        return;
    }

    // Użycie nowej klasy ChartWindow
    ChartWindow *chartWindow = new ChartWindow(paramCode);
    chartWindow->setCentralWidget(chartView);
    chartWindow->resize(600, 400);
    chartWindow->setWindowTitle("Wykres: " + paramCode);
    chartWindow->show();

    openCharts[paramCode] = chartWindow;
    qDebug() <<  "zapisane charty: " << openCharts;

    chartWindow->setOnCloseCallback([=](const QString &code) {
        openCharts.remove(code);
        qDebug() << "Wykres dla" << code << "zamknięty – usunięto z mapy.";
    });
}
*/
////////////////////////////
void MainWindow::drawChart() {
    // Sprawdź, czy są dane pomiarowe
    if (currentMeasurements.isEmpty()) {
        QMessageBox::information(this, "Brak danych", "Brak danych do wyświetlenia.");
        return;
    }

    // Utwórz nowy wykres
    QChart* chart = new QChart();
    chart->setTitle("Wykres parametrów pomiarowych");

    // Pobierz listę wybranych parametrów z QListWidget
    QList<QListWidgetItem*> selectedItems = ui->paramListWidget->selectedItems();

    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "Brak wyboru", "Wybierz przynajmniej jeden parametr.");
        return;
    }

    // Lista kolorów dla serii
    QList<QColor> colors = {
        Qt::red, Qt::blue, Qt::green, Qt::magenta, Qt::cyan, Qt::darkYellow, Qt::gray
    };
    int colorIndex = 0;

    // Tworzymy serię dla każdego parametru
    for (QListWidgetItem* item : selectedItems) {
        QString paramName = item->text();

        QLineSeries* series = new QLineSeries();
        series->setName(paramName);

        for (const auto& measurement : currentMeasurements) {
            QDateTime timestamp = measurement.timestamp;
            if (measurement.values.contains(paramName)) {
                qreal value = measurement.values[paramName];
                series->append(timestamp.toMSecsSinceEpoch(), value);
            }
        }

        series->setColor(colors[colorIndex % colors.size()]);
        colorIndex++;

        chart->addSeries(series);
    }

    // Konfiguracja osi X (czas)
    QDateTimeAxis* axisX = new QDateTimeAxis();
    axisX->setFormat("dd.MM hh:mm");
    axisX->setTitleText("Czas");
    chart->addAxis(axisX, Qt::AlignBottom);

    // Konfiguracja osi Y
    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("Wartość");
    chart->addAxis(axisY, Qt::AlignLeft);

    // Podłącz osie do każdej serii
    for (QAbstractSeries* series : chart->series()) {
        series->attachAxis(axisX);
        series->attachAxis(axisY);
    }

    // Wyświetl wykres
    chartView->setChart(chart);
}
////////////////////////////



//bool isChartDrawn = false;          // ponieważ wykres się rysuje 2 razy, dlatego robimy boola, żeby nie potrzebnie się nie nadrysowywał
void MainWindow::on_drawButton_clicked()
{
    /*Może to pod  rozwiąże problem podwójnych komunikatów?
    QString selectedParam = ui->sensorComboBox->currentText();
    if (selectedParam.isEmpty() || !sensorDataMap.contains(selectedParam)) {
        QMessageBox::warning(this, "Błąd", "Brak danych dla wybranego czujnika.");
        return;
    }
    qDebug() <<  "otwarte charty przed: " << openCharts;
    //if (openCharts.contains(selectedParam)) {
    //    QMessageBox::information(this, "Wykres", "Wykres już jest otwarty.");
    //    qDebug() <<  "otwarte charty: " << openCharts;
    //    return;
    //}

    QDateTime startDate = ui->startDateTimeEdit->dateTime();
    QDateTime endDate = ui->endDateTimeEdit->dateTime();

    if (startDate > endDate) {
        QMessageBox::warning(this, "Błąd daty", "Data początkowa nie może być późniejsza niż końcowa.");
        return;
    }

    drawnCharts.insert(selectedParam);  // zapamiętaj, że już był
    qDebug() <<  "narysowane charty: " << drawnCharts;

    QJsonArray data = sensorDataMap[selectedParam];
    drawChart(selectedParam, data);
    qDebug() <<  "koniec funkcji";
    return;
    qDebug() <<  "po returnie";
    */

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

    for (QListWidgetItem* item : selectedItems) {
        QString selectedSensor = item->text();
        if (!sensorDataMap.contains(selectedSensor)) {
            QMessageBox::warning(this, "Brak danych", QString("Brak danych pomiarowych dla %1.").arg(selectedSensor));
            continue;
        }
        if (!drawnCharts.contains(selectedSensor)) {
            drawChart(selectedSensor, sensorDataMap[selectedSensor]);
            drawnCharts.insert(selectedSensor);
        }
    }
}


void MainWindow::on_exportButton_clicked()
{
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

    // JSON eksport
    QString jsonFilename = QString("pomiar_%1_stacja_%2.json").arg(param).arg(lastStationId);
    QFile jsonFile(jsonFilename);
    if (jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&jsonFile);
        out << lastMeasurementJson;
        jsonFile.close();
    }

    // CSV eksport
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


void MainWindow::on_analyzeButton_clicked()
{
    QList<QListWidgetItem*> selectedItems = ui->paramListWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, "Błąd", "Wybierz parametr do analizy.");
        return;
    }
    // ANALIZUJE TYLKO PIERWSZY --> POPRAW!!!!!!!!!!!!!!!!!!!!!!!!!!!
    QString selectedParam = selectedItems.first()->text(); // Analizuj tylko pierwszy wybrany parametr
    if (!sensorDataMap.contains(selectedParam)) {
        QMessageBox::warning(this, "Błąd", "Brak danych dla wybranego czujnika.");
        return;
    }

    QJsonArray data = sensorDataMap[selectedParam];
    if (data.isEmpty()) {
        QMessageBox::warning(this, "Błąd", "Brak pomiarów do analizy.");
        return;
    }

    double minValue = std::numeric_limits<double>::max();
    double maxValue = std::numeric_limits<double>::lowest();
    double sum = 0.0;
    int count = 0;
    int exceedances = 0;  // liczba przekroczeń

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
        QMessageBox::information(this, "Analiza danych", "Brak dostępnych danych pomiarowych do analizy.");
        return;
    }

    double avg = sum / count;
    double exceedancePercent = (count > 0) ? (100.0 * exceedances / count) : 0.0;

    QString analysis = QString(
                           "Analiza danych dla parametru: %1\n\n"
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
                        "\nPrzekroczenia progu 25 µg/m³: %1 razy\n"
                        "Procent przekroczeń: %2%"
                        ).arg(exceedances)
                        .arg(QString::number(exceedancePercent, 'f', 2));
    }

    QMessageBox::information(this, "Analiza danych", analysis);
}

void MainWindow::on_exportChartButton_clicked()
{
    QString selectedParam = ui->sensorComboBox->currentText();
    if (selectedParam.isEmpty() || !openCharts.contains(selectedParam)) {
        QMessageBox::warning(this, "Błąd", "Nie znaleziono otwartego wykresu dla wybranego czujnika.");
        return;
    }

    ChartWindow *chartWindow = qobject_cast<ChartWindow *>(openCharts[selectedParam]);
    if (!chartWindow) {
        QMessageBox::warning(this, "Błąd", "Nieprawidłowe okno wykresu.");
        return;
    }

    QChartView *chartView = qobject_cast<QChartView *>(chartWindow->centralWidget());
    if (!chartView) {
        QMessageBox::warning(this, "Błąd", "Nie znaleziono widoku wykresu.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Zapisz wykres jako...",
                                                    QString("wykres_%1.png").arg(selectedParam),
                                                    "Obraz PNG (*.png)");
    if (fileName.isEmpty())
        return;  // Użytkownik anulował

    QPixmap pixmap(chartView->size());
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    chartView->render(&painter);
    painter.end();

    if (pixmap.save(fileName)) {
        QMessageBox::information(this, "Sukces", "Wykres zapisano do pliku:\n" + fileName);
    } else {
        QMessageBox::warning(this, "Błąd", "Nie udało się zapisać pliku.");
    }
}

void MainWindow::on_refreshButton_clicked()
{
    if (lastStationId == 0) {
        QMessageBox::warning(this, "Błąd", "Nie wybrano stacji do odświeżenia.");
        return;
    }

    // Zamknij wszystkie otwarte wykresy
    for (auto chartWindow : openCharts) {
        if (chartWindow) {
            chartWindow->close();
        }
    }
    openCharts.clear();

    measurementResults.clear();
    sensorsReceived = 0;
    totalSensorsExpected = 0;
    sensorDataMap.clear();
    ui->paramListWidget->clear();
    drawnCharts.clear();

    QMessageBox::information(this, "Odświeżanie", "Rozpoczęto odświeżanie danych dla wybranej stacji.");

    apiManager->getSensorsForStation(lastStationId);
}

