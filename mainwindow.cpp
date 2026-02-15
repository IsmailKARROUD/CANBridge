#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>

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

    // Connect server client events to UI handlers
    connect(server, &TcpServer::clientConnected, this, &MainWindow::onServerClientConnected);
    connect(server, &TcpServer::clientDisconnected, this, [this](const QString& address) {
        addServerEvent(QString("Client disconnected: %1").arg(address));
    });

    // Connect client connection events
    connect(client, &TcpClient::connected, this, &MainWindow::onClientConnected);
    connect(client, &TcpClient::disconnected, this, &MainWindow::onClientDisconnected);

    // Update message counter when frames added to model
    connect(messageModel, &MessageModel::frameAdded, this, [this]() {
        messageCountLabel->setText(QString("Messages: %1").arg(messageModel->rowCount()));
    });

    // Auto-update table when frames arrive from network
    connect(client, &TcpClient::frameReceived, messageModel, &MessageModel::addFrame);

    // Handle server errors (status indicator + event log)
    connect(server, &TcpServer::errorOccurred, this, [this](const QString& error) {
        serverStatusIndicator->setText("● Error");
        serverStatusIndicator->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        addServerEvent("Error: " + error);
    });

    // Handle client errors (status label only)
    connect(client, &TcpClient::errorOccurred, this, [this](const QString& error) {
        clientStatusLabel->setText("Error: " + error);
    });

    // Update event log when frames sent
    connect(server, &TcpServer::frameSent, this, [this](const CANFrame& frame) {
        addServerEvent(QString("Frame sent - ID: 0x%1").arg(frame.getId(), 0, 16));
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
    QVBoxLayout* serverGroupLayout = new QVBoxLayout();

    // Line 1: Port, Status Indicator, Buttons
    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->addWidget(new QLabel("Port:"));

    // Port configuration
    serverPortSpin = new QSpinBox();
    serverPortSpin->setRange(1024, 65535);  // Valid TCP port range
    serverPortSpin->setValue(5555);         // Default CAN gateway port
    controlLayout->addWidget(serverPortSpin);

    // Status indicator with colored circle
    serverStatusIndicator = new QLabel("● Stopped");
    serverStatusIndicator->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    QFont indicatorFont = serverStatusIndicator->font();
    indicatorFont.setPointSize(12);
    serverStatusIndicator->setFont(indicatorFont);
    controlLayout->addWidget(serverStatusIndicator);

    // Start/Stop buttons
    startServerBtn = new QPushButton("Start");
    stopServerBtn = new QPushButton("Stop");
    stopServerBtn->setEnabled(false);  // Disabled until server started
    controlLayout->addWidget(startServerBtn);
    controlLayout->addWidget(stopServerBtn);
    controlLayout->addStretch();

    serverGroupLayout->addLayout(controlLayout);

    // Line 2: Event log (last 5 events)
    serverEventLog = new QTextEdit();
    serverEventLog->setReadOnly(true);
    serverEventLog->setMaximumHeight(80);  // ~3-4 lines visible
    serverEventLog->setPlaceholderText("Server events will appear here...");
    serverGroupLayout->addWidget(serverEventLog);

    serverGroup->setLayout(serverGroupLayout);
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
    saveLogBtn = new QPushButton("Save Log");
    loadLogBtn = new QPushButton("Load Log");
    controlLayout->addWidget(clearBtn);
    controlLayout->addWidget(saveLogBtn);
    controlLayout->addWidget(loadLogBtn);
    controlLayout->addStretch();


    tableLayout->addLayout(controlLayout);
    tableGroup->setLayout(tableLayout);
    mainLayout->addWidget(tableGroup);

    tabWidget->addTab(analyzerTab, "Analyzer");

    // Connect UI signals to handler slots
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnect);
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearMessages);
    connect(saveLogBtn, &QPushButton::clicked, this, &MainWindow::onSaveLog);
    connect(loadLogBtn, &QPushButton::clicked, this, &MainWindow::onLoadLog);
}

// ==================== SIMULATOR HANDLERS ====================

// Start TCP server on configured port
void MainWindow::onStartServer()
{
        // Update UI
    if (server->startServer(serverPortSpin->value())) {
        serverStatusIndicator->setText("● Running");
        serverStatusIndicator->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        startServerBtn->setEnabled(false);
        stopServerBtn->setEnabled(true);
        serverPortSpin->setEnabled(false); // Lock port while running

        addServerEvent(QString("Server started on port %1").arg(serverPortSpin->value()));
    } else {
        serverStatusIndicator->setText("● Error");
        serverStatusIndicator->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        addServerEvent("Failed to start server - port may be in use");
    }
}

// Stop TCP server and reset UI
void MainWindow::onStopServer()
{
    server->stopServer();
    serverStatusIndicator->setText("● Stopped");
    serverStatusIndicator->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    startServerBtn->setEnabled(true);
    stopServerBtn->setEnabled(false);
    serverPortSpin->setEnabled(true);

    addServerEvent("Server stopped");
}

// Send a single CAN frame immediately
void MainWindow::onSendFrame()
{
    // Validate CAN ID
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFF) {  // Max 29-bit extended CAN ID
        addServerEvent("Error: Invalid CAN ID");
        return;
    }

    // Validate and parse data bytes
    QStringList dataList = dataEdit->text().split(' ', Qt::SkipEmptyParts);
    if (dataList.size() > 8) {
        addServerEvent("Error: Max 8 data bytes");
        return;
    }

    quint8 data[8] = {0};
    for (int i = 0; i < dataList.size(); ++i) {
        quint32 byte = dataList[i].toUInt(&ok, 16);
        if (!ok || byte > 0xFF) {
            addServerEvent("Error: Invalid data byte");
            return;
        }
        data[i] = static_cast<quint8>(byte);
    }

    CANFrame frame(id, dlcSpin->value(), data);
    server->sendFrame(frame);
}

// Start periodic transmission of configured frame
void MainWindow::onSendPeriodic()
{
    // Parse ID and data (same as onSendFrame)
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFF) {  // Max 29-bit extended CAN ID
        addServerEvent("Error: Invalid CAN ID");
        return;
    }

    // Validate and parse data bytes
    QStringList dataList = dataEdit->text().split(' ', Qt::SkipEmptyParts);
    if (dataList.size() > 8) {
        addServerEvent("Error: Max 8 data bytes");
        return;
    }

    quint8 data[8] = {0};
    for (int i = 0; i < dataList.size(); ++i) {
        quint32 byte = dataList[i].toUInt(&ok, 16);
        if (!ok || byte > 0xFF) {
            addServerEvent("Error: Invalid data byte");
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
    addServerEvent(QString("Client connected: %1").arg(address));
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


void MainWindow::onSaveLog()
{
    QString filename = QFileDialog::getSaveFileName(this, "Save Log", "", "CSV Files (*.csv)");
    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    QTextStream out(&file);
    out << "Timestamp,ID,DLC,Data\n";

    for (int i = 0; i < messageModel->rowCount(); ++i) {
        for (int j = 0; j < 4; ++j) {
            out << messageModel->data(messageModel->index(i, j)).toString();
            if (j < 3) out << ",";
        }
        out << "\n";
    }
}


void MainWindow::onLoadLog()
{
    // Left as exercise - parse CSV and re-inject frames
    QString filename = QFileDialog::getOpenFileName(this,"Select Log file","","CSV Files (*.csv)");
    if (filename.isEmpty()) return;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QString line = in.readLine() ; // skip title line
    int frameCount = 0;
    while(!in.atEnd()){
        line = in.readLine() ;

        QStringList listFrame = line.split(u',', Qt::SkipEmptyParts);

        bool ok;
        QString idText = listFrame[1].remove("0x", Qt::CaseInsensitive);
        quint32 id = idText.toUInt(&ok, 16);
        if (!ok || id > 0x1FFFFFFF) {  // Max 29-bit extended CAN ID
            addServerEvent("Error: Invalid CAN ID");
            return;
        }


        quint8 dlc = listFrame[2].toUInt(&ok, 10);
        if (!ok ) {
            addServerEvent("Error: Invalid DLC");
            return;
        }

        // Validate and parse data bytes
        QStringList dataList = listFrame[3].split(' ', Qt::SkipEmptyParts);
        if (dataList.size() > 8) {
            addServerEvent("Error: Max 8 data bytes");
            return;
        }

        quint8 data[8] = {0};
        for (int i = 0; i < dataList.size(); ++i) {
            quint32 byte = dataList[i].toUInt(&ok, 16);
            if (!ok || byte > 0xFF) {
                addServerEvent("Error: Invalid data byte");
                return;
            }
            data[i] = static_cast<quint8>(byte);
        }

        CANFrame frame(id, dlc, data);

        messageModel->addFrame(frame);
        frameCount++;
    }

    clientStatusLabel->setText("Log loaded");
    clientStatusLabel->setText(QString("Loaded %1 frames").arg(frameCount));


}

void MainWindow::addServerEvent(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp, message);

    // Keep only last 5 events
    eventLogHistory.prepend(logEntry);
    if (eventLogHistory.size() > 5) {
        eventLogHistory.removeLast();
    }

    serverEventLog->setText(eventLogHistory.join("\n"));
}
