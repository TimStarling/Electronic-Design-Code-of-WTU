#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QDateTime>
#include <QFileDialog>
#include <QTextCodec>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QInputDialog>
#include <QTextStream>
#include <QTextCursor>
#include <QGraphicsOpacityEffect>
#include <QEasingCurve>
#include <QPainter>
#include <QStyleOption>
#include <cmath>
#include <algorithm>
#include <QtGlobal>

class OscilloscopeWidget : public QWidget
{
public:
    struct Stats {
        double min = 0;
        double max = 0;
        double peakToPeak = 0;
        double rms = 0;
        double mean = 0;
        double period = 0;
        double freq = 0;
        double riseTime = 0;
        double fallTime = 0;
        double pulseWidth = 0;
        double duty = 0;
        bool hasPeriod = false;
        int samples = 0;
    };

    explicit OscilloscopeWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(240);
        setAutoFillBackground(true);
    }

    void configure(double sampleRate, double timeBaseMs, double gain, double vMin, double vMax)
    {
        m_sampleRate = std::max(1.0, sampleRate);
        m_timeBaseMs = std::max(0.1, timeBaseMs);
        m_gain = std::max(0.001, gain);
        m_vMin = vMin;
        m_vMax = vMax;
        update();
    }

    void setValues(const QVector<double> &values)
    {
        m_values = values;
        computeStats();
        update();
    }

    const Stats &stats() const { return m_stats; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const qreal leftMargin = 68.0;
        const qreal topMargin = 8.0;
        const qreal rightMargin = 8.0;
        const qreal bottomMargin = 8.0;
        QRectF rect = this->rect().adjusted(leftMargin, topMargin, -rightMargin, -bottomMargin);
        p.fillRect(rect, QColor("#ffffff"));

        // Grid
        p.setPen(QPen(QColor("#d1d1d6"), 1));
        const int divs = 10;
        for (int i = 0; i <= divs; ++i) {
            const double x = rect.left() + rect.width() * i / divs;
            p.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
            const double y = rect.top() + rect.height() * i / divs;
            p.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }

        // Left ruler labels
        double labelMin = m_vMin;
        double labelMax = m_vMax;
        QVector<double> visible = visibleValues();
        if (!visible.isEmpty()) {
            labelMin = *std::min_element(visible.begin(), visible.end());
            labelMax = *std::max_element(visible.begin(), visible.end());
        }
        double labelSpan = labelMax - labelMin;
        if (labelSpan < 1e-9) labelSpan = 1.0;
        p.setPen(QPen(QColor("#3a3a3c"), 1.2));
        const int ticks = 5;
        for (int i = 0; i <= ticks; ++i) {
            double t = static_cast<double>(i) / ticks;
            double y = rect.top() + rect.height() * t;
            double value = labelMax - t * (labelMax - labelMin);
            p.drawText(QRectF(4, y - 10, leftMargin - 12, 20), Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(value, 'f', 2) + " V");
        }

        if (visible.isEmpty()) {
            p.setPen(QPen(QColor("#8e8e93"), 1.2));
            p.drawText(rect, Qt::AlignCenter, QStringLiteral("等待波形数据..."));
            return;
        }

        const double minVal = *std::min_element(visible.begin(), visible.end());
        const double maxVal = *std::max_element(visible.begin(), visible.end());
        const double span = std::max(1e-9, maxVal - minVal);

        p.setPen(QPen(QColor("#007aff"), 2));

        const int n = visible.size();
        for (int i = 0; i < n - 1; ++i) {
            const double t0 = static_cast<double>(i) / (n - 1);
            const double t1 = static_cast<double>(i + 1) / (n - 1);
            const double v0 = (visible[i] - minVal) / span;
            const double v1 = (visible[i + 1] - minVal) / span;
            QPointF p0(rect.left() + t0 * rect.width(),
                       rect.bottom() - v0 * rect.height());
            QPointF p1(rect.left() + t1 * rect.width(),
                       rect.bottom() - v1 * rect.height());
            p.drawLine(p0, p1);
        }
    }

private:
    QVector<double> visibleValues() const
    {
        const double totalTimeSec = (m_timeBaseMs / 1000.0) * 10.0; // 10 div
        const int samples = static_cast<int>(totalTimeSec * m_sampleRate);
        if (samples <= 0 || m_values.isEmpty()) return {};
        const int start = std::max(0, m_values.size() - samples);
        return m_values.mid(start);
    }

    void computeStats()
    {
        QVector<double> values = visibleValues();
        m_stats = Stats();
        if (values.isEmpty()) {
            return;
        }
        const int n = values.size();
        m_stats.samples = n;
        double minV = values.first();
        double maxV = values.first();
        double sum = 0;
        double sumSq = 0;
        for (double v : values) {
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
            sum += v;
            sumSq += v * v;
        }
        m_stats.min = minV;
        m_stats.max = maxV;
        m_stats.peakToPeak = maxV - minV;
        m_stats.mean = sum / n;
        m_stats.rms = std::sqrt(sumSq / n);

        const double dt = 1.0 / m_sampleRate;
        // Zero-crossing for period/freq
        double lastCross = -1;
        QVector<double> periods;
        for (int i = 1; i < n; ++i) {
            const double v0 = values[i - 1] - m_stats.mean;
            const double v1 = values[i] - m_stats.mean;
            if ((v0 <= 0 && v1 > 0) || (v0 >= 0 && v1 < 0)) {
                double frac = std::abs(v0 - v1) > 1e-9 ? std::abs(v0) / std::abs(v0 - v1) : 0.0;
                double t = (i - 1 + frac) * dt;
                if (lastCross >= 0) {
                    periods.append(t - lastCross);
                }
                lastCross = t;
            }
        }
        if (!periods.isEmpty()) {
            double avg = 0;
            for (double pVal : periods) avg += pVal;
            avg /= periods.size();
            m_stats.period = avg;
            m_stats.freq = (avg > 0) ? 1.0 / avg : 0;
            m_stats.hasPeriod = true;
        }

        // Rise/fall/pulse/duty (simple threshold method)
        const double highThresh = m_stats.min + 0.9 * (m_stats.peakToPeak);
        const double lowThresh = m_stats.min + 0.1 * (m_stats.peakToPeak);
        int firstLow = -1, firstHigh = -1, lastHigh = -1;
        double riseStart = -1, riseEnd = -1, fallStart = -1, fallEnd = -1;
        QVector<double> highDurations;
        QVector<double> risingEdges;
        double currentHighStart = -1;
        for (int i = 1; i < n; ++i) {
            double prev = values[i - 1];
            double curr = values[i];
            double t = i * dt;
            if (prev < lowThresh && curr >= lowThresh && firstLow < 0) {
                firstLow = i;
            }
            if (prev < highThresh && curr >= highThresh) {
                risingEdges.append(t);
                if (riseStart < 0) riseStart = (i - 1) * dt;
                riseEnd = t;
                currentHighStart = t;
            }
            if (prev > highThresh && curr <= highThresh) {
                fallStart = (i - 1) * dt;
                fallEnd = t;
                if (currentHighStart >= 0) {
                    highDurations.append(t - currentHighStart);
                }
                currentHighStart = -1;
                lastHigh = i;
            }
            if (prev < highThresh && curr >= highThresh && firstHigh < 0) {
                firstHigh = i;
            }
        }
        if (riseStart >= 0 && riseEnd >= 0) m_stats.riseTime = riseEnd - riseStart;
        if (fallStart >= 0 && fallEnd >= 0) m_stats.fallTime = fallEnd - fallStart;

        // Refine period using rising edges if available
        if (risingEdges.size() >= 2) {
            QVector<double> risePeriods;
            for (int i = 1; i < risingEdges.size(); ++i) {
                risePeriods.append(risingEdges[i] - risingEdges[i - 1]);
            }
            double sumP = 0;
            for (double pVal : risePeriods) sumP += pVal;
            double avg = sumP / risePeriods.size();
            if (avg > 0) {
                m_stats.period = avg;
                m_stats.freq = 1.0 / avg;
                m_stats.hasPeriod = true;
            }
        }

        if (!highDurations.isEmpty()) {
            double sumHigh = 0;
            for (double d : highDurations) sumHigh += d;
            double avgHigh = sumHigh / highDurations.size();
            m_stats.pulseWidth = avgHigh;
            if (m_stats.hasPeriod && m_stats.period > 0) {
                m_stats.duty = std::min(100.0, std::max(0.0, (avgHigh / m_stats.period) * 100.0));
            }
        }
    }

    QVector<double> m_values;
    Stats m_stats;
    double m_sampleRate = 1000.0;
    double m_timeBaseMs = 50.0;
    double m_gain = 1.0;
    double m_vMin = 0.0;
    double m_vMax = 3.3;
};

namespace {
const char *kSettingsGroup = "MainWindow";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_settings("uartdebuger", "uartdebuger")
{
    ui->setupUi(this);
    m_scopeWidget = new OscilloscopeWidget(this);
    if (QLayout *lay = ui->scopePlotContainer->layout()) {
        lay->addWidget(m_scopeWidget);
        if (QWidget *placeholder = ui->scopePlaceholderLabel) {
            placeholder->deleteLater();
        }
    }
    initUi();
    applyStyleSheet();
    connectSignals();

    updatePortList(true);
    applySettings();
    setConnected(false);

    m_portRefreshTimer.setInterval(2500);
    connect(&m_portRefreshTimer, &QTimer::timeout, this, [this]() { updatePortList(); });
    m_portRefreshTimer.start();

    refreshScopeView();
}

MainWindow::~MainWindow()
{
    persistSettings();
    m_serial.close();
    delete ui;
}

void MainWindow::initUi()
{
    ui->baudRateComboBox->addItems({"9600", "19200", "38400", "57600", "115200", "230400", "460800"});
    ui->baudRateComboBox->setEditable(true);

    ui->dataBitsComboBox->addItem("5", QSerialPort::Data5);
    ui->dataBitsComboBox->addItem("6", QSerialPort::Data6);
    ui->dataBitsComboBox->addItem("7", QSerialPort::Data7);
    ui->dataBitsComboBox->addItem("8", QSerialPort::Data8);
    ui->dataBitsComboBox->setCurrentText("8");

    ui->parityComboBox->addItem(QStringLiteral("无 (N)"), QSerialPort::NoParity);
    ui->parityComboBox->addItem(QStringLiteral("偶校验 (E)"), QSerialPort::EvenParity);
    ui->parityComboBox->addItem(QStringLiteral("奇校验 (O)"), QSerialPort::OddParity);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
    ui->parityComboBox->addItem(QStringLiteral("标记 (M)"), QSerialPort::MarkParity);
    ui->parityComboBox->addItem(QStringLiteral("空格 (S)"), QSerialPort::SpaceParity);
#endif

    ui->stopBitsComboBox->addItem("1", QSerialPort::OneStop);
    ui->stopBitsComboBox->addItem("1.5", QSerialPort::OneAndHalfStop);
    ui->stopBitsComboBox->addItem("2", QSerialPort::TwoStop);

    ui->flowControlComboBox->addItem(QStringLiteral("无"), QSerialPort::NoFlowControl);
    ui->flowControlComboBox->addItem(QStringLiteral("硬件 RTS/CTS"), QSerialPort::HardwareControl);
    ui->flowControlComboBox->addItem(QStringLiteral("软件 XON/XOFF"), QSerialPort::SoftwareControl);

    ui->encodingComboBox->addItem(QStringLiteral("UTF-8"), "UTF-8");
    ui->encodingComboBox->addItem(QStringLiteral("GBK"), "GBK");
    ui->encodingComboBox->addItem(QStringLiteral("本地 8 位"), QByteArray());

    ui->newlineComboBox->addItem(QStringLiteral("无"), "");
    ui->newlineComboBox->addItem(QStringLiteral("LF (\\n)"), "\n");
    ui->newlineComboBox->addItem(QStringLiteral("CRLF (\\r\\n)"), "\r\n");

    ui->receiveTextEdit->setLineWrapMode(QTextEdit::NoWrap);
    ui->sendTextEdit->setLineWrapMode(QTextEdit::NoWrap);

    ui->statusbar->showMessage(QStringLiteral("已就绪"));

    m_autoSendTimer.setInterval(ui->sendIntervalSpinBox->value());
    connect(&m_autoSendTimer, &QTimer::timeout, this, &MainWindow::handleAutoSendTick);

    m_rxEffect = new QGraphicsOpacityEffect(this);
    m_rxEffect->setOpacity(1.0);
    ui->receiveTextEdit->setGraphicsEffect(m_rxEffect);
    m_rxHighlightAnim = new QPropertyAnimation(m_rxEffect, "opacity", this);
    m_rxHighlightAnim->setDuration(280);
    m_rxHighlightAnim->setEasingCurve(QEasingCurve::OutCubic);
}

void MainWindow::connectSignals()
{
    connect(ui->refreshPortsButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(ui->sendButton, &QPushButton::clicked, this, &MainWindow::sendData);
    connect(ui->clearSendButton, &QPushButton::clicked, this, &MainWindow::clearSend);
    connect(ui->clearReceiveButton, &QPushButton::clicked, this, &MainWindow::clearReceive);
    connect(ui->saveReceiveButton, &QPushButton::clicked, this, &MainWindow::saveReceive);
    connect(ui->loadFileButton, &QPushButton::clicked, this, &MainWindow::loadFileIntoSend);
    connect(ui->sendFileButton, &QPushButton::clicked, this, &MainWindow::sendBinaryFile);
    connect(ui->startAutoSendButton, &QPushButton::clicked, this, &MainWindow::startAutoSend);
    connect(ui->stopAutoSendButton, &QPushButton::clicked, this, &MainWindow::stopAutoSend);
    connect(ui->searchNextButton, &QPushButton::clicked, this, &MainWindow::findNext);

    connect(ui->addCommandButton, &QPushButton::clicked, this, &MainWindow::addCommand);
    connect(ui->editCommandButton, &QPushButton::clicked, this, &MainWindow::editCommand);
    connect(ui->removeCommandButton, &QPushButton::clicked, this, &MainWindow::removeCommand);
    connect(ui->importCommandsButton, &QPushButton::clicked, this, &MainWindow::importCommands);
    connect(ui->exportCommandsButton, &QPushButton::clicked, this, &MainWindow::exportCommands);
    connect(ui->loadCommandButton, &QPushButton::clicked, this, &MainWindow::loadCommandToSend);
    connect(ui->commandListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::commandDoubleClicked);

    connect(ui->receiveTabWidget, &QTabWidget::currentChanged, this, &MainWindow::handleScopeSettingChanged);
    connect(ui->scopeBitsSpinBox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &MainWindow::handleScopeSettingChanged);
    connect(ui->scopeVMinSpinBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MainWindow::handleScopeSettingChanged);
    connect(ui->scopeVMaxSpinBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MainWindow::handleScopeSettingChanged);
    connect(ui->scopeSampleRateSpinBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MainWindow::handleScopeSettingChanged);
    connect(ui->scopeTimeBaseSpinBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MainWindow::handleScopeSettingChanged);
    connect(ui->scopeGainSpinBox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &MainWindow::handleScopeSettingChanged);
    connect(ui->autoScopeButton, &QPushButton::clicked, this, &MainWindow::autoScope);
    connect(ui->clearScopeButton, &QPushButton::clicked, this, &MainWindow::clearScope);
    connect(ui->pauseTextCheckBox, &QCheckBox::toggled, this, &MainWindow::togglePauseText);
    connect(ui->pauseScopeCheckBox, &QCheckBox::toggled, this, &MainWindow::togglePauseScope);

    connect(&m_serial, &QSerialPort::readyRead, this, &MainWindow::handleReadyRead);
    connect(&m_serial, &QSerialPort::errorOccurred, this, &MainWindow::handleSerialError);
}

void MainWindow::applyStyleSheet()
{
    const QString base = QStringLiteral(
        "QMainWindow {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f7f8fa, stop:1 #eef0f3);"
        "  font-family: 'SF Pro Display', 'Helvetica Neue', 'Microsoft YaHei', sans-serif;"
        "}"
        "QGroupBox {"
        "  border: 1px solid #e5e5ea;"
        "  border-radius: 12px;"
        "  background: #ffffff;"
        "  margin-top: 10px;"
        "  padding: 12px;"
        "  font-weight: 600;"
        "  color: #111;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  padding: 0px 4px;"
        "}"
        "QLabel { color: #1c1c1e; }"
        "QTextEdit, QLineEdit, QComboBox, QSpinBox {"
        "  border: 1px solid #d1d1d6;"
        "  border-radius: 10px;"
        "  padding: 6px 8px;"
        "  background: #fbfbfd;"
        "}"
        "QTextEdit:focus, QLineEdit:focus, QComboBox:focus, QSpinBox:focus {"
        "  border: 1px solid %1;"
        "  box-shadow: 0 0 0 3px rgba(0,122,255,0.15);"
        "}"
        "QPushButton {"
        "  border: none;"
        "  border-radius: 12px;"
        "  padding: 8px 14px;"
        "  background: %1;"
        "  color: white;"
        "  font-weight: 600;"
        "}"
        "QPushButton:disabled {"
        "  background: #c7c7cc;"
        "  color: #f2f2f7;"
        "}"
        "QPushButton:hover {"
        "  background: #1b82ff;"
        "}"
        "QPushButton:pressed {"
        "  background: #0060df;"
        "}"
        "QCheckBox {"
        "  spacing: 8px;"
        "  font-weight: 500;"
        "}"
        "QListWidget {"
        "  border: 1px solid #d1d1d6;"
        "  border-radius: 10px;"
        "  background: #fbfbfd;"
        "}"
        "QStatusBar {"
        "  background: #f2f2f7;"
        "  color: #3a3a3c;"
        "}"
        "QScrollBar:vertical {"
        "  width: 10px;"
        "  background: transparent;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #d1d1d6;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    ).arg(m_accentColor);
    this->setStyleSheet(base);
}

void MainWindow::updatePortList(bool force)
{
    QStringList ports;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        ports << info.portName();
    }
    if (!force && ports == m_lastPorts) {
        return;
    }
    m_lastPorts = ports;

    const QString currentPort = ui->portComboBox->currentData().toString().isEmpty()
            ? ui->portComboBox->currentText()
            : ui->portComboBox->currentData().toString();
    ui->portComboBox->clear();

    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        QString text = info.portName();
        if (!info.description().isEmpty()) {
            text += " (" + info.description() + ")";
        }
        ui->portComboBox->addItem(text, info.portName());
    }

    int index = ui->portComboBox->findData(currentPort);
    if (index >= 0) {
        ui->portComboBox->setCurrentIndex(index);
    } else if (ui->portComboBox->count() > 0) {
        ui->portComboBox->setCurrentIndex(0);
    }
}

void MainWindow::applySettings()
{
    m_settings.beginGroup(kSettingsGroup);
    restoreGeometry(m_settings.value("geometry").toByteArray());
    ui->baudRateComboBox->setEditText(m_settings.value("baud", "115200").toString());
    ui->dataBitsComboBox->setCurrentText(m_settings.value("dataBits", "8").toString());
    ui->parityComboBox->setCurrentIndex(m_settings.value("parityIndex", 0).toInt());
    ui->stopBitsComboBox->setCurrentIndex(m_settings.value("stopIndex", 0).toInt());
    ui->flowControlComboBox->setCurrentIndex(m_settings.value("flowIndex", 0).toInt());
    ui->encodingComboBox->setCurrentIndex(m_settings.value("encodingIndex", 0).toInt());
    ui->newlineComboBox->setCurrentIndex(m_settings.value("newlineIndex", 0).toInt());
    ui->hexSendCheckBox->setChecked(m_settings.value("hexSend", false).toBool());
    ui->hexDisplayCheckBox->setChecked(m_settings.value("hexDisplay", false).toBool());
    ui->timestampCheckBox->setChecked(m_settings.value("timestamps", false).toBool());
    ui->bufferSizeSpinBox->setValue(m_settings.value("bufferSize", 0).toInt());
    ui->sendIntervalSpinBox->setValue(m_settings.value("autoInterval", 1000).toInt());
    ui->autoSendCountSpinBox->setValue(m_settings.value("autoCount", 0).toInt());
    ui->autoScrollCheckBox->setChecked(m_settings.value("autoScroll", true).toBool());
    const QString savedPort = m_settings.value("port").toString();
    int portIndex = ui->portComboBox->findData(savedPort);
    if (portIndex >= 0) {
        ui->portComboBox->setCurrentIndex(portIndex);
    }
    m_commands = commandsFromSettings();
    reloadCommandList();
    m_settings.endGroup();
}

void MainWindow::persistSettings()
{
    m_settings.beginGroup(kSettingsGroup);
    m_settings.setValue("geometry", saveGeometry());
    m_settings.setValue("baud", ui->baudRateComboBox->currentText());
    m_settings.setValue("dataBits", ui->dataBitsComboBox->currentText());
    m_settings.setValue("parityIndex", ui->parityComboBox->currentIndex());
    m_settings.setValue("stopIndex", ui->stopBitsComboBox->currentIndex());
    m_settings.setValue("flowIndex", ui->flowControlComboBox->currentIndex());
    m_settings.setValue("port", ui->portComboBox->currentData().toString());
    m_settings.setValue("encodingIndex", ui->encodingComboBox->currentIndex());
    m_settings.setValue("newlineIndex", ui->newlineComboBox->currentIndex());
    m_settings.setValue("hexSend", ui->hexSendCheckBox->isChecked());
    m_settings.setValue("hexDisplay", ui->hexDisplayCheckBox->isChecked());
    m_settings.setValue("timestamps", ui->timestampCheckBox->isChecked());
    m_settings.setValue("bufferSize", ui->bufferSizeSpinBox->value());
    m_settings.setValue("autoInterval", ui->sendIntervalSpinBox->value());
    m_settings.setValue("autoCount", ui->autoSendCountSpinBox->value());
    m_settings.setValue("autoScroll", ui->autoScrollCheckBox->isChecked());
    saveCommandsToSettings();
    m_settings.endGroup();
}

void MainWindow::setConnected(bool connected)
{
    ui->connectButton->setText(connected ? QStringLiteral("关闭") : QStringLiteral("打开"));
    ui->connectButton->setEnabled(true);
    ui->sendButton->setEnabled(connected);
    ui->sendFileButton->setEnabled(connected);
    ui->startAutoSendButton->setEnabled(connected);
    ui->stopAutoSendButton->setEnabled(false);
    ui->statusbar->showMessage(connected ? QStringLiteral("串口已打开") : QStringLiteral("未连接"));
    ui->connectionStateLabel->setText(connected ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    ui->connectionStateLabel->setStyleSheet(connected ? "color: rgb(0,180,0);" : "color: rgb(200,0,0);");
}

void MainWindow::resetStats()
{
    m_txBytes = 0;
    m_rxBytes = 0;
    ui->txBytesLabel->setText("0");
    ui->rxBytesLabel->setText("0");
}

void MainWindow::refreshPorts()
{
    updatePortList(true);
}

void MainWindow::toggleConnection()
{
    if (m_serial.isOpen()) {
        stopAutoSend();
        m_serial.close();
        setConnected(false);
        return;
    }

    const QString portName = ui->portComboBox->currentData().toString().isEmpty()
            ? ui->portComboBox->currentText()
            : ui->portComboBox->currentData().toString();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("串口"), QStringLiteral("未选择串口。"));
        return;
    }
    m_serial.setPortName(portName);
    m_serial.setReadBufferSize(ui->bufferSizeSpinBox->value());

    bool ok = false;
    int baud = ui->baudRateComboBox->currentText().toInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("波特率"), QStringLiteral("波特率无效。"));
        return;
    }
    if (!m_serial.setBaudRate(baud)) {
        QMessageBox::warning(this, QStringLiteral("波特率"), QStringLiteral("设置波特率失败。"));
        return;
    }
    if (!m_serial.setDataBits(static_cast<QSerialPort::DataBits>(ui->dataBitsComboBox->currentData().toInt()))) {
        QMessageBox::warning(this, QStringLiteral("数据位"), QStringLiteral("设置数据位失败。"));
        return;
    }
    if (!m_serial.setParity(static_cast<QSerialPort::Parity>(ui->parityComboBox->currentData().toInt()))) {
        QMessageBox::warning(this, QStringLiteral("校验位"), QStringLiteral("设置校验位失败。"));
        return;
    }
    if (!m_serial.setStopBits(static_cast<QSerialPort::StopBits>(ui->stopBitsComboBox->currentData().toInt()))) {
        QMessageBox::warning(this, QStringLiteral("停止位"), QStringLiteral("设置停止位失败。"));
        return;
    }
    if (!m_serial.setFlowControl(static_cast<QSerialPort::FlowControl>(ui->flowControlComboBox->currentData().toInt()))) {
        QMessageBox::warning(this, QStringLiteral("流控"), QStringLiteral("设置流控失败。"));
        return;
    }

    if (!m_serial.open(QIODevice::ReadWrite)) {
        QMessageBox::critical(this, QStringLiteral("串口"), QStringLiteral("打开串口失败：\n") + m_serial.errorString());
        return;
    }

    resetStats();
    setConnected(true);
    setLastError("-");
}

void MainWindow::handleReadyRead()
{
    const QByteArray data = m_serial.readAll();
    if (data.isEmpty()) {
        return;
    }
    m_rxBytes += data.size();
    ui->rxBytesLabel->setText(QString::number(m_rxBytes));

    if (isScopeMode()) {
        if (m_pauseScope) {
            return;
        }
        processScopeData(data);
        return;
    }

    if (m_pauseText) {
        return;
    }

    QString line;
    if (ui->timestampCheckBox->isChecked()) {
        line += "[" + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") + "] ";
    }
    if (ui->hexDisplayCheckBox->isChecked()) {
        QString hex;
        for (unsigned char c : data) {
            hex += QString("%1 ").arg(c, 2, 16, QLatin1Char('0')).toUpper();
        }
        line += hex.trimmed();
    } else {
        line += formatAscii(data);
    }
    appendReceiveText(line);
    if (m_rxHighlightAnim && m_rxEffect) {
        m_rxHighlightAnim->stop();
        m_rxEffect->setOpacity(0.6);
        m_rxHighlightAnim->setStartValue(0.6);
        m_rxHighlightAnim->setEndValue(1.0);
        m_rxHighlightAnim->start();
    }
}

void MainWindow::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    setLastError(m_serial.errorString());
    if (error == QSerialPort::ResourceError || error == QSerialPort::PermissionError || error == QSerialPort::DeviceNotFoundError) {
        stopAutoSend();
        QMessageBox::critical(this, QStringLiteral("串口错误"), m_serial.errorString());
        m_serial.close();
        setConnected(false);
    }
}

QByteArray MainWindow::encodeText(const QString &text) const
{
    const QByteArray codecName = ui->encodingComboBox->currentData().toByteArray();
    if (codecName.isEmpty()) {
        return text.toLocal8Bit();
    }
    QTextCodec *codec = QTextCodec::codecForName(codecName);
    if (!codec) {
        return text.toUtf8();
    }
    return codec->fromUnicode(text);
}

QString MainWindow::decodeBytes(const QByteArray &bytes) const
{
    const QByteArray codecName = ui->encodingComboBox->currentData().toByteArray();
    if (codecName.isEmpty()) {
        return QString::fromLocal8Bit(bytes);
    }
    QTextCodec *codec = QTextCodec::codecForName(codecName);
    if (!codec) {
        return QString::fromUtf8(bytes);
    }
    return codec->toUnicode(bytes);
}

QByteArray MainWindow::buildPayload(bool *ok, QString *error) const
{
    *ok = false;
    QByteArray payload;
    const QString newline = ui->newlineComboBox->currentData().toString();
    if (ui->hexSendCheckBox->isChecked()) {
        QString hex = ui->sendTextEdit->toPlainText();
        hex.remove(' ');
        hex.remove('\n');
        hex.remove('\r');
        if (hex.length() % 2 != 0) {
            *error = QStringLiteral("HEX 字符串长度必须为偶数。");
            return QByteArray();
        }
        for (int i = 0; i < hex.length(); i += 2) {
            bool byteOk = false;
            int value = hex.mid(i, 2).toInt(&byteOk, 16);
            if (!byteOk) {
                *error = QStringLiteral("HEX 内容无效。");
                return QByteArray();
            }
            payload.append(static_cast<char>(value));
        }
        payload.append(newline.toUtf8());
    } else {
        QString text = ui->sendTextEdit->toPlainText();
        text.append(newline);
        payload = encodeText(text);
    }
    *ok = true;
    return payload;
}

void MainWindow::sendData()
{
    transmitPayload(true);
}

bool MainWindow::transmitPayload(bool showDialogs)
{
    if (!m_serial.isOpen()) {
        if (showDialogs) {
            QMessageBox::warning(this, QStringLiteral("发送"), QStringLiteral("串口未打开。"));
        }
        return false;
    }
    bool ok = false;
    QString error;
    QByteArray payload = buildPayload(&ok, &error);
    if (!ok) {
        if (showDialogs) {
            QMessageBox::warning(this, QStringLiteral("发送"), error);
        }
        return false;
    }
    if (payload.isEmpty()) {
        if (showDialogs) {
            QMessageBox::information(this, QStringLiteral("发送"), QStringLiteral("没有要发送的内容。"));
        }
        return false;
    }
    const qint64 written = m_serial.write(payload);
    if (written == -1) {
        if (showDialogs) {
            QMessageBox::critical(this, QStringLiteral("发送"), QStringLiteral("写入失败：") + m_serial.errorString());
        }
        return false;
    }
    m_txBytes += written;
    ui->txBytesLabel->setText(QString::number(m_txBytes));
    if (showDialogs) {
        ui->statusbar->showMessage(QStringLiteral("发送 %1 字节").arg(written), 1500);
    }
    return true;
}

void MainWindow::clearSend()
{
    ui->sendTextEdit->clear();
}

void MainWindow::clearReceive()
{
    ui->receiveTextEdit->clear();
}

void MainWindow::saveReceive()
{
    const QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("保存日志"), QString(), QStringLiteral("文本 (*.txt);;Hex (*.hex);;所有文件 (*)"));
    if (fileName.isEmpty()) {
        return;
    }
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("保存"), QStringLiteral("打开文件失败。"));
        return;
    }
    QTextStream stream(&file);
    stream << ui->receiveTextEdit->toPlainText();
    ui->statusbar->showMessage(QStringLiteral("已保存到：") + fileName, 2000);
}

void MainWindow::loadFileIntoSend()
{
    const QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("加载文件"), QString(), QStringLiteral("所有文件 (*)"));
    if (fileName.isEmpty()) {
        return;
    }
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("加载"), QStringLiteral("打开文件失败。"));
        return;
    }
    const QByteArray content = file.readAll();
    ui->sendTextEdit->setPlainText(decodeBytes(content));
}

void MainWindow::sendBinaryFile()
{
    if (!m_serial.isOpen()) {
        QMessageBox::warning(this, QStringLiteral("二进制发送"), QStringLiteral("串口未打开。"));
        return;
    }
    const QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("发送二进制文件"), QString(), QStringLiteral("所有文件 (*)"));
    if (fileName.isEmpty()) {
        return;
    }
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("二进制发送"), QStringLiteral("打开文件失败。"));
        return;
    }
    const QByteArray content = file.readAll();
    const qint64 written = m_serial.write(content);
    if (written == -1) {
        QMessageBox::critical(this, QStringLiteral("二进制发送"), QStringLiteral("写入失败：") + m_serial.errorString());
        return;
    }
    m_txBytes += written;
    ui->txBytesLabel->setText(QString::number(m_txBytes));
    ui->statusbar->showMessage(QStringLiteral("发送二进制 %1 字节").arg(written), 2000);
}

void MainWindow::startAutoSend()
{
    if (!m_serial.isOpen()) {
        QMessageBox::warning(this, QStringLiteral("自动发送"), QStringLiteral("串口未打开。"));
        return;
    }
    stopAutoSend();
    m_autoSendRemaining = ui->autoSendCountSpinBox->value();
    m_autoSendTimer.start(ui->sendIntervalSpinBox->value());
    ui->startAutoSendButton->setEnabled(false);
    ui->stopAutoSendButton->setEnabled(true);
    handleAutoSendTick();
}

void MainWindow::stopAutoSend()
{
    m_autoSendTimer.stop();
    ui->startAutoSendButton->setEnabled(m_serial.isOpen());
    ui->stopAutoSendButton->setEnabled(false);
}

void MainWindow::handleAutoSendTick()
{
    if (!m_serial.isOpen()) {
        stopAutoSend();
        return;
    }
    if (m_autoSendRemaining == 0) {
        if (!transmitPayload(false)) {
            ui->statusbar->showMessage(QStringLiteral("自动发送因发送错误已停止"), 3000);
            stopAutoSend();
        }
        return;
    }
    if (m_autoSendRemaining < 0) {
        stopAutoSend();
        return;
    }
    if (!transmitPayload(false)) {
        ui->statusbar->showMessage(QStringLiteral("自动发送因发送错误已停止"), 3000);
        stopAutoSend();
        return;
    }
    m_autoSendRemaining--;
    if (m_autoSendRemaining == 0) {
        stopAutoSend();
    }
}

void MainWindow::findNext()
{
    const QString term = ui->searchLineEdit->text();
    if (term.isEmpty()) {
        return;
    }
    if (!ui->receiveTextEdit->find(term)) {
        // loop back
        QTextCursor cursor = ui->receiveTextEdit->textCursor();
        cursor.movePosition(QTextCursor::Start);
        ui->receiveTextEdit->setTextCursor(cursor);
        ui->receiveTextEdit->find(term);
    }
}

void MainWindow::appendReceiveText(const QString &text)
{
    ui->receiveTextEdit->append(text);
    if (ui->autoScrollCheckBox->isChecked()) {
        ui->receiveTextEdit->moveCursor(QTextCursor::End);
    }
}

QString MainWindow::formatAscii(const QByteArray &bytes) const
{
    QString output;
    const QString decoded = decodeBytes(bytes);
    for (QChar ch : decoded) {
        if (ch == '\n' || ch == '\r' || ch == '\t') {
            output.append(ch);
        } else if (ch.isPrint() && !ch.isNull()) {
            output.append(ch);
        } else {
            output.append(QString("[0x%1]").arg(QString::number(ch.unicode(), 16).toUpper().rightJustified(2, '0')));
        }
    }
    return output;
}

bool MainWindow::isScopeMode() const
{
    return ui->receiveTabWidget->currentIndex() == 1;
}

void MainWindow::processScopeData(const QByteArray &data)//示波器接收
{
    // 按当前配置将串口收到的数字流转换为电压并缓存
    const double vMin = ui->scopeVMinSpinBox->value();
    const double vMax = ui->scopeVMaxSpinBox->value();
    const int bits = ui->scopeBitsSpinBox->value();
    const double maxCode = std::max(1.0, std::pow(2.0, bits) - 1.0);
    const double gain = ui->scopeGainSpinBox->value();

    for (char c : data) {
        // 用空格/逗号/换行等作为分隔符
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ',' || c == ';') {
            if (!m_scopePending.isEmpty()) {
                bool ok = false;
                int raw = m_scopePending.toInt(&ok, 10);
                if (ok) {
                    // 数字映射为电压：0->vMin，满量程->vMax，再乘放大倍数
                    double clamped = std::max(0.0, std::min(maxCode, static_cast<double>(raw)));
                    double volt = vMin + (clamped / maxCode) * (vMax - vMin);
                    volt *= gain;
                    m_scopeValues.append(volt);
                    if (m_scopeValues.size() > m_scopeMaxSamples) {
                        m_scopeValues.remove(0, m_scopeValues.size() - m_scopeMaxSamples);
                    }
                }
                m_scopePending.clear();
            }
        } else {
            // 累积数字字符
            m_scopePending.append(QChar(c));
        }
    }
    // 推动波形刷新与测量
    refreshScopeView();
}

void MainWindow::refreshScopeView()
{
    if (!m_scopeWidget) return;
    m_scopeWidget->configure(ui->scopeSampleRateSpinBox->value(),
                             ui->scopeTimeBaseSpinBox->value(),
                             ui->scopeGainSpinBox->value(),
                             ui->scopeVMinSpinBox->value(),
                             ui->scopeVMaxSpinBox->value());
    m_scopeWidget->setValues(m_scopeValues);
    updateScopeLabels();
}

void MainWindow::updateScopeLabels()
{
    if (!m_scopeWidget) return;
    const auto &s = m_scopeWidget->stats();
    auto fmt = [](double v, const QString &unit, int prec = 3) -> QString {
        return QString::number(v, 'f', prec) + unit;
    };
    if (s.samples == 0) {
        ui->scopePeakLabel->setText("-");
        ui->scopePkPkLabel->setText("-");
        ui->scopeRmsLabel->setText("-");
        ui->scopeDcLabel->setText("-");
        ui->scopePeriodLabel->setText("-");
        ui->scopeFreqLabel->setText("-");
        ui->scopeRiseLabel->setText("-");
        ui->scopeFallLabel->setText("-");
        ui->scopePulseLabel->setText("-");
        ui->scopeDutyLabel->setText("-");
        return;
    }
    ui->scopePeakLabel->setText(fmt(s.max, " V"));
    ui->scopePkPkLabel->setText(fmt(s.peakToPeak, " V"));
    ui->scopeRmsLabel->setText(fmt(s.rms, " V"));
    ui->scopeDcLabel->setText(fmt(s.mean, " V"));
    ui->scopePeriodLabel->setText(s.hasPeriod ? fmt(s.period * 1000.0, " ms", 3) : "-");
    ui->scopeFreqLabel->setText(s.hasPeriod && s.freq > 0 ? fmt(s.freq, " Hz", 3) : "-");
    ui->scopeRiseLabel->setText(s.riseTime > 0 ? fmt(s.riseTime * 1000.0, " ms", 3) : "-");
    ui->scopeFallLabel->setText(s.fallTime > 0 ? fmt(s.fallTime * 1000.0, " ms", 3) : "-");
    ui->scopePulseLabel->setText(s.pulseWidth > 0 ? fmt(s.pulseWidth * 1000.0, " ms", 3) : "-");
    ui->scopeDutyLabel->setText(s.duty > 0 ? fmt(s.duty, " %", 1) : "-");
}

void MainWindow::addCommand()
{
    QString name = QInputDialog::getText(this, QStringLiteral("添加命令"), QStringLiteral("名称："));
    if (name.isEmpty()) {
        return;
    }
    const QString data = ui->sendTextEdit->toPlainText();
    if (data.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("命令"), QStringLiteral("发送区为空。"));
        return;
    }
    CommandEntry entry;
    entry.name = name;
    entry.data = data;
    entry.hexMode = ui->hexSendCheckBox->isChecked();
    m_commands.append(entry);
    reloadCommandList();
}

void MainWindow::editCommand()
{
    const int row = ui->commandListWidget->currentRow();
    if (row < 0 || row >= m_commands.size()) {
        return;
    }
    CommandEntry entry = m_commands.at(row);
    QString newName = QInputDialog::getText(this, QStringLiteral("编辑命令"), QStringLiteral("名称："), QLineEdit::Normal, entry.name);
    if (newName.isEmpty()) {
        return;
    }
    QString newData = QInputDialog::getMultiLineText(this, QStringLiteral("编辑命令"), QStringLiteral("数据："), entry.data);
    if (newData.isEmpty()) {
        return;
    }
    const QMessageBox::StandardButton hexChoice = QMessageBox::question(this, QStringLiteral("HEX 模式"), QStringLiteral("以 HEX 发送？"), QMessageBox::Yes | QMessageBox::No, entry.hexMode ? QMessageBox::Yes : QMessageBox::No);
    entry.name = newName;
    entry.data = newData;
    entry.hexMode = (hexChoice == QMessageBox::Yes);
    m_commands[row] = entry;
    reloadCommandList();
}

void MainWindow::removeCommand()
{
    const int row = ui->commandListWidget->currentRow();
    if (row < 0 || row >= m_commands.size()) {
        return;
    }
    m_commands.removeAt(row);
    reloadCommandList();
}

void MainWindow::importCommands()
{
    const QString fileName = QFileDialog::getOpenFileName(this, QStringLiteral("导入命令"), QString(), QStringLiteral("JSON (*.json);;所有文件 (*)"));
    if (fileName.isEmpty()) {
        return;
    }
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("导入"), QStringLiteral("打开文件失败。"));
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        QMessageBox::warning(this, QStringLiteral("导入"), QStringLiteral("JSON 格式无效。"));
        return;
    }
    QList<CommandEntry> imported;
    for (const QJsonValue &val : doc.array()) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        CommandEntry entry;
        entry.name = obj.value("name").toString();
        entry.data = obj.value("data").toString();
        entry.hexMode = obj.value("hex").toBool(false);
        if (!entry.name.isEmpty()) {
            imported.append(entry);
        }
    }
    if (imported.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导入"), QStringLiteral("未找到命令。"));
        return;
    }
    m_commands = imported;
    reloadCommandList();
}

void MainWindow::exportCommands()
{
    const QString fileName = QFileDialog::getSaveFileName(this, QStringLiteral("导出命令"), QString(), QStringLiteral("JSON (*.json);;所有文件 (*)"));
    if (fileName.isEmpty()) {
        return;
    }
    QJsonArray arr;
    for (const CommandEntry &entry : m_commands) {
        QJsonObject obj;
        obj["name"] = entry.name;
        obj["data"] = entry.data;
        obj["hex"] = entry.hexMode;
        arr.append(obj);
    }
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("导出"), QStringLiteral("打开文件失败。"));
        return;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    ui->statusbar->showMessage(QStringLiteral("命令已导出"), 2000);
}

void MainWindow::loadCommandToSend()
{
    const int row = ui->commandListWidget->currentRow();
    if (row < 0 || row >= m_commands.size()) {
        return;
    }
    handleCommandSend(m_commands.at(row), false);
}

void MainWindow::commandDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;
    const int row = ui->commandListWidget->row(item);
    if (row < 0 || row >= m_commands.size()) {
        return;
    }
    handleCommandSend(m_commands.at(row), true);
}

void MainWindow::clearScope()
{
    m_scopeValues.clear();
    m_scopePending.clear();
    refreshScopeView();
}

void MainWindow::handleScopeSettingChanged()
{
    if (isScopeMode()) {
        refreshScopeView();
    }
}

void MainWindow::autoScope()
{
    if (m_scopeValues.isEmpty() || !m_scopeWidget) {
        ui->statusbar->showMessage(QStringLiteral("没有波形数据，无法自动调整"), 2000);
        return;
    }
    refreshScopeView();
    const auto &s = m_scopeWidget->stats();
    const double vSpanRaw = s.peakToPeak;
    double gain = ui->scopeGainSpinBox->value();
    const double gainMin = ui->scopeGainSpinBox->minimum();
    const double gainMax = ui->scopeGainSpinBox->maximum();
    const double desiredSpan = (ui->scopeVMaxSpinBox->value() - ui->scopeVMinSpinBox->value()) * 0.7;
    if (vSpanRaw > 0.0 && desiredSpan > 0.0) {
        double targetGain = gain * (desiredSpan / vSpanRaw);
        targetGain = std::min(std::max(targetGain, gainMin), gainMax);
        ui->scopeGainSpinBox->setValue(targetGain);
        gain = targetGain;
    }

    if (s.hasPeriod && s.period > 0) {
        double windowSec = s.period * 3.0; // show about 3 periods
        double timeBaseMs = (windowSec * 1000.0) / 10.0; // per division
        double clamped = std::min(std::max(timeBaseMs, ui->scopeTimeBaseSpinBox->minimum()), ui->scopeTimeBaseSpinBox->maximum());
        ui->scopeTimeBaseSpinBox->setValue(clamped);
    } else if (ui->scopeSampleRateSpinBox->value() > 0 && s.samples > 0) {
        double windowSec = static_cast<double>(s.samples) / ui->scopeSampleRateSpinBox->value();
        double timeBaseMs = (windowSec * 1000.0) / 10.0;
        double clamped = std::min(std::max(timeBaseMs, ui->scopeTimeBaseSpinBox->minimum()), ui->scopeTimeBaseSpinBox->maximum());
        ui->scopeTimeBaseSpinBox->setValue(clamped);
    }

    refreshScopeView();
    ui->statusbar->showMessage(QStringLiteral("AUTO 已调整波形显示"), 1500);
}

void MainWindow::togglePauseText(bool checked)
{
    m_pauseText = checked;
    if (checked) {
        ui->statusbar->showMessage(QStringLiteral("文本接收已暂停"), 1500);
    } else {
        ui->statusbar->showMessage(QStringLiteral("文本接收已恢复"), 1500);
    }
}

void MainWindow::togglePauseScope(bool checked)
{
    m_pauseScope = checked;
    if (checked) {
        ui->statusbar->showMessage(QStringLiteral("波形接收已暂停"), 1500);
    } else {
        ui->statusbar->showMessage(QStringLiteral("波形接收已恢复"), 1500);
    }
}

void MainWindow::handleCommandSend(const CommandEntry &entry, bool sendNow)
{
    ui->sendTextEdit->setPlainText(entry.data);
    ui->hexSendCheckBox->setChecked(entry.hexMode);
    if (sendNow) {
        sendData();
    }
}

void MainWindow::reloadCommandList()
{
    ui->commandListWidget->clear();
    for (const CommandEntry &entry : m_commands) {
        QListWidgetItem *item = new QListWidgetItem(entry.name, ui->commandListWidget);
        item->setToolTip(entry.data.left(200));
        item->setData(Qt::UserRole, entry.hexMode);
    }
}

QList<MainWindow::CommandEntry> MainWindow::commandsFromSettings() const
{
    QList<CommandEntry> list;
    const QByteArray json = m_settings.value("commands").toByteArray();
    if (json.isEmpty()) {
        return list;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) {
        return list;
    }
    for (const QJsonValue &val : doc.array()) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        CommandEntry entry;
        entry.name = obj.value("name").toString();
        entry.data = obj.value("data").toString();
        entry.hexMode = obj.value("hex").toBool(false);
        if (!entry.name.isEmpty()) {
            list.append(entry);
        }
    }
    return list;
}

void MainWindow::saveCommandsToSettings()
{
    QJsonArray arr;
    for (const CommandEntry &entry : m_commands) {
        QJsonObject obj;
        obj["name"] = entry.name;
        obj["data"] = entry.data;
        obj["hex"] = entry.hexMode;
        arr.append(obj);
    }
    m_settings.setValue("commands", QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void MainWindow::setLastError(const QString &errorText)
{
    ui->lastErrorLabel->setText(errorText.isEmpty() ? "-" : errorText);
}
