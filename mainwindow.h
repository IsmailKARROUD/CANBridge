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
#include "tcpserver.h"
#include "tcpclient.h"
#include "messagemodel.h"

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
    void onSendFrame();                 ///< Parse UI inputs and send a single CAN frame
    void onSendPeriodic();              ///< Start periodic transmission of a CAN frame
    void onStopPeriodic();              ///< Stop all periodic transmissions

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
    QLineEdit* canIdEdit;               ///< CAN ID input (hex format, e.g., "0x123")
    QSpinBox* dlcSpin;                  ///< Data Length Code selector (0-8)
    QLineEdit* dataEdit;                ///< Data bytes input (space-separated hex)
    QPushButton* sendOnceBtn;           ///< Button to send a single frame
    QSpinBox* intervalSpin;             ///< Periodic interval selector in milliseconds
    QPushButton* sendPeriodicBtn;       ///< Button to start periodic transmission
    QPushButton* stopPeriodicBtn;       ///< Button to stop periodic transmission
    QLabel* lastFrameStatusLabel;       ///< Status of the last frame send operation

    // -- Analyzer tab --
    QTableView* messageTable;           ///< Table view bound to MessageModel
    QPushButton* clearBtn;              ///< Button to clear all captured frames
    QPushButton* saveFramesBtn;         ///< Button to export frames to CSV
    QPushButton* loadFramesBtn;         ///< Button to import frames from CSV
    QLabel* messageCountLabel;          ///< Label showing current frame count

    // ========================================================================
    // Backend Components
    // ========================================================================

    TcpServer* server;                  ///< TCP server for broadcasting CAN frames
    TcpClient* client;                  ///< TCP client for connecting to a remote server
    MessageModel* messageModel;         ///< Data model holding captured CAN frames

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
};

#endif // MAINWINDOW_H
