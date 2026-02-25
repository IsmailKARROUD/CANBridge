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
#include <QRegularExpression>
#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>

#include "mainwindow.h"
#include "aboutdialog.h"

// Forward declaration of file-scope helper (defined in the Apply helpers section)
static int maxBytesForType(CanType t);

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

    // Log when a remote client connects to our server, and push current settings to it
    connect(server, &TcpServer::clientConnected, this, [this](const QString& address) {
        onServerClientConnected(address);
        // Send current CAN settings to all connected clients (including the new one)
        server->sendSettings(m_canType, m_idFormat);
    });

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

    // Apply CAN settings pushed by the server (silent — no downgrade dialog)
    connect(client, &TcpClient::settingsReceived, this, &MainWindow::applySettingsFromServer);

    // Log frames sent by the client and update the simulator status label
    connect(client, &TcpClient::frameSent, this, [this](const CANFrame& frame) {
        addLogEvent(QString("Frame sent - ID: 0x%1").arg(frame.getId(), 0, 16), "Frame");
        messageModel->addFrame(frame, true);  // TX
        lastFrameStatusLabel->setText(QString("✓ Sent ID: 0x%1").arg(frame.getId(), 0, 16));
        lastFrameStatusLabel->setStyleSheet("QLabel { color: green; }");
    });

    // ==== Model signal connections ====

    // Update the message count label and optionally auto-scroll whenever a new frame is added
    connect(messageModel, &MessageModel::frameAdded, this, [this]() {
        messageCountLabel->setText(QString("Messages: %1").arg(messageModel->rowCount()));
        if (m_autoScroll)
            messageTable->scrollToBottom();
    });
}

MainWindow::~MainWindow()
{
}

// ============================================================================
// Menu Bar Setup + Theme  →  see mainwindow_menubar.cpp
// ============================================================================

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
    serverGroup  = new QGroupBox("Server");
    QVBoxLayout* serverVBox   = new QVBoxLayout(serverGroup);

    // Row 1: port, status, start/stop
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
    startServerBtn->setStyleSheet("QPushButton { color: green; font-weight: bold; }");
    stopServerBtn = new QPushButton("Stop");
    stopServerBtn->setStyleSheet("QPushButton { color: grey; font-weight: bold; }");
    stopServerBtn->setEnabled(false);
    stopServerBtn->setVisible(false);
    serverLayout->addWidget(startServerBtn);
    serverLayout->addWidget(stopServerBtn);
    serverLayout->addStretch();

    serverVBox->addLayout(serverLayout);

    // Row 2: CAN Bus Type and CAN ID Format
    QHBoxLayout* canSettingsLayout = new QHBoxLayout();

    canSettingsLayout->addWidget(new QLabel("CAN Bus Type:"));
    canTypeCombo = new QComboBox();
    canTypeCombo->addItems({"Classic  (8 bytes)", "FD  (64 bytes)", "XL  (2048 bytes)"});
    canTypeCombo->setCurrentIndex(0);
    canTypeCombo->setMaximumWidth(160);
    canSettingsLayout->addWidget(canTypeCombo);

    canSettingsLayout->addSpacing(20);

    canSettingsLayout->addWidget(new QLabel("CAN ID Format:"));
    idFormatCombo = new QComboBox();
    idFormatCombo->addItems({"Standard  (11-bit)", "Extended  (29-bit)"});
    idFormatCombo->setCurrentIndex(0);
    idFormatCombo->setMaximumWidth(160);
    canSettingsLayout->addWidget(idFormatCombo);

    canSettingsLayout->addStretch();
    serverVBox->addLayout(canSettingsLayout);

    mainLayout->addWidget(serverGroup);

    // Wire CAN settings combos — use activated() so programmatic setCurrentIndex doesn't re-trigger
    connect(canTypeCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx) {
        applyCanType(static_cast<CanType>(idx));
    });
    connect(idFormatCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx) {
        applyIdFormat(static_cast<IdFormat>(idx));
    });

    // -- Client section --
    clientGroup = new QGroupBox("Client");
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
    connectBtn->setStyleSheet("QPushButton { color: green; font-weight: bold; }");
    disconnectBtn = new QPushButton("Disconnect");
    disconnectBtn->setStyleSheet("QPushButton { color: grey; font-weight: bold; }");
    disconnectBtn->setEnabled(false);       // Disabled until connected
    disconnectBtn->setVisible(false);       // hide until connected
    clientLayout->addWidget(connectBtn);
    clientLayout->addWidget(disconnectBtn);
    clientLayout->addStretch();

    // Badge labels showing server-pushed bus settings (only visible when connected)
    static const QString badgeBase = "QLabel { color: white; font-weight: bold; border-radius: 4px; padding: 2px 10px; }";
    clientCanTypeLabel  = new QLabel("");
    clientCanTypeLabel->setStyleSheet(badgeBase);
    clientCanTypeLabel->setVisible(false);
    clientIdFormatLabel = new QLabel("");
    clientIdFormatLabel->setStyleSheet(badgeBase);
    clientIdFormatLabel->setVisible(false);

    QHBoxLayout* badgeLayout = new QHBoxLayout();
    badgeLayout->addWidget(clientCanTypeLabel);
    badgeLayout->addWidget(clientIdFormatLabel);
    badgeLayout->addStretch();

    QVBoxLayout* clientVBox = new QVBoxLayout(clientGroup);
    clientVBox->addLayout(clientLayout);
    clientVBox->addLayout(badgeLayout);

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

    // -- Bottom bar: Save Log button --
    QHBoxLayout* logControlLayout = new QHBoxLayout();
    saveLogBtn = new QPushButton("Save Log");
    saveLogBtn->setMaximumWidth(120);
    logControlLayout->addWidget(saveLogBtn);
    logControlLayout->addStretch();
    mainLayout->addLayout(logControlLayout);

    tabWidget->addTab(logTab, "Log");

    connect(logFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLogFilterChanged);
    connect(saveLogBtn, &QPushButton::clicked, this, &MainWindow::onSaveLog);
}

// ============================================================================
// Tab 3: Simulator
// ============================================================================

/**
 * Build the Simulator tab with multi-frame support:
 *  - Add Frame button to create new frame widgets
 *  - Scrollable area containing individual frame widgets
 *  - Global controls: Send All, Stop All
 */
void MainWindow::setupSimulatorTab()
{
    QWidget* simTab = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(simTab);

    // -- Top bar: Add Frame + Show Hidden buttons --
    QHBoxLayout* topLayout = new QHBoxLayout();

    addFrameBtn = new QPushButton("+ Add Frame");
    addFrameBtn->setMaximumWidth(150);
    addFrameBtn->setStyleSheet("QPushButton { color: green; font-weight: bold; }");
    topLayout->addWidget(addFrameBtn);

    // "Show Hidden" button — only visible when at least one frame is hidden.
    // Label includes the hidden count so the user knows how many to restore.
    showHiddenBtn = new QPushButton();
    showHiddenBtn->setMaximumWidth(180);
    showHiddenBtn->setVisible(false);  // Hidden until a frame is hidden
    topLayout->addWidget(showHiddenBtn);

    topLayout->addStretch();
    mainLayout->addLayout(topLayout);

    // -- Scrollable area for frame widgets --
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget* scrollWidget = new QWidget();
    framesLayout = new QVBoxLayout(scrollWidget);
    framesLayout->setSpacing(10);
    framesLayout->addStretch();  // Push frames to top

    scrollArea->setWidget(scrollWidget);
    scrollArea->setMinimumHeight(300);
    mainLayout->addWidget(scrollArea);

    // -- Global controls at bottom --
    QHBoxLayout* globalLayout = new QHBoxLayout();
    sendAllBtn = new QPushButton("▶︎ Send All");
    stopAllBtn = new QPushButton("⏹ Stop All");
    sendAllBtn->setMaximumWidth(150);
    sendAllBtn->setStyleSheet("QPushButton { color: green; font-weight: bold; }");
    stopAllBtn->setMaximumWidth(150);
    stopAllBtn->setStyleSheet("QPushButton { color: grey; font-weight: bold; }");

    globalLayout->addWidget(sendAllBtn);
    globalLayout->addWidget(stopAllBtn);
    globalLayout->addStretch();
    mainLayout->addLayout(globalLayout);

    // -- Status label: shows result of the most recent send operation --
    // Updated by the server/client frameSent signal callbacks in the constructor.
    lastFrameStatusLabel = new QLabel("");
    lastFrameStatusLabel->setStyleSheet("QLabel { color: gray; }");
    mainLayout->addWidget(lastFrameStatusLabel);

    tabWidget->addTab(simTab, "Simulator");

    // Add first frame by default
    addFrameWidget(0x100);

    // -- Wire global button signals --
    connect(addFrameBtn, &QPushButton::clicked, this, [this]() {
        // Find next available ID (respects current ID format)
        uint32_t maxId = (m_idFormat == IdFormat::Standard) ? 0x7FF : 0x1FFFFFFF;
        uint32_t nextId = 0x100;
        while (frameWidgets.contains(nextId)) {
            nextId++;
            if (nextId > maxId) nextId = 0x100;
        }
        addFrameWidget(nextId);
    });

    connect(sendAllBtn, &QPushButton::clicked, this, [this]() {
        for (auto* widget : std::as_const(frameWidgets)) {
            if(widget->getSendButton()->isEnabled() && widget->getSendButton()->isVisible()) widget->getSendButton()->click();
        }
        addLogEvent(QString("Sent all frames (%1 frames)").arg(frameWidgets.size()), "Frame");
    });

    connect(stopAllBtn, &QPushButton::clicked, this, [this]() {
        for (auto* widget : std::as_const(frameWidgets)) {
            if(widget->getStopButton()->isEnabled() && widget->getStopButton()->isVisible()) widget->getStopButton()->click();
        }
        addLogEvent("Stopped all periodic transmissions", "Frame");
    });

    // -- Show Hidden: open a dialog listing all hidden frames for selective restore --
    connect(showHiddenBtn, &QPushButton::clicked, this, [this]() {
        // Collect all hidden widgets into a list keyed by CAN ID
        QList<QPair<uint32_t, FrameWidget*>> hiddenList;
        for (auto it = frameWidgets.begin(); it != frameWidgets.end(); ++it) {
            if (!it.value()->isVisible()) {
                hiddenList.append({it.key(), it.value()});
            }
        }

        if (hiddenList.isEmpty()) return;  // Nothing to show (shouldn't happen)

        // Build the restore dialog
        QDialog dialog(this);
        dialog.setWindowTitle("Restore Hidden Frames");
        dialog.setMinimumWidth(280);

        QVBoxLayout* layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel("Select frames to restore:"));

        // One checkbox row per hidden frame, labelled with the CAN ID
        QListWidget* list = new QListWidget(&dialog);
        for (const auto& pair : hiddenList) {
            QListWidgetItem* item = new QListWidgetItem(
                QString("Frame 0x%1").arg(pair.first, 3, 16, QChar('0')).toUpper(),
                list);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);  // Pre-select all by default
        }
        layout->addWidget(list);

        // Restore Selected / Cancel buttons
        QDialogButtonBox* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        buttons->button(QDialogButtonBox::Ok)->setText("Restore Selected");
        layout->addWidget(buttons);

        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) return;

        // Restore only the checked frames
        int restoredCount = 0;
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->checkState() == Qt::Checked) {
                hiddenList[i].second->setVisible(true);
                restoredCount++;
            }
        }

        // Refresh the hidden-frame button count after restoring
        updateHiddenFramesButton();

        addLogEvent(QString("Restored %1 hidden frame(s)").arg(restoredCount), "Frame");
    });
}

// ============================================================================
// Multi-Frame Simulator Helpers
// ============================================================================

/**
 * Add a new FrameWidget to the simulator with the specified default CAN ID.
 * Automatically finds a unique ID if the default is already taken.
 */
void MainWindow::addFrameWidget(uint32_t defaultId)
{
    // Find unique ID if default is taken
    uint32_t maxId = (m_idFormat == IdFormat::Standard) ? 0x7FF : 0x1FFFFFFF;
    uint32_t canId = defaultId;
    while (frameWidgets.contains(canId)) {
        canId++;
        if (canId > maxId) canId = 0x100;  // Wrap around
    }

    FrameWidget* widget = new FrameWidget(canId);

    // Inject a live connection checker so the widget can verify that a
    // server is listening or a client is connected at the moment Send is
    // clicked — without coupling FrameWidget to TcpServer or TcpClient.
    widget->setConnectionChecker([this]() {
        return server->isListening() || client->isConnected();
    });

    // Set the interval spinner range and default value entirely from settings —
    // no hardcoded range lives in the FrameWidget constructor.
    widget->setMinInterval(m_timerResolutionMs);
    widget->setMaxInterval(m_maxIntervalMs);
    widget->setIntervalValue(qMax(100, m_timerResolutionMs));

    // Apply current global CAN type and ID format
    widget->setCanType(m_canType);
    widget->setIdFormat(m_idFormat);

    // Insert before the stretch spacer
    framesLayout->insertWidget(framesLayout->count() - 1, widget);
    frameWidgets[canId] = widget;

    // Connect frame widget signals
    connect(widget, &FrameWidget::sendOnceClicked, this, &MainWindow::onFrameSendOnce);
    connect(widget, &FrameWidget::onSendFramePeriodicClicked, this, &MainWindow::onSendFramePeriodic);
    connect(widget, &FrameWidget::onStopFramePeriodicClicked, this, &MainWindow::onStopFramePeriodic);
    connect(widget, &FrameWidget::removeClicked, this, &MainWindow::onFrameRemove);


    connect(widget, &FrameWidget::hideClicked, this, [this, widget]() {
        widget->setVisible(false);
        // Refresh the "Show Hidden (N)" button in the top bar
        updateHiddenFramesButton();
    });

    connect(widget, &FrameWidget::canIdChanged, this,
            [this](uint32_t oldId, uint32_t newId) {
                // Check for duplicate
                if (isCanIdDuplicate(newId, frameWidgets[oldId])) {
                    QMessageBox::warning(this, "Duplicate CAN ID",
                                         QString("CAN ID 0x%1 is already in use!\n\nPlease choose a different ID.")
                                             .arg(newId, 0, 16));

                    // Revert the change in the widget
                    auto widget = frameWidgets[oldId];
                    CANFrame frame = widget->getFrame();
                    frame.setID(oldId);   // Restore old ID
                    widget->setFrame(frame);
                    return;
                }

                // Update map
                auto widget = frameWidgets[oldId];
                frameWidgets.remove(oldId);
                frameWidgets[newId] = widget;

                addLogEvent(QString("Frame ID changed: 0x%1 → 0x%2")
                                .arg(oldId, 0, 16).arg(newId, 0, 16), "Frame");
            });

    addLogEvent(QString("Added frame with ID: 0x%1").arg(canId, 0, 16), "Frame");
}

/**
 * Recount hidden frame widgets and update the "Show Hidden" button.
 *
 * Iterates the full frameWidgets map and counts entries where isVisible()
 * returns false. The button is shown with a live count when hidden frames
 * exist, and hidden entirely when all frames are visible — so it never
 * takes up space unnecessarily.
 */
void MainWindow::updateHiddenFramesButton()
{
    int hiddenCount = 0;
    for (auto* widget : std::as_const(frameWidgets)) {
        if (!widget->isVisible()) {
            hiddenCount++;
        }
    }

    if (hiddenCount > 0) {
        showHiddenBtn->setText(QString("- Show Hidden (%1)").arg(hiddenCount));
        showHiddenBtn->setVisible(true);
    } else {
        // No hidden frames — remove the button from view entirely
        showHiddenBtn->setVisible(false);
    }
}

/**
 * Check if a CAN ID is already in use by another frame widget.
 */
bool MainWindow::isCanIdDuplicate(uint32_t canId, FrameWidget* excludeWidget)
{
    for (auto it = frameWidgets.begin(); it != frameWidgets.end(); ++it) {
        if (it.value() != excludeWidget && it.key() == canId) {
            return true;
        }
    }
    return false;
}

/**
 * Send a single frame via server (if running) or client (if connected).
 */
void MainWindow::onFrameSendOnce(const CANFrame& frame)
{
    if (!server->isListening() && !client->isConnected()) {
        addLogEvent("Error: Not connected", "Frame");
        return;
    }

    int     maxBytes = maxBytesForType(m_canType);
    quint32 maxId    = (m_idFormat == IdFormat::Standard) ? CANFrame::ID_MAX_STANDARD
                                                          : CANFrame::ID_MAX_EXTENDED;
    if (frame.getData().size() > static_cast<qsizetype>(maxBytes) || frame.getId() > maxId) {
        addLogEvent(QString("Error: Frame 0x%1 exceeds current bus limits — not sent")
                        .arg(frame.getId(), 0, 16), "Frame");
        return;
    }

    if (server->isListening()) {
        server->sendFrame(frame);
    } else if (client->isConnected()) {
        client->sendFrame(frame);
    }
}

/**
 * Handle periodic transmission state changes for a specific frame.
 */
void MainWindow::onSendFramePeriodic(const CANFrame& frame, int intervalMs)
{


        // Start periodic transmission
        int     maxBytes = maxBytesForType(m_canType);
        quint32 maxId    = (m_idFormat == IdFormat::Standard) ? CANFrame::ID_MAX_STANDARD
                                                              : CANFrame::ID_MAX_EXTENDED;
        if (frame.getData().size() > static_cast<qsizetype>(maxBytes) || frame.getId() > maxId) {
            addLogEvent(QString("Error: Frame 0x%1 exceeds current bus limits — periodic not started")
                            .arg(frame.getId(), 0, 16), "Frame");
            return;
        }

        if (server->isListening()) {
            server->addPeriodicFrame(frame, intervalMs);
            addLogEvent(QString("Started periodic: ID 0x%1, %2ms")
                            .arg(frame.getId(), 0, 16).arg(intervalMs), "Frame");
        } else if (client->isConnected()) {
            client->addPeriodicFrame(frame, intervalMs);
            addLogEvent(QString("Started periodic: ID 0x%1, %2ms")
                            .arg(frame.getId(), 0, 16).arg(intervalMs), "Frame");
        }
}

void MainWindow::onStopFramePeriodic(const int& Id) {
        // Stop periodic transmission
        if (server->isListening()) {
            server->removePeriodicFrame(Id);
        } else if (client->isConnected()) {
            client->removePeriodicFrame(Id);
        }
        addLogEvent(QString("Stopped periodic: ID 0x%1").arg(Id, 0, 16), "Frame");
    }

/**
 * Remove a frame widget from the simulator.
 */
void MainWindow::onFrameRemove(uint32_t canId)
{
    auto it = frameWidgets.find(canId);
    if (it != frameWidgets.end()) {
        // Stop periodic if active
        if ((*it)->isPeriodicEnabled()) {
            server->removePeriodicFrame(canId);
            client->removePeriodicFrame(canId);
        }

        (*it)->deleteLater();
        frameWidgets.erase(it);

        // A hidden frame may have been removed — keep the button count accurate
        updateHiddenFramesButton();

        addLogEvent(QString("Removed frame ID: 0x%1").arg(canId, 0, 16), "Frame");
    }
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

    // Auto-format as user types
    connect(dataFilterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        // Block signals to prevent infinite loop
        dataFilterEdit->blockSignals(true);

        int cursorPos = dataFilterEdit->cursorPosition();
        QString formatted = formatHexWithSpaces(text);

        // Adjust cursor position for added spaces
        int spacesBefore = text.first(cursorPos).count(' ');

        // Calculate adjusted cursor position
        int adjustedPos = qMin(cursorPos + (formatted.count(' ') - spacesBefore), formatted.length());
        int spacesAfter = formatted.first(adjustedPos).count(' ');

        dataFilterEdit->setText(formatted);
        dataFilterEdit->setCursorPosition(cursorPos + (spacesAfter - spacesBefore));

        dataFilterEdit->blockSignals(false);

        // Now trigger the filter
        filterProxyModel->setDataFilter(formatted);
    });
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
        serverStatusIndicator->setText("▶︎ Running");
        serverStatusIndicator->setStyleSheet("QLabel { color: green; font-weight: bold; }");
        startServerBtn->setEnabled(false);
        startServerBtn->setVisible(false);
        stopServerBtn->setEnabled(true);
        stopServerBtn->setVisible(true);
        serverPortSpin->setEnabled(false);  // Lock port while server is running
        canTypeCombo->setEnabled(false);
        idFormatCombo->setEnabled(false);
        clientGroup->setEnabled(false);     // Disable client while server is running

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
    startServerBtn->setVisible(true);
    stopServerBtn->setEnabled(false);
    stopServerBtn->setVisible(false);
    serverPortSpin->setEnabled(true);  // Unlock port for reconfiguration
    canTypeCombo->setEnabled(true);
    idFormatCombo->setEnabled(true);
    clientGroup->setEnabled(true);     // Re-enable client when server stops

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
    clientStatusIndicator->setText("▶︎ Connected");
    clientStatusIndicator->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    connectBtn->setEnabled(false);
    connectBtn->setVisible(false);
    disconnectBtn->setEnabled(true);
    disconnectBtn->setVisible(true);
    hostEdit->setEnabled(false);        // Lock inputs while connected
    clientPortSpin->setEnabled(false);
    serverGroup->setEnabled(false);     // Disable server while client is connected

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
    connectBtn->setVisible(true);
    disconnectBtn->setEnabled(false);
    disconnectBtn->setVisible(false);
    hostEdit->setEnabled(true);         // Unlock inputs for reconfiguration
    clientPortSpin->setEnabled(true);
    serverGroup->setEnabled(true);      // Re-enable server when client disconnects

    clientCanTypeLabel->setVisible(false);
    clientIdFormatLabel->setVisible(false);
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
    out << "Timestamp,Dir,ID,DLC,Data,FrameType,IDFormat\n";  // 7-column CSV header

    // Write each frame as a CSV row
    for (int i = 0; i < messageModel->rowCount(); ++i) {
        for (int j = 0; j < 5; ++j) {
            out << messageModel->data(messageModel->index(i, j)).toString();
            out << ",";
        }
        const CANFrame& f = messageModel->frameAt(i);
        // FrameType column
        switch (f.getFrameType()) {
        case CanType::Classic: out << "Classic"; break;
        case CanType::FD:      out << "FD";      break;
        case CanType::XL:      out << "XL";      break;
        }
        out << ",";
        // IDFormat column
        out << (f.getIdFormat() == IdFormat::Standard ? "Standard" : "Extended");
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
        QStringList fields = line.split(',');

        if (fields.size() < 5) continue;  // Skip malformed rows

        // Parse CAN ID
        bool ok;
        QString idText = fields[2].trimmed().remove("0x", Qt::CaseInsensitive);
        quint32 id = idText.toUInt(&ok, 16);
        if (!ok || id > 0x1FFFFFFF) continue;

        // Parse DLC
        quint16 dlc = static_cast<quint16>(fields[3].trimmed().toUInt(&ok, 10));
        if (!ok) continue;

        // Parse data bytes
        QStringList dataList = fields[4].trimmed().split(' ', Qt::SkipEmptyParts);
        QByteArray data;
        bool dataOk = true;
        for (const QString& hexByte : std::as_const(dataList)) {
            quint32 byte = hexByte.toUInt(&ok, 16);
            if (!ok || byte > 0xFF) { dataOk = false; break; }
            data.append(static_cast<char>(byte));
        }
        if (!dataOk) continue;

        // Parse FrameType and IDFormat (new 7-column format; default to Classic/Standard for old files)
        CanType  frameType = CanType::Classic;
        IdFormat idFmt     = IdFormat::Standard;
        if (fields.size() >= 7) {
            QString ft = fields[5].trimmed();
            if (ft == "FD")      frameType = CanType::FD;
            else if (ft == "XL") frameType = CanType::XL;

            QString idf = fields[6].trimmed();
            if (idf == "Extended") idFmt = IdFormat::Extended;
        }

        CANFrame frame(id, dlc, data, frameType, idFmt);
        messageModel->addFrame(frame, fields[1].trimmed() == "TX");
        frameCount++;
    }

    addLogEvent(QString("Loaded %1 frames from %2").arg(frameCount).arg(filename), "Frame");
}

// ============================================================================
// CAN Type / ID Format — Apply helpers
// ============================================================================

bool MainWindow::isDowngrade(CanType from, CanType to) const
{
    return static_cast<int>(to) < static_cast<int>(from);
}

bool MainWindow::isDowngrade(IdFormat from, IdFormat to) const
{
    return static_cast<int>(to) < static_cast<int>(from);
}

/**
 * Returns the maximum data payload in bytes for a given CanType.
 */
static int maxBytesForType(CanType t)
{
    switch (t) {
    case CanType::Classic: return 8;
    case CanType::FD:      return 64;
    case CanType::XL:      return 2048;
    }
    return 8;
}

bool MainWindow::analyzerHasConflicts(CanType newType, IdFormat newFmt) const
{
    int      maxBytes = maxBytesForType(newType);
    quint32  maxId    = (newFmt == IdFormat::Standard) ? CANFrame::ID_MAX_STANDARD
                                                        : CANFrame::ID_MAX_EXTENDED;
    for (int i = 0; i < messageModel->rowCount(); ++i) {
        const CANFrame& f = messageModel->frameAt(i);
        if (f.getData().size() > maxBytes || f.getId() > maxId)
            return true;
    }
    return false;
}

bool MainWindow::simulatorHasConflicts(CanType newType, IdFormat newFmt) const
{
    int      maxBytes = maxBytesForType(newType);
    quint32  maxId    = (newFmt == IdFormat::Standard) ? CANFrame::ID_MAX_STANDARD
                                                        : CANFrame::ID_MAX_EXTENDED;
    for (auto* widget : std::as_const(frameWidgets)) {
        CANFrame f = widget->getFrame();
        if (f.getData().size() > maxBytes || f.getId() > maxId)
            return true;
    }
    return false;
}

/**
 * Propagate a new CanType globally:
 *  - If upgrading: apply silently.
 *  - If downgrading with conflicts: show Clear All / Cancel dialog.
 *  - If Cancel: restore the previous combo index and return.
 */
void MainWindow::applyCanType(CanType newType)
{
    if (isDowngrade(m_canType, newType)) {
        bool analyzerConflict  = analyzerHasConflicts(newType, m_idFormat);
        bool simulatorConflict = simulatorHasConflicts(newType, m_idFormat);

        if (analyzerConflict || simulatorConflict) {
            int  maxBytes = maxBytesForType(newType);
            QString typeName = (newType == CanType::Classic) ? "CAN Classic"
                             : (newType == CanType::FD)      ? "CAN-FD"
                             :                                 "CAN-XL";
            QString fromName = (m_canType == CanType::Classic) ? "CAN Classic"
                             : (m_canType == CanType::FD)      ? "CAN-FD"
                             :                                   "CAN-XL";

            // Count conflicts for the warning message
            int analyzerCount = 0, simulatorCount = 0;
            quint32 maxId = (m_idFormat == IdFormat::Standard) ? CANFrame::ID_MAX_STANDARD
                                                                : CANFrame::ID_MAX_EXTENDED;
            for (int i = 0; i < messageModel->rowCount(); ++i) {
                const CANFrame& f = messageModel->frameAt(i);
                if (f.getData().size() > maxBytes || f.getId() > maxId) analyzerCount++;
            }
            for (auto* w : std::as_const(frameWidgets)) {
                CANFrame f = w->getFrame();
                if (f.getData().size() > maxBytes || f.getId() > maxId) simulatorCount++;
            }

            QString msg = QString("Downgrading from %1 to %2 will affect:\n").arg(fromName, typeName);
            if (analyzerCount)  msg += QString("  • %1 Analyzer frame(s) with data > %2 bytes\n").arg(analyzerCount).arg(maxBytes);
            if (simulatorCount) msg += QString("  • %1 Simulator frame(s) with data > %2 bytes\n").arg(simulatorCount).arg(maxBytes);
            msg += "\nChoose how to handle conflicting frames:";

            QMessageBox dlg(this);
            dlg.setWindowTitle("CAN Bus Type Downgrade");
            dlg.setText(msg);
            QPushButton* clearAllBtn  = dlg.addButton("Clear All", QMessageBox::DestructiveRole);
            QPushButton* keepBtn      = dlg.addButton("Keep",      QMessageBox::AcceptRole);
            QPushButton* cancelBtn    = dlg.addButton("Cancel",    QMessageBox::RejectRole);
            Q_UNUSED(keepBtn);
            dlg.setDefaultButton(cancelBtn);
            dlg.exec();

            QAbstractButton* clicked = dlg.clickedButton();
            if (clicked == cancelBtn) {
                // Restore combo to reflect the unchanged setting
                canTypeCombo->setCurrentIndex(static_cast<int>(m_canType));
                return;
            }

            if (clicked == clearAllBtn) {
                messageModel->clear();
                QList<uint32_t> toRemove;
                for (auto it = frameWidgets.cbegin(); it != frameWidgets.cend(); ++it) {
                    CANFrame f = it.value()->getFrame();
                    if (f.getData().size() > static_cast<qsizetype>(maxBytes) || f.getId() > maxId)
                        toRemove.append(it.key());
                }
                for (uint32_t id : toRemove)
                    onFrameRemove(id);
            }
            // "Keep": conflicting frames are retained; send-time validation will reject them
        }
    }

    // Apply to all widgets
    m_canType = newType;
    canTypeCombo->setCurrentIndex(static_cast<int>(newType));
    for (auto* w : std::as_const(frameWidgets))
        w->setCanType(newType);

    // Push new settings to all connected clients
    if (server->isListening())
        server->sendSettings(m_canType, m_idFormat);

    addLogEvent(QString("CAN bus type changed to: %1")
                    .arg(newType == CanType::Classic ? "Classic"
                       : newType == CanType::FD      ? "FD"
                       :                               "XL"), "Server");
}

/**
 * Propagate a new IdFormat globally, showing a downgrade dialog if needed.
 */
void MainWindow::applyIdFormat(IdFormat newFmt)
{
    if (isDowngrade(m_idFormat, newFmt)) {
        bool analyzerConflict  = analyzerHasConflicts(m_canType, newFmt);
        bool simulatorConflict = simulatorHasConflicts(m_canType, newFmt);

        if (analyzerConflict || simulatorConflict) {
            quint32 maxId    = (newFmt == IdFormat::Standard) ? CANFrame::ID_MAX_STANDARD
                                                               : CANFrame::ID_MAX_EXTENDED;
            int     maxBytes = maxBytesForType(m_canType);
            QString fmtName  = (newFmt == IdFormat::Standard) ? "Standard (11-bit)" : "Extended (29-bit)";

            int analyzerCount = 0, simulatorCount = 0;
            for (int i = 0; i < messageModel->rowCount(); ++i) {
                if (messageModel->frameAt(i).getId() > maxId) analyzerCount++;
            }
            for (auto* w : std::as_const(frameWidgets)) {
                if (w->getFrame().getId() > maxId) simulatorCount++;
            }

            QString msg = QString("Downgrading to %1 will affect:\n").arg(fmtName);
            if (analyzerCount)  msg += QString("  • %1 Analyzer frame(s) with ID > 0x%2\n").arg(analyzerCount).arg(maxId, 0, 16);
            if (simulatorCount) msg += QString("  • %1 Simulator frame(s) with ID > 0x%2\n").arg(simulatorCount).arg(maxId, 0, 16);
            msg += "\nChoose how to handle conflicting frames:";

            QMessageBox dlg(this);
            dlg.setWindowTitle("CAN ID Format Downgrade");
            dlg.setText(msg);
            QPushButton* clearAllBtn = dlg.addButton("Clear All", QMessageBox::DestructiveRole);
            QPushButton* keepBtn     = dlg.addButton("Keep",      QMessageBox::AcceptRole);
            QPushButton* cancelBtn   = dlg.addButton("Cancel",    QMessageBox::RejectRole);
            Q_UNUSED(keepBtn);
            dlg.setDefaultButton(cancelBtn);
            dlg.exec();

            QAbstractButton* clicked = dlg.clickedButton();
            if (clicked == cancelBtn) {
                idFormatCombo->setCurrentIndex(static_cast<int>(m_idFormat));
                return;
            }

            if (clicked == clearAllBtn) {
                messageModel->clear();
                QList<uint32_t> toRemove;
                for (auto it = frameWidgets.cbegin(); it != frameWidgets.cend(); ++it) {
                    if (it.value()->getFrame().getId() > maxId)
                        toRemove.append(it.key());
                }
                for (uint32_t id : toRemove)
                    onFrameRemove(id);
            }
            // "Keep": conflicting frames are retained; send-time validation will reject them
        }
    }

    m_idFormat = newFmt;
    idFormatCombo->setCurrentIndex(static_cast<int>(newFmt));
    for (auto* w : std::as_const(frameWidgets))
        w->setIdFormat(newFmt);

    // Push new settings to all connected clients
    if (server->isListening())
        server->sendSettings(m_canType, m_idFormat);

    addLogEvent(QString("CAN ID format changed to: %1")
                    .arg(newFmt == IdFormat::Standard ? "Standard (11-bit)" : "Extended (29-bit)"), "Server");
}

/**
 * Apply CAN settings received from a remote server.
 * If the server settings are a downgrade and there are conflicting local frames,
 * prompt the user to clear them or keep them. Settings are always applied.
 */
void MainWindow::applySettingsFromServer(CanType type, IdFormat fmt)
{
    // Check for conflicts before updating state (old values still valid here)
    if (isDowngrade(m_canType, type) || isDowngrade(m_idFormat, fmt)) {
        bool analyzerConflict  = analyzerHasConflicts(type, fmt);
        bool simulatorConflict = simulatorHasConflicts(type, fmt);

        if (analyzerConflict || simulatorConflict) {
            int     maxBytes = maxBytesForType(type);
            quint32 maxId    = (fmt == IdFormat::Standard) ? CANFrame::ID_MAX_STANDARD
                                                           : CANFrame::ID_MAX_EXTENDED;
            QString typeName = (type == CanType::Classic) ? "CAN Classic"
                             : (type == CanType::FD)      ? "CAN-FD"
                             :                              "CAN-XL";
            QString fmtName  = (fmt == IdFormat::Standard) ? "Standard (11-bit)" : "Extended (29-bit)";

            int analyzerCount = 0, simulatorCount = 0;
            for (int i = 0; i < messageModel->rowCount(); ++i) {
                const CANFrame& f = messageModel->frameAt(i);
                if (f.getData().size() > static_cast<qsizetype>(maxBytes) || f.getId() > maxId)
                    analyzerCount++;
            }
            for (auto it = frameWidgets.cbegin(); it != frameWidgets.cend(); ++it) {
                CANFrame f = it.value()->getFrame();
                if (f.getData().size() > static_cast<qsizetype>(maxBytes) || f.getId() > maxId)
                    simulatorCount++;
            }

            QString msg = QString("The server uses %1 / %2, which is lower than your current setup.\n").arg(typeName, fmtName);
            if (analyzerCount)  msg += QString("  • %1 Analyzer frame(s) conflict with the new limits\n").arg(analyzerCount);
            if (simulatorCount) msg += QString("  • %1 Simulator frame(s) conflict with the new limits\n").arg(simulatorCount);
            msg += "\nServer settings will be applied regardless.";

            QMessageBox dlg(this);
            dlg.setWindowTitle("Server Settings Downgrade");
            dlg.setText(msg);
            QPushButton* clearAllBtn = dlg.addButton("Clear All", QMessageBox::DestructiveRole);
            QPushButton* keepBtn     = dlg.addButton("Keep",      QMessageBox::RejectRole);
            dlg.setDefaultButton(keepBtn);
            dlg.exec();

            if (dlg.clickedButton() == clearAllBtn) {
                messageModel->clear();
                QList<uint32_t> toRemove;
                for (auto it = frameWidgets.cbegin(); it != frameWidgets.cend(); ++it) {
                    CANFrame f = it.value()->getFrame();
                    if (f.getData().size() > static_cast<qsizetype>(maxBytes) || f.getId() > maxId)
                        toRemove.append(it.key());
                }
                for (uint32_t id : toRemove)
                    onFrameRemove(id);
            }
        }
    }

    m_canType  = type;
    m_idFormat = fmt;

    // Sync the server-side combos so the display is consistent
    canTypeCombo->setCurrentIndex(static_cast<int>(type));
    idFormatCombo->setCurrentIndex(static_cast<int>(fmt));

    // Update all simulator widgets
    for (auto* w : std::as_const(frameWidgets)) {
        w->setCanType(type);
        w->setIdFormat(fmt);
    }

    // Show received settings as colored badges in the client group
    QString typeText, typeColor;
    if      (type == CanType::Classic) { typeText = "CAN Classic"; typeColor = "#2196F3"; }
    else if (type == CanType::FD)      { typeText = "CAN-FD";      typeColor = "#FF9800"; }
    else                               { typeText = "CAN-XL";      typeColor = "#9C27B0"; }

    QString fmtText, fmtColor;
    if (fmt == IdFormat::Standard) { fmtText = "Standard (11-bit)"; fmtColor = "#607D8B"; }
    else                           { fmtText = "Extended (29-bit)"; fmtColor = "#009688"; }

    auto badgeStyle = [](const QString& color) {
        return QString("QLabel { color: white; font-weight: bold; border-radius: 4px;"
                       " padding: 2px 10px; background-color: %1; }").arg(color);
    };

    clientCanTypeLabel->setText(typeText);
    clientCanTypeLabel->setStyleSheet(badgeStyle(typeColor));
    clientCanTypeLabel->setVisible(true);

    clientIdFormatLabel->setText(fmtText);
    clientIdFormatLabel->setStyleSheet(badgeStyle(fmtColor));
    clientIdFormatLabel->setVisible(true);

    QString typeName = typeText;
    QString fmtName  = fmtText;

    addLogEvent(QString("Received server settings: CAN %1 / %2").arg(typeName, fmtName), "Client");
}

// ============================================================================
// Log Management
// ============================================================================

/**
 * Export the complete log history to a plain-text file.
 *
 * Saves ALL log entries regardless of the active category filter, so
 * the output file is always a full record of the session. Each line
 * follows the same format used in the display:
 *   [HH:MM:SS] [Category] Message
 *
 * The file is written newest-first (same order as logHistory).
 */
void MainWindow::onSaveLog()
{
    if (logHistory.isEmpty()) {
        QMessageBox::information(this, "Save Log", "No log entries to save.");
        return;
    }

    QString filename = QFileDialog::getSaveFileName(
        this, "Save Log", "", "Text Files (*.txt);;All Files (*)");
    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save Log",
                             "Could not open file for writing:\n" + filename);
        return;
    }

    QTextStream out(&file);

    // Write a session header for context
    out << "CANBridge Session Log\n";
    out << "Exported: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n";
    out << QString(50, '-') << "\n";

    // Write every entry in logHistory (full history, not filtered)
    for (const LogEntry& entry : logHistory) {
        out << QString("[%1] [%2] %3\n")
                   .arg(entry.timestamp)
                   .arg(entry.category)
                   .arg(entry.message);
    }

    addLogEvent(QString("Log saved to %1").arg(filename), "Server");
}

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

    // Cap log history at MAXLOGHISTORY entries
    while (logHistory.size() > MAXLOGHISTORY) {
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

// Auto-format hex input with spaces every 2 characters
QString MainWindow::formatHexWithSpaces(const QString& input)
{
    // Remove all spaces and non-hex characters
    QString cleaned = input.toUpper();
    cleaned.remove(QRegularExpression("[^0-9A-F]"));

    // Add space after every 2 characters
    QString formatted;
    for (int i = 0; i < cleaned.length(); i++) {
        if (i > 0 && i % 2 == 0) {
            formatted += ' ';
        }
        formatted += cleaned[i];
    }

    return formatted;
}
