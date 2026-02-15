#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QDateTime>
#include <QTextStream>

// Constructor: Initialize main window and all components
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , server(new TcpServer(this))
    , client(new TcpClient(this))
    , messageModel(new MessageModel(this))
    , currentLogFilter("All")
{
    setWindowTitle("CANBridge - CAN Bus Analyzer/Simulator");
    resize(1000, 600);

    // Create central tab widget
    tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

    // Build all tabs
    setupConnectionTab();
    setupLogTab();
    setupSimulatorTab();
    setupAnalyzerTab();

    // ========== Server event connections ==========
    connect(server, &TcpServer::clientConnected, this, &MainWindow::onServerClientConnected);
    connect(server, &TcpServer::clientDisconnected, this, [this](const QString& address) {
        addLogEvent(QString("Client disconnected: %1").arg(address), "Server");
    });
    connect(server, &TcpServer::errorOccurred, this, [this](const QString& error) {
        serverStatusIndicator->setText("● Error");
        serverStatusIndicator->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        addLogEvent("Server error: " + error, "Server");
    });
    connect(server, &TcpServer::frameSent, this, [this](const CANFrame& frame) {
        addLogEvent(QString("Frame sent - ID: 0x%1").arg(frame.getId(), 0, 16), "Frame");
    });

    // ========== Client event connections ==========
    connect(client, &TcpClient::connected, this, &MainWindow::onClientConnected);
    connect(client, &TcpClient::disconnected, this, &MainWindow::onClientDisconnected);
    connect(client, &TcpClient::errorOccurred, this, [this](const QString& error) {
        clientStatusIndicator->setText("● Error");
        clientStatusIndicator->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        addLogEvent("Client error: " + error, "Client");
    });
    connect(client, &TcpClient::frameReceived, messageModel, &MessageModel::addFrame);
    connect(client, &TcpClient::frameReceived, this, [this](const CANFrame& frame) {
        addLogEvent(QString("Frame received - ID: 0x%1").arg(frame.getId(), 0, 16), "Frame");
    });

    // ========== UI updates ==========
    connect(messageModel, &MessageModel::frameAdded, this, [this]() {
        messageCountLabel->setText(QString("Messages: %1").arg(messageModel->rowCount()));
    });
}

MainWindow::~MainWindow()
{
}

// ========== TAB 1: CONNECTION ==========
void MainWindow::setupConnectionTab()
{
    QWidget* connTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(connTab);

    // ===== Server Section =====
    QGroupBox* serverGroup = new QGroupBox("Server");
    QHBoxLayout* serverLayout = new QHBoxLayout();

    serverLayout->addWidget(new QLabel("Port:"));
    serverPortSpin = new QSpinBox();
    serverPortSpin->setRange(1024, 65535);
    serverPortSpin->setValue(5555);
    serverLayout->addWidget(serverPortSpin);

    serverStatusIndicator = new QLabel("● Stopped");
    serverStatusIndicator->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    QFont font = serverStatusIndicator->font();
    font.setPointSize(12);
    serverStatusIndicator->setFont(font);
    serverLayout->addWidget(serverStatusIndicator);

    startServerBtn = new QPushButton("Start");
    stopServerBtn = new QPushButton("Stop");
    stopServerBtn->setEnabled(false);
    serverLayout->addWidget(startServerBtn);
    serverLayout->addWidget(stopServerBtn);
    serverLayout->addStretch();

    serverGroup->setLayout(serverLayout);
    mainLayout->addWidget(serverGroup);

    // ===== Client Section =====
    QGroupBox* clientGroup = new QGroupBox("Client");
    QHBoxLayout* clientLayout = new QHBoxLayout();

    clientLayout->addWidget(new QLabel("Host:"));
    hostEdit = new QLineEdit("127.0.0.1");
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
    disconnectBtn->setEnabled(false);
    clientLayout->addWidget(connectBtn);
    clientLayout->addWidget(disconnectBtn);
    clientLayout->addStretch();

    clientGroup->setLayout(clientLayout);
    mainLayout->addWidget(clientGroup);

    mainLayout->addStretch();
    tabWidget->addTab(connTab, "Connection");

    // Connect signals
    connect(startServerBtn, &QPushButton::clicked, this, &MainWindow::onStartServer);
    connect(stopServerBtn, &QPushButton::clicked, this, &MainWindow::onStopServer);
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::onConnect);
    connect(disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnect);
}

// ========== TAB 2: LOG ==========
void MainWindow::setupLogTab()
{
    QWidget* logTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(logTab);

    // Filter controls
    QHBoxLayout* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("Filter:"));
    logFilterCombo = new QComboBox();
    logFilterCombo->addItems({"All Events", "Server", "Client", "Frame"});
    filterLayout->addWidget(logFilterCombo);
    filterLayout->addStretch();
    mainLayout->addLayout(filterLayout);

    // Log display
    logDisplay = new QTextEdit();
    logDisplay->setReadOnly(true);
    logDisplay->setPlaceholderText("Events will appear here...");
    mainLayout->addWidget(logDisplay);

    tabWidget->addTab(logTab, "Log");

    connect(logFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLogFilterChanged);
}

// ========== TAB 3: SIMULATOR ==========
void MainWindow::setupSimulatorTab()
{
    QWidget* simTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(simTab);

    // Message Configuration
    QGroupBox* msgGroup = new QGroupBox("Message Configuration");
    QGridLayout* msgLayout = new QGridLayout();

    msgLayout->addWidget(new QLabel("CAN ID (hex):"), 0, 0);
    canIdEdit = new QLineEdit("0x123");
    msgLayout->addWidget(canIdEdit, 0, 1);

    msgLayout->addWidget(new QLabel("DLC:"), 1, 0);
    dlcSpin = new QSpinBox();
    dlcSpin->setRange(0, 8);
    dlcSpin->setValue(8);
    msgLayout->addWidget(dlcSpin, 1, 1);

    msgLayout->addWidget(new QLabel("Data (hex):"), 2, 0);
    dataEdit = new QLineEdit("01 02 03 04 05 06 07 08");
    msgLayout->addWidget(dataEdit, 2, 1);

    msgGroup->setLayout(msgLayout);
    mainLayout->addWidget(msgGroup);

    // Transmission Controls
    QGroupBox* txGroup = new QGroupBox("Transmission");
    QHBoxLayout* txLayout = new QHBoxLayout();

    sendOnceBtn = new QPushButton("Send Once");
    txLayout->addWidget(sendOnceBtn);

    txLayout->addWidget(new QLabel("Interval (ms):"));
    intervalSpin = new QSpinBox();
    intervalSpin->setRange(10, 10000);
    intervalSpin->setValue(100);
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

    // Connect signals
    connect(sendOnceBtn, &QPushButton::clicked, this, &MainWindow::onSendFrame);
    connect(sendPeriodicBtn, &QPushButton::clicked, this, &MainWindow::onSendPeriodic);
    connect(stopPeriodicBtn, &QPushButton::clicked, this, &MainWindow::onStopPeriodic);
}

// ========== TAB 4: ANALYZER ==========
void MainWindow::setupAnalyzerTab()
{
    QWidget* analyzerTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(analyzerTab);

    // Message table
    messageTable = new QTableView();
    messageTable->setModel(messageModel);
    messageTable->horizontalHeader()->setStretchLastSection(true);
    mainLayout->addWidget(messageTable);

    // Controls
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

    // Connect signals
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClearMessages);
    connect(saveFramesBtn, &QPushButton::clicked, this, &MainWindow::onSaveFrames);
    connect(loadFramesBtn, &QPushButton::clicked, this, &MainWindow::onLoadFrames);
}

// ========== CONNECTION HANDLERS ==========
void MainWindow::onStartServer()
{
    if (server->startServer(serverPortSpin->value())) {
        serverStatusIndicator->setText("● Running");
        serverStatusIndicator->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        startServerBtn->setEnabled(false);
        stopServerBtn->setEnabled(true);
        serverPortSpin->setEnabled(false);

        addLogEvent(QString("Server started on port %1").arg(serverPortSpin->value()), "Server");
    } else {
        serverStatusIndicator->setText("● Error");
        serverStatusIndicator->setStyleSheet("QLabel { color: red; font-weight: bold; }");
        addLogEvent("Failed to start server - port may be in use", "Server");
    }
}

void MainWindow::onStopServer()
{
    server->stopServer();
    serverStatusIndicator->setText("● Stopped");
    serverStatusIndicator->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    startServerBtn->setEnabled(true);
    stopServerBtn->setEnabled(false);
    serverPortSpin->setEnabled(true);

    addLogEvent("Server stopped", "Server");
}

void MainWindow::onConnect()
{
    client->connectToServer(hostEdit->text(), clientPortSpin->value());
}

void MainWindow::onDisconnect()
{
    client->disconnectFromServer();
}

void MainWindow::onClientConnected()
{
    clientStatusIndicator->setText("● Connected");
    clientStatusIndicator->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    connectBtn->setEnabled(false);
    disconnectBtn->setEnabled(true);
    hostEdit->setEnabled(false);
    clientPortSpin->setEnabled(false);

    addLogEvent(QString("Connected to %1:%2").arg(hostEdit->text()).arg(clientPortSpin->value()), "Client");
}

void MainWindow::onClientDisconnected()
{
    clientStatusIndicator->setText("● Disconnected");
    clientStatusIndicator->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    connectBtn->setEnabled(true);
    disconnectBtn->setEnabled(false);
    hostEdit->setEnabled(true);
    clientPortSpin->setEnabled(true);

    addLogEvent("Disconnected from server", "Client");
}

void MainWindow::onServerClientConnected(const QString& address)
{
    addLogEvent(QString("Client connected: %1").arg(address), "Server");
}

// ========== SIMULATOR HANDLERS ==========
void MainWindow::onSendFrame()
{
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFF) {
        addLogEvent("Error: Invalid CAN ID", "Frame");
        return;
    }

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
    server->sendFrame(frame);
}

void MainWindow::onSendPeriodic()
{
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFF) {
        addLogEvent("Error: Invalid CAN ID", "Frame");
        return;
    }

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
    server->addPeriodicFrame(frame, intervalSpin->value());

    sendPeriodicBtn->setEnabled(false);
    stopPeriodicBtn->setEnabled(true);

    addLogEvent(QString("Started periodic transmission (ID: 0x%1, Interval: %2ms)")
                    .arg(id, 0, 16).arg(intervalSpin->value()), "Frame");
}

void MainWindow::onStopPeriodic()
{
    server->clearPeriodicFrames();
    sendPeriodicBtn->setEnabled(true);
    stopPeriodicBtn->setEnabled(false);

    addLogEvent("Stopped periodic transmission", "Frame");
}

// ========== ANALYZER HANDLERS ==========
void MainWindow::onClearMessages()
{
    messageModel->clear();
    messageCountLabel->setText("Messages: 0");
}

void MainWindow::onSaveFrames()
{
    QString filename = QFileDialog::getSaveFileName(this, "Save Frames", "", "CSV Files (*.csv)");
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

    addLogEvent(QString("Saved %1 frames to %2").arg(messageModel->rowCount()).arg(filename), "Frame");
}

void MainWindow::onLoadFrames()
{
    QString filename = QFileDialog::getOpenFileName(this, "Load Frames", "", "CSV Files (*.csv)");
    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    in.readLine(); // Skip header

    int frameCount = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(',', Qt::SkipEmptyParts);

        if (fields.size() < 4) continue;

        bool ok;
        QString idText = fields[1].remove("0x", Qt::CaseInsensitive);
        quint32 id = idText.toUInt(&ok, 16);
        if (!ok || id > 0x1FFFFFFF) continue;

        quint8 dlc = fields[2].toUInt(&ok, 10);
        if (!ok) continue;

        QStringList dataList = fields[3].split(' ', Qt::SkipEmptyParts);
        if (dataList.size() > 8) continue;

        quint8 data[8] = {0};
        for (int i = 0; i < dataList.size(); ++i) {
            quint32 byte = dataList[i].toUInt(&ok, 16);
            if (!ok || byte > 0xFF) continue;
            data[i] = static_cast<quint8>(byte);
        }

        CANFrame frame(id, dlc, data);
        messageModel->addFrame(frame);
        frameCount++;
    }

    addLogEvent(QString("Loaded %1 frames from %2").arg(frameCount).arg(filename), "Frame");
}

// ========== LOG HANDLERS ==========
void MainWindow::onLogFilterChanged(int index)
{
    QStringList filters = {"All", "Server", "Client", "Frame"};
    currentLogFilter = filters[index];
    updateLogDisplay();
}

void MainWindow::addLogEvent(const QString& message, const QString& category)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    entry.category = category;
    entry.message = message;

    logHistory.prepend(entry);

    // Keep last 50 events
    if (logHistory.size() > 50) {
        logHistory.removeLast();
    }

    updateLogDisplay();
}

void MainWindow::updateLogDisplay()
{
    QStringList displayLines;

    for (const LogEntry& entry : logHistory) {
        // Apply filter
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
