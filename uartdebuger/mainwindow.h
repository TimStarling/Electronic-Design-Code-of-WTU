#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QSettings>
#include <QListWidgetItem>
#include <QStringList>
#include <QList>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class OscilloscopeWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    struct CommandEntry {
        QString name;
        QString data;
        bool hexMode = false;
    };

    void initUi();
    void connectSignals();
    void applyStyleSheet();
    void updatePortList(bool force = false);
    void applySettings();
    void persistSettings();
    void setConnected(bool connected);
    void resetStats();
    bool transmitPayload(bool showDialogs);
    QByteArray buildPayload(bool *ok, QString *error) const;
    QByteArray encodeText(const QString &text) const;
    QString decodeBytes(const QByteArray &bytes) const;
    QString formatAscii(const QByteArray &bytes) const;
    void appendReceiveText(const QString &text);
    void handleCommandSend(const CommandEntry &entry, bool sendNow);
    void reloadCommandList();
    QList<CommandEntry> commandsFromSettings() const;
    void saveCommandsToSettings();
    void setLastError(const QString &errorText);
    void updateScopeLabels();
    void processScopeData(const QByteArray &data);
    void refreshScopeView();
    bool isScopeMode() const;

private slots:
    void refreshPorts();
    void toggleConnection();
    void handleReadyRead();
    void handleSerialError(QSerialPort::SerialPortError error);
    void sendData();
    void clearSend();
    void clearReceive();
    void saveReceive();
    void loadFileIntoSend();
    void sendBinaryFile();
    void startAutoSend();
    void stopAutoSend();
    void handleAutoSendTick();
    void findNext();
    void addCommand();
    void editCommand();
    void removeCommand();
    void importCommands();
    void exportCommands();
    void loadCommandToSend();
    void commandDoubleClicked(QListWidgetItem *item);
    void clearScope();
    void handleScopeSettingChanged();
    void autoScope();
    void togglePauseText(bool checked);
    void togglePauseScope(bool checked);

private:
    Ui::MainWindow *ui;
    OscilloscopeWidget *m_scopeWidget = nullptr;
    QSerialPort m_serial;
    QTimer m_autoSendTimer;
    QTimer m_portRefreshTimer;
    QSettings m_settings;
    QGraphicsOpacityEffect *m_rxEffect = nullptr;
    QPropertyAnimation *m_rxHighlightAnim = nullptr;
    QString m_accentColor = "#007aff";
    bool m_pauseText = false;
    bool m_pauseScope = false;
    qint64 m_txBytes = 0;
    qint64 m_rxBytes = 0;
    int m_autoSendRemaining = 0;
    QStringList m_lastPorts;
    QList<CommandEntry> m_commands;
    QVector<double> m_scopeValues;
    QString m_scopePending;
    int m_scopeMaxSamples = 6000;
};
#endif // MAINWINDOW_H
