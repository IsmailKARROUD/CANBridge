/**
 * @file mainwindow.cpp
 * @brief Implementation of the MainWindow class.
 *
 * Contains the construction of all four tabs (Connection, Log, Simulator,
 * Analyzer), signal/slot wiring between the UI and backend components
 * (TcpServer, TcpClient, MessageModel), event handlers, theme management,
 * and CSV import/export logic.
 */

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QDateTime>
#include <QTextStream>
#include <QMenuBar>
#include <QActionGroup>
#include <QApplication>

#include "mainwindow.h"
#include "aboutdialog.h"

// ============================================================================
// Constructor / Destructor
// ============================================================================

/**
 * Build the entire UI, instantiate backend components, and wire all
 * signal/slot connections between the server, client, model, and UI.
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , server(new TcpServer(this))
    , client(new TcpClient(this))
    , messageModel(new MessageModel(this))
    , filterProxyModel(new FrameFilterProxyModel(this))
    , currentLogFilter("All")
{
    // Set up the proxy model chain for the Analyzer tab filtering:
    //   QTableView  ->  FrameFilterProxyModel  ->  MessageModel
    // The proxy intercepts row access and hides rows that don't match
    // the user's filter criteria, without modifying the underlying data.
    filterProxyModel->setSourceModel(messageModel);

    setWindowTitle("CANBridge - CAN Bus Analyzer/Simulator");
    resize(1000, 600);

    // Central tab widget holds all four application tabs
    tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

    // Build each tab's layout and widgets
    setupConnectionTab();
    setupLogTab();
    setupSimulatorTab();
    setupAnalyzerTab();

    // Initialize theme and menu bar
    isDarkMode = false;
    setupMenuBar();

    // ==== Server signal connections ====

    // Log when a remote client connects to our server
    connect(server, &TcpServer::clientConnected, this, &MainWindow::onServerClientConnected);

    // Log when a remote client disconnects
    connect(server, &TcpServer::clientDisconnected, this, [this](const QString& address) {
        addLogEvent(QString("Client disconnected: %1").arg(address), "Server");
    });

    // Display server errors in the status indicator and log
    connect(server, &TcpServer::errorOccurred, this, [this](const QString& error) {
        serverStatusIndicator->setText("● Error");
        serverStatusIndicator->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        addLogEvent("Server error: " + error, "Server");
    });

    // Log outgoing frames and update the simulator status label
    connect(server, &TcpServer::frameSent, this, [this](const CANFrame& frame) {
        messageModel->addFrame(frame, true);  // TX
        addLogEvent(QString("Frame sent - ID: 0x%1").arg(frame.getId(), 0, 16), "Frame");
        lastFrameStatusLabel->setText(QString("✓ Sent ID: 0x%1").arg(frame.getId(), 0, 16));
        lastFrameStatusLabel->setStyleSheet("QLabel { color: green; }");
    });


    // Add frames received by the server to the analyzer model and log frames received by the server
    connect(server, &TcpServer::frameReceived, this, [this](const CANFrame& frame) {
        messageModel->addFrame(frame, false);  // RX
        addLogEvent(QString("Frame received - ID: 0x%1").arg(frame.getId(), 0, 16), "Frame");
    });

    // ==== Client signal connections ====

    // Handle client connection/disconnection for UI updates
    connect(client, &TcpClient::connected, this, &MainWindow::onClientConnected);
    connect(client, &TcpClient::disconnected, this, &MainWindow::onClientDisconnected);

    // Display client errors
    connect(client, &TcpClient::errorOccurred, this, [this](const QString& error) {
        clientStatusIndicator->setText("● Error");
        clientStatusIndicator->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        addLogEvent("Client error: " + error, "Client");
    });

    // Add frames received by the client to the analyzer model and log
    connect(client, &TcpClient::frameReceived, this, [this](const CANFrame& frame) {
        messageModel->addFrame(frame, false);  // RX
        addLogEvent(QString("Frame received - ID: 0x%1").arg(frame.getId(), 0, 16), "Frame");
    });

    // Log frames sent by the client and update the simulator status label
    connect(client, &TcpClient::frameSent, this, [this](const CANFrame& frame) {
        addLogEvent(QString("Frame sent - ID: 0x%1").arg(frame.getId(), 0, 16), "Frame");
        messageModel->addFrame(frame, true);  // TX
        lastFrameStatusLabel->setText(QString("✓ Sent ID: 0x%1").arg(frame.getId(), 0, 16));
        lastFrameStatusLabel->setStyleSheet("QLabel { color: green; }");
    });

    // ==== Model signal connections ====

    // Update the message count label whenever a new frame is added
    connect(messageModel, &MessageModel::frameAdded, this, [this]() {
        messageCountLabel->setText(QString("Messages: %1").arg(messageModel->rowCount()));
    });
}

MainWindow::~MainWindow()
{
}

// ============================================================================
// Menu Bar Setup
// ============================================================================

/**
 * Create the application menu bar with:
 *  - CANBridge menu: About dialog and Quit action
 *  - View menu: Theme selection (Auto/Light/Dark)
 */
void MainWindow::setupMenuBar()
{
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // -- CANBridge application menu --
    QMenu* appMenu = menuBar->addMenu("CANBridge");

    QAction* aboutAction = appMenu->addAction("About CANBridge");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        AboutDialog dialog(this);
        dialog.exec();
    });

    appMenu->addSeparator();

    QAction* quitAction = appMenu->addAction("Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    // -- View menu with theme selection --
    QMenu* viewMenu = menuBar->addMenu("View");

    QMenu* themeMenu = viewMenu->addMenu("Theme");
    QActionGroup* themeGroup = new QActionGroup(this);  // Ensures mutual exclusivity

    QAction* autoThemeAction = themeMenu->addAction("Auto (System)");
    autoThemeAction->setCheckable(true);
    autoThemeAction->setChecked(true);
    themeGroup->addAction(autoThemeAction);

    QAction* lightThemeAction = themeMenu->addAction("Light");
    lightThemeAction->setCheckable(true);
    themeGroup->addAction(lightThemeAction);

    darkModeAction = themeMenu->addAction("Dark");
    darkModeAction->setCheckable(true);
    themeGroup->addAction(darkModeAction);

    connect(autoThemeAction, &QAction::triggered, this, [this]() {
        applyTheme(false);  // Default to light (platform detection not yet implemented)
    });

    connect(lightThemeAction, &QAction::triggered, this, [this]() {
        applyTheme(false);
    });

    connect(darkModeAction, &QAction::triggered, this, [this]() {
        applyTheme(true);
    });

    // -- Timestamp format selection --
    QMenu* timestampMenu = viewMenu->addMenu("Timestamp Format");
    QActionGroup* tsGroup = new QActionGroup(this);

    QAction* timeOnlyAction = timestampMenu->addAction("Time Only (HH:mm:ss.zzz)");
    timeOnlyAction->setCheckable(true);
    timeOnlyAction->setChecked(true);
    tsGroup->addAction(timeOnlyAction);

    QAction* dateTimeAction = timestampMenu->addAction("Date && Time (yyyy-MMM-dd HH:mm:ss.zzz)");
    dateTimeAction->setCheckable(true);
    tsGroup->addAction(dateTimeAction);

    QAction* rawMsAction = timestampMenu->addAction("Raw Milliseconds");
    rawMsAction->setCheckable(true);
    tsGroup->addAction(rawMsAction);

    connect(timeOnlyAction, &QAction::triggered, this, [this]() {
        messageModel->setTimestampFormat("hh:mm:ss.zzz");
    });

    connect(dateTimeAction, &QAction::triggered, this, [this]() {
        messageModel->setTimestampFormat("yyyy-MMM-dd hh:mm:ss.zzz");
    });

    connect(rawMsAction, &QAction::triggered, this, [this]() {
        messageModel->setTimestampFormat("ms");
    });
}

/**
 * Apply a dark or light theme to the entire application via stylesheet.
 * Dark theme uses a custom stylesheet; light theme clears the stylesheet
 * to restore Qt's default platform look.
 */
void MainWindow::applyTheme(bool isDark)
{
    isDarkMode = isDark;

    if (isDark) {
        qApp->setStyleSheet(R"(
            QMainWindow, QWidget {
                background-color: #2b2b2b;
                color: #ffffff;
            }
            QGroupBox {
                border: 1px solid #555555;
                border-radius: 5px;
                margin-top: 10px;
                font-weight: bold;
                color: #ffffff;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
            QLineEdit, QSpinBox, QTextEdit {
                background-color: #3c3c3c;
                color: #ffffff;
                border: 1px solid #555555;
                border-radius: 3px;
                padding: 5px;
            }
            QPushButton {
                background-color: #4a4a4a;
                color: #ffffff;
                border: 1px solid #666666;
                border-radius: 5px;
                padding: 5px 15px;
            }
            QPushButton:hover {
                background-color: #5a5a5a;
            }
            QPushButton:pressed {
                background-color: #3a3a3a;
            }
            QPushButton:disabled {
                background-color: #2b2b2b;
                color: #666666;
            }
            QTableView {
                background-color: #3c3c3c;
                color: #ffffff;
                gridline-color: #555555;
            }
            QHeaderView::section {
                background-color: #4a4a4a;
                color: #ffffff;
                border: 1px solid #555555;
                padding: 5px;
            }
            QTabWidget::pane {
                border: 1px solid #555555;
            }
            QTabBar::tab {
                background-color: #4a4a4a;
                color: #ffffff;
                border: 1px solid #555555;
                padding: 8px 16px;
            }
            QTabBar::tab:selected {
                background-color: #2b2b2b;
            }
            QComboBox {
                background-color: #3c3c3c;
                color: #ffffff;
                border: 1px solid #555555;
                border-radius: 3px;
                padding: 5px;
            }
            QComboBox::drop-down {
                border: none;
            }
        )");
    } else {
        // Clear stylesheet to restore default platform appearance
        qApp->setStyleSheet("");
    }
}

// ============================================================================
// Tab 1: Connection
// ============================================================================

/**
 * Build the Connection tab with two sections:
 *  - Server: port selector, start/stop buttons, status indicator
 *  - Client: host/port inputs, connect/disconnect buttons, status indicator
 */
void MainWindow::setupConnectionTab()
{
    QWidget* connTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(connTab);

    // -- Server section --
    QGroupBox* serverGroup = new QGroupBox("Server");
    QHBoxLayout* serverLayout = new QHBoxLayout();

    serverLayout->addWidget(new QLabel("Port:"));
    serverPortSpin = new QSpinBox();
    serverPortSpin->setRange(1024, 65535);  // Valid non-privileged port range
    serverPortSpin->setValue(5555);          // Default port
    serverLayout->addWidget(serverPortSpin);

    serverStatusIndicator = new QLabel("● Stopped");
    serverStatusIndicator->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    QFont font = serverStatusIndicator->font();
    font.setPointSize(12);
    serverStatusIndicator->setFont(font);
    serverLayout->addWidget(serverStatusIndicator);

    startServerBtn = new QPushButton("Start");
    stopServerBtn = new QPushButton("Stop");
    stopServerBtn->setEnabled(false);       // Disabled until server is started
    serverLayout->addWidget(startServerBtn);
    serverLayout->addWidget(stopServerBtn);
    serverLayout->addStretch();

    serverGroup->setLayout(serverLayout);
    mainLayout->addWidget(serverGroup);

    // -- Client section --
    QGroupBox* clientGroup = new QGroupBox("Client");
    QHBoxLayout* clientLayout = new QHBoxLayout();

    clientLayout->addWidget(new QLabel("Host:"));
    hostEdit = new QLineEdit("127.0.0.1");  // Default to localhost
    clientLayout->addWidget(hostEdit);

    clientLayout->addWidget(new QLabel("Port:"));
    clientPortSpin = new QSpinBox();
    clientPortSpin->setRange(1024, 65535);
    clientPortSpin->setValue(5555);
    clientLayout->addWidget(clientPortSpin);

    clientStatusIndicator = new QLabel("● Disconnected");
    clientStatusIndicator->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    clientStatusIndicator->setFont(font);
    clientLayout->addWidget(clientStatusIndicator);

    connectBtn = new QPushButton("Connect");
    disconnectBtn = new QPushButton("Disconnect");
    disconnectBtn->setEnabled(false);       // Disabled until connected
    clientLayout->addWidget(connectBtn);
    clientLayout->addWidget(disconnectBtn);
    clientLayout->addStretch();

    clientGroup->setLayout(clientLayout);
    mainLayout->addWidget(clientGroup);

    mainLayout->addStretch();
    tabWidget->addTab(connTab, "Connection");

    // Wire button signals to slot handlers
    connect(startServerBtn, &QPushButton::clicked, this, &MainWindow::onStartServer);
    connect(stopServerBtn, &QPushButton::clicked, this, &MainWindow::onStopServer);
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnect);
}

// ============================================================================
// Tab 2: Log
// ============================================================================

/**
 * Build the Log tab with a category filter dropdown and a read-only text area
 * that displays timestamped events from server, client, and frame operations.
 */
void MainWindow::setupLogTab()
{
    QWidget* logTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(logTab);

    // Category filter dropdown
    QHBoxLayout* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("Filter:"));
    logFilterCombo = new QComboBox();
    logFilterCombo->addItems({"All Events", "Server", "Client", "Frame"});
    filterLayout->addWidget(logFilterCombo);
    filterLayout->addStretch();
    mainLayout->addLayout(filterLayout);

    // Read-only log display area
    logDisplay = new QTextEdit();
    logDisplay->setReadOnly(true);
    logDisplay->setPlaceholderText("Events will appear here...");
    mainLayout->addWidget(logDisplay);

    tabWidget->addTab(logTab, "Log");

    connect(logFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLogFilterChanged);
}

// ============================================================================
// Tab 3: Simulator
// ============================================================================

/**
 * Build the Simulator tab with:
 *  - Message configuration: CAN ID (hex), DLC (0-8), data bytes (hex)
 *  - Transmission controls: Send Once, periodic interval, Start/Stop Periodic
 *  - Status display showing the last send operation result
 */
void MainWindow::setupSimulatorTab()
{
    QWidget* simTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(simTab);

    // -- Message configuration group --
    QGroupBox* msgGroup = new QGroupBox("Message Configuration");
    QGridLayout* msgLayout = new QGridLayout();

    msgLayout->addWidget(new QLabel("CAN ID (hex):"), 0, 0);
    canIdEdit = new QLineEdit("0x123");         // Default CAN ID
    msgLayout->addWidget(canIdEdit, 0, 1);

    msgLayout->addWidget(new QLabel("DLC:"), 1, 0);
    dlcSpin = new QSpinBox();
    dlcSpin->setRange(0, 8);                    // CAN DLC range
    dlcSpin->setValue(8);
    msgLayout->addWidget(dlcSpin, 1, 1);

    msgLayout->addWidget(new QLabel("Data (hex):"), 2, 0);
    dataEdit = new QLineEdit("01 02 03 04 05 06 07 08");  // Default data bytes
    msgLayout->addWidget(dataEdit, 2, 1);

    msgGroup->setLayout(msgLayout);
    mainLayout->addWidget(msgGroup);

    // -- Transmission controls group --
    QGroupBox* txGroup = new QGroupBox("Transmission");
    QHBoxLayout* txLayout = new QHBoxLayout();

    sendOnceBtn = new QPushButton("Send Once");
    txLayout->addWidget(sendOnceBtn);

    txLayout->addWidget(new QLabel("Interval (ms):"));
    intervalSpin = new QSpinBox();
    intervalSpin->setRange(10, 10000);          // 10ms to 10s range
    intervalSpin->setValue(100);                 // Default 100ms interval
    txLayout->addWidget(intervalSpin);

    sendPeriodicBtn = new QPushButton("Start Periodic");
    stopPeriodicBtn = new QPushButton("Stop Periodic");
    stopPeriodicBtn->setEnabled(false);         // Disabled until periodic starts
    txLayout->addWidget(sendPeriodicBtn);
    txLayout->addWidget(stopPeriodicBtn);
    txLayout->addStretch();

    txGroup->setLayout(txLayout);
    mainLayout->addWidget(txGroup);

    // -- Last action status --
    QHBoxLayout* statusLayout = new QHBoxLayout();
    statusLayout->addWidget(new QLabel("Last action:"));
    lastFrameStatusLabel = new QLabel("No frames sent yet");
    lastFrameStatusLabel->setStyleSheet("QLabel { color: gray; }");
    statusLayout->addWidget(lastFrameStatusLabel);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);

    mainLayout->addStretch();
    tabWidget->addTab(simTab, "Simulator");

    // Wire button signals
    connect(sendOnceBtn, &QPushButton::clicked, this, &MainWindow::onSendFrame);
    connect(sendPeriodicBtn, &QPushButton::clicked, this, &MainWindow::onSendPeriodic);
    connect(stopPeriodicBtn, &QPushButton::clicked, this, &MainWindow::onStopPeriodic);
}

// ============================================================================
// Tab 4: Analyzer
// ============================================================================

/**
 * Build the Analyzer tab with filter controls, a QTableView bound to the
 * filter proxy model, and buttons for clearing, saving, and loading frames.
 */
void MainWindow::setupAnalyzerTab()
{
    QWidget* analyzerTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(analyzerTab);

    // -- Filter bar --
    // Horizontal row of filter controls above the frame table.
    // All filters work in real-time (update on every keystroke or selection change).
    QHBoxLayout* filterLayout = new QHBoxLayout();

    // Direction filter: combo box with "All" (no filter), "TX", or "RX"
    filterLayout->addWidget(new QLabel("Dir:"));
    dirFilterCombo = new QComboBox();
    dirFilterCombo->addItems({"All", "TX", "RX"});
    filterLayout->addWidget(dirFilterCombo);

    // CAN ID filter: free-text hex substring match (e.g., "1A3" matches "0x1A3")
    filterLayout->addWidget(new QLabel("ID:"));
    idFilterEdit = new QLineEdit();
    idFilterEdit->setPlaceholderText("e.g. 1A3");
    idFilterEdit->setMaximumWidth(120);
    filterLayout->addWidget(idFilterEdit);

    // Data filter: free-text hex substring match (e.g., "FF 01" matches data containing those bytes)
    filterLayout->addWidget(new QLabel("Data:"));
    dataFilterEdit = new QLineEdit();
    dataFilterEdit->setPlaceholderText("e.g. FF 01");
    dataFilterEdit->setMaximumWidth(200);
    filterLayout->addWidget(dataFilterEdit);

    filterLayout->addStretch();
    mainLayout->addLayout(filterLayout);

    // -- Frame display table --
    // Bound to the filter proxy model (not directly to MessageModel)
    // so that filter changes hide/show rows without modifying the data.
    messageTable = new QTableView();
    messageTable->setModel(filterProxyModel);
    messageTable->horizontalHeader()->setStretchLastSection(true);
    mainLayout->addWidget(messageTable);

    // -- Control bar: message count, clear, save, load --
    QHBoxLayout* controlLayout = new QHBoxLayout();
    messageCountLabel = new QLabel("Messages: 0");
    controlLayout->addWidget(messageCountLabel);

    clearBtn = new QPushButton("Clear");
    saveFramesBtn = new QPushButton("Save Frames");
    loadFramesBtn = new QPushButton("Load Frames");
    controlLayout->addWidget(clearBtn);
    controlLayout->addWidget(saveFramesBtn);
    controlLayout->addWidget(loadFramesBtn);
    controlLayout->addStretch();

    mainLayout->addLayout(controlLayout);
    tabWidget->addTab(analyzerTab, "Analyzer");

    // Wire filter signals — each control updates the proxy model in real-time.
    // Each setter calls beginResetModel()/endResetModel() internally, which
    // causes the table to instantly show/hide rows matching the new criteria.
    connect(dirFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        filterProxyModel->setDirectionFilter(dirFilterCombo->currentText());
    });
    connect(idFilterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        filterProxyModel->setIdFilter(text);
    });
    connect(dataFilterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        filterProxyModel->setDataFilter(text);
    });

    // Wire button signals
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearMessages);
    connect(saveFramesBtn, &QPushButton::clicked, this, &MainWindow::onSaveFrames);
    connect(loadFramesBtn, &QPushButton::clicked, this, &MainWindow::onLoadFrames);
}

// ============================================================================
// Connection Handlers
// ============================================================================

/**
 * Start the TCP server on the port specified in the UI.
 * Updates the status indicator and enables/disables controls accordingly.
 */
void MainWindow::onStartServer()
{
    if (server->startServer(serverPortSpin->value())) {
        serverStatusIndicator->setText("● Running");
        serverStatusIndicator->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        startServerBtn->setEnabled(false);
        stopServerBtn->setEnabled(true);
        serverPortSpin->setEnabled(false);  // Lock port while server is running

        addLogEvent(QString("Server started on port %1").arg(serverPortSpin->value()), "Server");
    } else {
        serverStatusIndicator->setText("● Error");
        serverStatusIndicator->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        addLogEvent("Failed to start server - port may be in use", "Server");
    }
}

/**
 * Stop the TCP server, reset the status indicator, and re-enable controls.
 */
void MainWindow::onStopServer()
{
    server->stopServer();
    serverStatusIndicator->setText("● Stopped");
    serverStatusIndicator->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    startServerBtn->setEnabled(true);
    stopServerBtn->setEnabled(false);
    serverPortSpin->setEnabled(true);  // Unlock port for reconfiguration

    addLogEvent("Server stopped", "Server");
}

/**
 * Initiate a TCP connection to the host and port specified in the UI.
 */
void MainWindow::onConnect()
{
    client->connectToServer(hostEdit->text(), clientPortSpin->value());
}

/**
 * Disconnect the client from the remote server.
 */
void MainWindow::onDisconnect()
{
    client->disconnectFromServer();
}

/**
 * Called when the client successfully connects to a remote server.
 * Updates the status indicator and locks the host/port inputs.
 */
void MainWindow::onClientConnected()
{
    clientStatusIndicator->setText("● Connected");
    clientStatusIndicator->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    connectBtn->setEnabled(false);
    disconnectBtn->setEnabled(true);
    hostEdit->setEnabled(false);        // Lock inputs while connected
    clientPortSpin->setEnabled(false);

    addLogEvent(QString("Connected to %1:%2").arg(hostEdit->text()).arg(clientPortSpin->value()), "Client");
}

/**
 * Called when the client disconnects from the remote server.
 * Resets the status indicator and unlocks host/port inputs.
 */
void MainWindow::onClientDisconnected()
{
    clientStatusIndicator->setText("● Disconnected");
    clientStatusIndicator->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    connectBtn->setEnabled(true);
    disconnectBtn->setEnabled(false);
    hostEdit->setEnabled(true);         // Unlock inputs for reconfiguration
    clientPortSpin->setEnabled(true);

    addLogEvent("Disconnected from server", "Client");
}

/**
 * Log when a remote client connects to our server.
 */
void MainWindow::onServerClientConnected(const QString& address)
{
    addLogEvent(QString("Client connected: %1").arg(address), "Server");
}

// ============================================================================
// Simulator Handlers
// ============================================================================

/**
 * Parse the CAN ID, DLC, and data bytes from the simulator UI inputs,
 * construct a CANFrame, and send it via server (if running) or client (if connected).
 *
 * Validates:
 *  - Server is running or client is connected
 *  - CAN ID is valid hex and within 29-bit range (0x1FFFFFFF max)
 *  - Data bytes are valid hex and at most 8 bytes
 */
void MainWindow::onSendFrame()
{
    // Require an active connection (server or client)
    if (!server->isListening() && !client->isConnected()) {
        lastFrameStatusLabel->setText("✗ Error: Server not running and client are not connected");
        lastFrameStatusLabel->setStyleSheet("QLabel { color: red; }");
        return;
    }

    // Parse CAN ID from hex input
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFF) {  // 29-bit max for extended CAN ID
        addLogEvent("Error: Invalid CAN ID", "Frame");
        lastFrameStatusLabel->setText("✗ Error: Invalid CAN ID");
        lastFrameStatusLabel->setStyleSheet("QLabel { color: red; }");
        return;
    }

    // Parse data bytes from space-separated hex string
    QStringList dataList = dataEdit->text().split(' ', Qt::SkipEmptyParts);
    if (dataList.size() > 8) {
        addLogEvent("Error: Max 8 data bytes", "Frame");
        return;
    }

    quint8 data[8] = {0};
    for (int i = 0; i < dataList.size(); ++i) {
        quint32 byte = dataList[i].toUInt(&ok, 16);
        if (!ok || byte > 0xFF) {
            addLogEvent("Error: Invalid data byte", "Frame");
            return;
        }
        data[i] = static_cast<quint8>(byte);
    }

    // Build and send the frame via the active channel
    CANFrame frame(id, dlcSpin->value(), data);
    if (server->isListening()) {
        server->sendFrame(frame);
    } else if (client->isConnected()) {
        client->sendFrame(frame);
    } else {
        lastFrameStatusLabel->setText("✗ Error: Server not running and client are not connected");
        lastFrameStatusLabel->setStyleSheet("QLabel { color: red; }");
    }
}

/**
 * Parse a CAN frame from the UI and start periodic transmission.
 * Uses the server's periodic timer if the server is running, otherwise
 * sends a single frame via the client (periodic client transmission not yet supported).
 */
void MainWindow::onSendPeriodic()
{
    // Require an active connection
    if (!server->isListening() && !client->isConnected()) {
        lastFrameStatusLabel->setText("✗ Error: Server not running and client are not connected");
        lastFrameStatusLabel->setStyleSheet("QLabel { color: red; }");
        return;
    }

    // Parse CAN ID
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFF) {
        addLogEvent("Error: Invalid CAN ID", "Frame");
        return;
    }

    // Parse data bytes
    QStringList dataList = dataEdit->text().split(' ', Qt::SkipEmptyParts);
    if (dataList.size() > 8) {
        addLogEvent("Error: Max 8 data bytes", "Frame");
        return;
    }

    quint8 data[8] = {0};
    for (int i = 0; i < dataList.size(); ++i) {
        quint32 byte = dataList[i].toUInt(&ok, 16);
        if (!ok || byte > 0xFF) {
            addLogEvent("Error: Invalid data byte", "Frame");
            return;
        }
        data[i] = static_cast<quint8>(byte);
    }

    CANFrame frame(id, dlcSpin->value(), data);

    if (server->isListening()) {
        server->addPeriodicFrame(frame, intervalSpin->value());
    } else if (client->isConnected()) {
        client->addPeriodicFrame(frame, intervalSpin->value());
    } else {
        lastFrameStatusLabel->setText("✗ Error: Server not running and client are not connected");
        lastFrameStatusLabel->setStyleSheet("QLabel { color: red; }");
    }

    // Update button states
    sendPeriodicBtn->setEnabled(false);
    stopPeriodicBtn->setEnabled(true);

    addLogEvent(QString("Started periodic transmission (ID: 0x%1, Interval: %2ms)")
                    .arg(id, 0, 16).arg(intervalSpin->value()), "Frame");
}

/**
 * Stop all periodic frame transmissions and reset button states.
 */
void MainWindow::onStopPeriodic()
{
    server->clearPeriodicFrames();
    client->clearPeriodicFrames();
    sendPeriodicBtn->setEnabled(true);
    stopPeriodicBtn->setEnabled(false);

    addLogEvent("Stopped periodic transmission", "Frame");
}

// ============================================================================
// Analyzer Handlers
// ============================================================================

/**
 * Clear all captured frames from the model and reset the message count.
 */
void MainWindow::onClearMessages()
{
    messageModel->clear();
    messageCountLabel->setText("Messages: 0");
}

/**
 * Export all captured frames to a CSV file.
 *
 * CSV format:
 *   Timestamp,Dir,ID,DLC,Data
 *   12:34:56.789,0x123,8,01 02 03 04 05 06 07 08
 */
void MainWindow::onSaveFrames()
{
    QString filename = QFileDialog::getSaveFileName(this, "Save Frames", "", "CSV Files (*.csv)");
    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "Timestamp,Dir,ID,DLC,Data\n";  // CSV header

    // Write each frame as a CSV row
    for (int i = 0; i < messageModel->rowCount(); ++i) {
        for (int j = 0; j < 5; ++j) {
            out << messageModel->data(messageModel->index(i, j)).toString();
            if (j < 4) out << ",";
        }
        out << "\n";
    }

    addLogEvent(QString("Saved %1 frames to %2").arg(messageModel->rowCount()).arg(filename), "Frame");
}

/**
 * Import CAN frames from a CSV file and add them to the analyzer model.
 *
 * Expected CSV format:
 *   Timestamp,Dir,ID,DLC,Data
 *   (timestamp is ignored on import; a new timestamp is generated)
 *
 * Invalid rows are silently skipped.
 */
void MainWindow::onLoadFrames()
{
    QString filename = QFileDialog::getOpenFileName(this, "Load Frames", "", "CSV Files (*.csv)");
    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    in.readLine();  // Skip CSV header row

    int frameCount = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(',', Qt::SkipEmptyParts);

        if (fields.size() < 5) continue;  // Skip malformed rows

        // Parse CAN ID
        bool ok;
        QString idText = fields[2].remove("0x", Qt::CaseInsensitive);
        quint32 id = idText.toUInt(&ok, 16);
        if (!ok || id > 0x1FFFFFFF) continue;

        // Parse DLC
        quint8 dlc = fields[3].toUInt(&ok, 10);
        if (!ok) continue;

        // Parse data bytes
        QStringList dataList = fields[4].split(' ', Qt::SkipEmptyParts);
        if (dataList.size() > 8) continue;

        quint8 data[8] = {0};
        for (int i = 0; i < dataList.size(); ++i) {
            quint32 byte = dataList[i].toUInt(&ok, 16);
            if (!ok || byte > 0xFF) continue;
            data[i] = static_cast<quint8>(byte);
        }

        CANFrame frame(id, dlc, data);
        messageModel->addFrame(frame,fields[1]=="TX");
        frameCount++;
    }

    addLogEvent(QString("Loaded %1 frames from %2").arg(frameCount).arg(filename), "Frame");
}

// ============================================================================
// Log Management
// ============================================================================

/**
 * Handle log filter dropdown changes.
 * Maps combo box indices to filter strings and refreshes the display.
 */
void MainWindow::onLogFilterChanged(int index)
{
    QStringList filters = {"All", "Server", "Client", "Frame"};
    currentLogFilter = filters[index];
    updateLogDisplay();
}

/**
 * Add a new log entry with the current timestamp.
 * Entries are prepended (newest first) and capped at 50 to limit memory usage.
 */
void MainWindow::addLogEvent(const QString& message, const QString& category)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    entry.category = category;
    entry.message = message;

    logHistory.prepend(entry);  // Newest entries first

    // Cap log history at 50 entries
    if (logHistory.size() > 50) {
        logHistory.removeLast();
    }

    updateLogDisplay();
}

/**
 * Rebuild the log display text by filtering log entries according to
 * the currently selected category filter.
 *
 * Output format: [HH:MM:SS] [Category] Message text
 */
void MainWindow::updateLogDisplay()
{
    QStringList displayLines;

    for (const LogEntry& entry : logHistory) {
        // Skip entries that don't match the active filter
        if (currentLogFilter != "All" && entry.category != currentLogFilter) {
            continue;
        }

        QString line = QString("[%1] [%2] %3")
                           .arg(entry.timestamp)
                           .arg(entry.category)
                           .arg(entry.message);
        displayLines.append(line);
    }

    logDisplay->setText(displayLines.join("\n"));
}
