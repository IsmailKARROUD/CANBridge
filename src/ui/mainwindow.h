/**
 * @file mainwindow.h
 * @brief Definition of the MainWindow class — the primary application UI.
 *
 * MainWindow provides a tabbed interface with four sections:
 *  1. Connection — Start/stop the TCP server, connect/disconnect a TCP client
 *  2. Log        — Real-time event log with category filtering
 *  3. Simulator  — Compose and transmit CAN frames (one-shot or periodic)
 *  4. Analyzer   — Table view of captured frames with CSV import/export
 *
 * It owns the TcpServer, TcpClient, and MessageModel instances and wires
 * their signals to the UI for status updates, logging, and data display.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTableView>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QMap>
#include <QScrollArea>
#include <QMessageBox>

#include "tcpserver.h"
#include "tcpclient.h"
#include "messagemodel.h"
#include "framefilterproxymodel.h"
#include "framewidget.h"

/**
 * @class MainWindow
 * @brief Main application window with tabbed UI for CAN bridge operations.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // --- Connection tab: Server controls ---
    void onStartServer();               ///< Start the TCP server on the configured port
    void onStopServer();                ///< Stop the TCP server and disconnect all clients

    // --- Connection tab: Client controls ---
    void onConnect();                   ///< Connect to a remote server
    void onDisconnect();                ///< Disconnect from the remote server

    // --- Simulator tab ---
    void addFrameWidget(uint32_t defaultId = 0x100);
    bool isCanIdDuplicate(uint32_t canId, FrameWidget* excludeWidget = nullptr);
    void onFrameSendOnce(const CANFrame& frame);
    void onSendFramePeriodic(const CANFrame& frame, int intervalMs);
    void onStopFramePeriodic(const int& frame);
    void onFrameRemove(uint32_t canId);

    // --- Analyzer tab ---
    void onClearMessages();             ///< Clear all captured frames from the model
    void onSaveFrames();                ///< Export captured frames to a CSV file
    void onLoadFrames();                ///< Import CAN frames from a CSV file

    // --- Log tab ---
    void onLogFilterChanged(int index); ///< Update log display when filter selection changes

    // --- Status updates from backend ---
    void onServerClientConnected(const QString& address);   ///< Log when a client connects to server
    void onClientConnected();           ///< Update UI when client connects to remote server
    void onClientDisconnected();        ///< Update UI when client disconnects from remote server

private:
    // --- Tab setup methods ---
    void setupConnectionTab();          ///< Build the Connection tab layout and widgets
    void setupLogTab();                 ///< Build the Log tab layout and widgets
    void setupSimulatorTab();           ///< Build the Simulator tab layout and widgets
    void setupAnalyzerTab();            ///< Build the Analyzer tab layout and widgets

    // --- Log management ---
    /**
     * @brief Add a timestamped event to the log history.
     * @param message  The log message text.
     * @param category Category for filtering: "Server", "Client", or "Frame".
     */
    void addLogEvent(const QString& message, const QString& category);

    /// Rebuild the log display text based on the current filter selection.
    void updateLogDisplay();

    // --- Menu bar and theme ---
    void setupMenuBar();                ///< Create the application menu bar (CANBridge, View)
    void applyTheme(bool isDark);       ///< Apply dark or light theme stylesheet

    // ========================================================================
    // UI Widgets
    // ========================================================================

    QTabWidget* tabWidget;              ///< Central tab container

    // -- Connection tab: Server --
    QSpinBox* serverPortSpin;           ///< Port number selector for the server
    QPushButton* startServerBtn;        ///< Button to start the server
    QPushButton* stopServerBtn;         ///< Button to stop the server
    QLabel* serverStatusIndicator;      ///< Status label (Running/Stopped/Error)

    // -- Connection tab: Client --
    QLineEdit* hostEdit;                ///< Server hostname/IP input field
    QSpinBox* clientPortSpin;           ///< Port number selector for client connection
    QPushButton* connectBtn;            ///< Button to connect to server
    QPushButton* disconnectBtn;         ///< Button to disconnect from server
    QLabel* clientStatusIndicator;      ///< Status label (Connected/Disconnected/Error)

    // -- Log tab --
    QComboBox* logFilterCombo;          ///< Dropdown for log category filtering
    QTextEdit* logDisplay;              ///< Read-only text area for event log

    // -- Simulator tab --
    QVBoxLayout* framesLayout;              ///< Container for FrameWidget instances
    QMap<uint32_t, FrameWidget*> frameWidgets;  ///< Active frame widgets keyed by CAN ID
    QPushButton* addFrameBtn;               ///< Button to add a new frame widget
    QPushButton* sendAllBtn;               ///< Button to send all frames once
    QPushButton* stopAllBtn;               ///< Button to stop all periodic transmissions
    QLabel* lastFrameStatusLabel;          ///< Status label showing the last sent frame result
    // Note: addFrameWidget() and isCanIdDuplicate() are declared in private slots: above

    // -- Analyzer tab --
    QTableView* messageTable;           ///< Table view bound to proxy model
    QComboBox* dirFilterCombo;          ///< Direction filter: All / TX / RX
    QLineEdit* idFilterEdit;            ///< CAN ID filter (hex substring match)
    QLineEdit* dataFilterEdit;          ///< Data filter (hex substring match)
    QPushButton* clearBtn;              ///< Button to clear all captured frames
    QPushButton* saveFramesBtn;         ///< Button to export frames to CSV
    QPushButton* loadFramesBtn;         ///< Button to import frames from CSV
    QLabel* messageCountLabel;          ///< Label showing current frame count

    // ========================================================================
    // Backend Components
    // ========================================================================

    TcpServer* server;                  ///< TCP server for broadcasting CAN frames
    TcpClient* client;                  ///< TCP client for connecting to a remote server
    MessageModel* messageModel;                 ///< Source data model holding all captured CAN frames
    FrameFilterProxyModel* filterProxyModel;    ///< Proxy between MessageModel and QTableView; filters rows by Dir/ID/Data

    // ========================================================================
    // Log Management
    // ========================================================================

    /**
     * @brief Represents a single log entry with timestamp, category, and message.
     */
    struct LogEntry {
        QString timestamp;              ///< Time the event occurred (hh:mm:ss)
        QString category;              ///< Event category: "Server", "Client", or "Frame"
        QString message;               ///< Human-readable event description
    };
    QList<LogEntry> logHistory;         ///< Chronological list of log entries (max 50)
    QString currentLogFilter;           ///< Active filter: "All", "Server", "Client", or "Frame"

    // ========================================================================
    // Theme
    // ========================================================================

    QAction* darkModeAction;            ///< Menu action for dark theme selection
    bool isDarkMode;                    ///< Current theme state

    // ========================================================================
    // Other function
    // ========================================================================
private:
    QString formatHexWithSpaces(const QString& input);
};

#endif // MAINWINDOW_H
