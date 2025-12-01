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
        // 常用命令条目：名称、原始文本、是否按 HEX 发送
        QString name;
        QString data;
        bool hexMode = false;
    };

    // 初始化 UI 控件配置
    void initUi();
    // 绑定信号槽
    void connectSignals();
    // 应用全局样式
    void applyStyleSheet();
    // 刷新串口列表，force 为 true 时强制刷新
    void updatePortList(bool force = false);
    // 从 QSettings 读取设置到界面
    void applySettings();
    // 将当前配置写入 QSettings
    void persistSettings();
    // 根据连接状态调整按钮/状态显示
    void setConnected(bool connected);
    // 清空收发统计
    void resetStats();
    // 发送当前构造的 payload；showDialogs 控制是否提示
    bool transmitPayload(bool showDialogs);
    // 组装发送数据（文本或 HEX），返回 QByteArray
    QByteArray buildPayload(bool *ok, QString *error) const;
    // 字符串按选择的编码转字节
    QByteArray encodeText(const QString &text) const;
    // 字节按选择的编码转字符串
    QString decodeBytes(const QByteArray &bytes) const;
    // 将不可打印字符转为 [0xXX] 形式
    QString formatAscii(const QByteArray &bytes) const;
    // 在接收文本区追加并处理自动滚动
    void appendReceiveText(const QString &text);
    // 处理命令库条目装载/发送
    void handleCommandSend(const CommandEntry &entry, bool sendNow);
    // 刷新命令列表显示
    void reloadCommandList();
    // 从设置读取命令库
    QList<CommandEntry> commandsFromSettings() const;
    // 将命令库写入设置
    void saveCommandsToSettings();
    // 更新状态栏/标签的错误提示
    void setLastError(const QString &errorText);
    // 更新示波器测量标签
    void updateScopeLabels();
    // 解析串口数据到示波器缓冲
    void processScopeData(const QByteArray &data);
    // 更新示波器配置与绘制
    void refreshScopeView();
    // 当前是否处于示波器页
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
    void showHelpGuide();

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
