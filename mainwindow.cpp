#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>

// Constructor: Initialize main window and all components
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , server(new TcpServer(this))      // Create TCP server for simulator
    , client(new TcpClient(this))      // Create TCP client for analyzer
    , messageModel(new MessageModel(this))  // Model for message table
{
    setWindowTitle("CANBridge - CAN Bus Analyzer/Simulator");
    resize(1000, 600);

    // Create central tab widget (Simulator | Analyzer)
    tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

    // Build UI for both tabs
    setupSimulatorTab();
    setupAnalyzerTab();

    // Connect backend signals to UI slots
    connect(server, &TcpServer::clientConnected, this, &MainWindow::onServerClientConnected);
    connect(client, &TcpClient::connected, this, &MainWindow::onClientConnected);
    connect(client, &TcpClient::disconnected, this, &MainWindow::onClientDisconnected);

    // Update message counter when frames arrive
    connect(client, &TcpClient::frameReceived, this, [this]() {
        messageCountLabel->setText(QString("Messages: %1").arg(messageModel->rowCount()));
    });

    // Auto-update table when frames arrive from network
    connect(client, &TcpClient::frameReceived, messageModel, &MessageModel::addFrame);

    // Handle errors
    connect(server, &TcpServer::errorOccurred, this, [this](const QString& error) {
        serverStatusLabel->setText("Error: " + error);
    });

    connect(client, &TcpClient::errorOccurred, this, [this](const QString& error) {
        clientStatusLabel->setText("Error: " + error);
    });
}

MainWindow::~MainWindow()
{
}

// Build Simulator tab UI (server/transmitter side)
void MainWindow::setupSimulatorTab()
{
    QWidget* simTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(simTab);

    // ========== Server Control Section ==========
    QGroupBox* serverGroup = new QGroupBox("Server Control");
    QHBoxLayout* serverLayout = new QHBoxLayout();

    // Port configuration
    serverLayout->addWidget(new QLabel("Port:"));
    serverPortSpin = new QSpinBox();
    serverPortSpin->setRange(1024, 65535);  // Valid TCP port range
    serverPortSpin->setValue(5555);         // Default CAN gateway port
    serverLayout->addWidget(serverPortSpin);

    // Start/Stop buttons
    startServerBtn = new QPushButton("Start Server");
    stopServerBtn = new QPushButton("Stop Server");
    stopServerBtn->setEnabled(false);  // Disabled until server started
    serverLayout->addWidget(startServerBtn);
    serverLayout->addWidget(stopServerBtn);

    // Status indicator
    serverStatusLabel = new QLabel("Server: Stopped");
    serverLayout->addWidget(serverStatusLabel);
    serverLayout->addStretch();

    serverGroup->setLayout(serverLayout);
    mainLayout->addWidget(serverGroup);

    // ========== Message Configuration Section ==========
    QGroupBox* msgGroup = new QGroupBox("Message Configuration");
    QGridLayout* msgLayout = new QGridLayout();

    // CAN ID input (hex format)
    msgLayout->addWidget(new QLabel("CAN ID (hex):"), 0, 0);
    canIdEdit = new QLineEdit("0x123");
    msgLayout->addWidget(canIdEdit, 0, 1);

    // Data Length Code (0-8 bytes)
    msgLayout->addWidget(new QLabel("DLC:"), 1, 0);
    dlcSpin = new QSpinBox();
    dlcSpin->setRange(0, 8);
    dlcSpin->setValue(8);
    msgLayout->addWidget(dlcSpin, 1, 1);

    // Payload data (hex bytes)
    msgLayout->addWidget(new QLabel("Data (hex):"), 2, 0);
    dataEdit = new QLineEdit("01 02 03 04 05 06 07 08");
    msgLayout->addWidget(dataEdit, 2, 1);

    msgGroup->setLayout(msgLayout);
    mainLayout->addWidget(msgGroup);

    // ========== Transmission Control Section ==========
    QGroupBox* txGroup = new QGroupBox("Transmission");
    QHBoxLayout* txLayout = new QHBoxLayout();

    // One-shot transmission
    sendOnceBtn = new QPushButton("Send Once");
    txLayout->addWidget(sendOnceBtn);

    // Periodic transmission configuration
    txLayout->addWidget(new QLabel("Interval (ms):"));
    intervalSpin = new QSpinBox();
    intervalSpin->setRange(10, 10000);   // 10ms to 10s interval
    intervalSpin->setValue(100);         // Default 100ms (10Hz)
    txLayout->addWidget(intervalSpin);

    sendPeriodicBtn = new QPushButton("Start Periodic");
    stopPeriodicBtn = new QPushButton("Stop Periodic");
    stopPeriodicBtn->setEnabled(false);
    txLayout->addWidget(sendPeriodicBtn);
    txLayout->addWidget(stopPeriodicBtn);
    txLayout->addStretch();

    txGroup->setLayout(txLayout);
    mainLayout->addWidget(txGroup);

    mainLayout->addStretch();
    tabWidget->addTab(simTab, "Simulator");

    // Connect UI signals to handler slots
    connect(startServerBtn, &QPushButton::clicked, this, &MainWindow::onStartServer);
    connect(stopServerBtn, &QPushButton::clicked, this, &MainWindow::onStopServer);
    connect(sendOnceBtn, &QPushButton::clicked, this, &MainWindow::onSendFrame);
    connect(sendPeriodicBtn, &QPushButton::clicked, this, &MainWindow::onSendPeriodic);
    connect(stopPeriodicBtn, &QPushButton::clicked, this, &MainWindow::onStopPeriodic);
    connect(canIdEdit, &QLineEdit::textChanged, [this]() {
        canIdEdit->setStyleSheet("");  // Clear error styling
    });
}

// Build Analyzer tab UI (client/receiver side)
void MainWindow::setupAnalyzerTab()
{
    QWidget* analyzerTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(analyzerTab);

    // ========== Connection Control Section ==========
    QGroupBox* connGroup = new QGroupBox("Connection");
    QHBoxLayout* connLayout = new QHBoxLayout();

    // Server address input
    connLayout->addWidget(new QLabel("Host:"));
    hostEdit = new QLineEdit("127.0.0.1");  // Default to localhost
    connLayout->addWidget(hostEdit);

    // Server port input (must match simulator port)
    connLayout->addWidget(new QLabel("Port:"));
    clientPortSpin = new QSpinBox();
    clientPortSpin->setRange(1024, 65535);
    clientPortSpin->setValue(5555);
    connLayout->addWidget(clientPortSpin);

    // Connect/Disconnect buttons
    connectBtn = new QPushButton("Connect");
    disconnectBtn = new QPushButton("Disconnect");
    disconnectBtn->setEnabled(false);  // Disabled until connected
    connLayout->addWidget(connectBtn);
    connLayout->addWidget(disconnectBtn);

    // Connection status indicator
    clientStatusLabel = new QLabel("Status: Disconnected");
    connLayout->addWidget(clientStatusLabel);
    connLayout->addStretch();

    connGroup->setLayout(connLayout);
    mainLayout->addWidget(connGroup);

    // ========== Message Display Section ==========
    QGroupBox* tableGroup = new QGroupBox("Received Messages");
    QVBoxLayout* tableLayout = new QVBoxLayout();

    // Table view for CAN frames
    messageTable = new QTableView();
    messageTable->setModel(messageModel);  // Connect to data model
    messageTable->horizontalHeader()->setStretchLastSection(true);  // Stretch last column
    tableLayout->addWidget(messageTable);

    // Control bar below table
    QHBoxLayout* controlLayout = new QHBoxLayout();
    messageCountLabel = new QLabel("Messages: 0");
    controlLayout->addWidget(messageCountLabel);

    clearBtn = new QPushButton("Clear");
    controlLayout->addWidget(clearBtn);
    controlLayout->addStretch();

    tableLayout->addLayout(controlLayout);
    tableGroup->setLayout(tableLayout);
    mainLayout->addWidget(tableGroup);

    tabWidget->addTab(analyzerTab, "Analyzer");

    // Connect UI signals to handler slots
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnect);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearMessages);
}

// ==================== SIMULATOR HANDLERS ====================

// Start TCP server on configured port
void MainWindow::onStartServer()
{
    if (server->startServer(serverPortSpin->value())) {
        // Update UI to reflect running state
        serverStatusLabel->setText("Server: Running");
        startServerBtn->setEnabled(false);
        stopServerBtn->setEnabled(true);
        serverPortSpin->setEnabled(false);  // Lock port while running
    }
}

// Stop TCP server and reset UI
void MainWindow::onStopServer()
{
    server->stopServer();
    serverStatusLabel->setText("Server: Stopped");
    startServerBtn->setEnabled(true);
    stopServerBtn->setEnabled(false);
    serverPortSpin->setEnabled(true);
}

// Send a single CAN frame immediately
void MainWindow::onSendFrame()
{
    // Validate CAN ID
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFF) {  // Max 29-bit extended CAN ID
        serverStatusLabel->setText("Error: Invalid CAN ID");
        return;
    }

    // Validate and parse data bytes
    QStringList dataList = dataEdit->text().split(' ', Qt::SkipEmptyParts);
    if (dataList.size() > 8) {
        serverStatusLabel->setText("Error: Max 8 data bytes");
        return;
    }

    quint8 data[8] = {0};
    for (int i = 0; i < dataList.size(); ++i) {
        quint32 byte = dataList[i].toUInt(&ok, 16);
        if (!ok || byte > 0xFF) {
            serverStatusLabel->setText("Error: Invalid data byte");
            return;
        }
        data[i] = static_cast<quint8>(byte);
    }

    CANFrame frame(id, dlcSpin->value(), data);
    server->sendFrame(frame);
    serverStatusLabel->setText("Frame sent");
}

// Start periodic transmission of configured frame
void MainWindow::onSendPeriodic()
{
    // Parse ID and data (same as onSendFrame)
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFF) {  // Max 29-bit extended CAN ID
        serverStatusLabel->setText("Error: Invalid CAN ID");
        return;
    }

    // Validate and parse data bytes
    QStringList dataList = dataEdit->text().split(' ', Qt::SkipEmptyParts);
    if (dataList.size() > 8) {
        serverStatusLabel->setText("Error: Max 8 data bytes");
        return;
    }

    quint8 data[8] = {0};
    for (int i = 0; i < dataList.size(); ++i) {
        quint32 byte = dataList[i].toUInt(&ok, 16);
        if (!ok || byte > 0xFF) {
            serverStatusLabel->setText("Error: Invalid data byte");
            return;
        }
        data[i] = static_cast<quint8>(byte);
    }

    // Add to periodic transmission list
    CANFrame frame(id, dlcSpin->value(), data);
    server->addPeriodicFrame(frame, intervalSpin->value());

    // Update UI state
    sendPeriodicBtn->setEnabled(false);
    stopPeriodicBtn->setEnabled(true);
}

// Stop all periodic transmissions
void MainWindow::onStopPeriodic()
{
    server->clearPeriodicFrames();
    sendPeriodicBtn->setEnabled(true);
    stopPeriodicBtn->setEnabled(false);
}

// ==================== ANALYZER HANDLERS ====================

// Connect to remote TCP server
void MainWindow::onConnect()
{
    client->connectToServer(hostEdit->text(), clientPortSpin->value());
}

// Disconnect from remote server
void MainWindow::onDisconnect()
{
    client->disconnectFromServer();
}

// Clear message table
void MainWindow::onClearMessages()
{
    messageModel->clear();
    messageCountLabel->setText("Messages: 0");
}

// ==================== STATUS UPDATE HANDLERS ====================

// Update status when client connects to our server
void MainWindow::onServerClientConnected(const QString& address)
{
    serverStatusLabel->setText(QString("Server: Client %1 connected").arg(address));
}

// Update UI when analyzer connects successfully
void MainWindow::onClientConnected()
{
    clientStatusLabel->setText("Status: Connected");

    // Disable connection controls while connected
    connectBtn->setEnabled(false);
    disconnectBtn->setEnabled(true);
    hostEdit->setEnabled(false);
    clientPortSpin->setEnabled(false);
}

// Update UI when analyzer disconnects
void MainWindow::onClientDisconnected()
{
    clientStatusLabel->setText("Status: Disconnected");

    // Re-enable connection controls
    connectBtn->setEnabled(true);
    disconnectBtn->setEnabled(false);
    hostEdit->setEnabled(true);
    clientPortSpin->setEnabled(true);
}
