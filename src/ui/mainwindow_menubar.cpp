/**
 * @file mainwindow_menubar.cpp
 * @brief Menu bar construction and theme application for MainWindow.
 *
 * Contains:
 *  - MainWindow::setupMenuBar()  — builds the full menu bar (CANBridge, View, Settings)
 *  - QGuiApplication::styleHints()   — applies dark/light stylesheet to the application
 *
 * This file is a split of the MainWindow class implementation.
 * The class definition and all other slots live in mainwindow.h / mainwindow.cpp.
 */

#include "mainwindow.h"
#include "aboutdialog.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QGuiApplication>
#include <QStyleHints>

// ============================================================================
// Menu Bar Setup
// ============================================================================

/**
 * Create the application menu bar with:
 *  - CANBridge menu : About dialog and Quit action
 *  - View menu      : Theme selection, Timestamp format
 *  - Settings menu  : Runtime parameters (frame cap, log cap, timer resolution,
 *                     max periodic interval, auto-scroll)
 */
void MainWindow::setupMenuBar()
{
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // -------------------------------------------------------------------------
    // CANBridge application menu
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // File menu — project save / load
    // -------------------------------------------------------------------------
    QMenu* fileMenu = menuBar->addMenu("File");

    QAction* newProjectAction = fileMenu->addAction("New Project");
    newProjectAction->setShortcut(QKeySequence::New);
    connect(newProjectAction, &QAction::triggered, this, &MainWindow::onNewProject);

    QAction* openProjectAction = fileMenu->addAction("Open Project...");
    openProjectAction->setShortcut(QKeySequence::Open);
    connect(openProjectAction, &QAction::triggered, this, &MainWindow::onOpenProject);

    fileMenu->addSeparator();

    QAction* saveProjectAction = fileMenu->addAction("Save Project");
    saveProjectAction->setShortcut(QKeySequence::Save);
    connect(saveProjectAction, &QAction::triggered, this, &MainWindow::onSaveProject);

    QAction* saveProjectAsAction = fileMenu->addAction("Save Project As...");
    saveProjectAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveProjectAsAction, &QAction::triggered, this, &MainWindow::onSaveProjectAs);

    // -------------------------------------------------------------------------
    // View menu — theme and timestamp format
    // -------------------------------------------------------------------------
    QMenu* viewMenu = menuBar->addMenu("View");

    // Theme sub-menu
    QMenu* themeMenu = viewMenu->addMenu("Theme");
    QActionGroup* themeGroup = new QActionGroup(this);

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

    connect(autoThemeAction,  &QAction::triggered, this, [this]() {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
    });
    connect(lightThemeAction, &QAction::triggered, this, [this]() {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    });
    connect(darkModeAction,   &QAction::triggered, this, [this]() {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    });

    // Timestamp format sub-menu
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

    // -------------------------------------------------------------------------
    // Settings menu — all runtime-tunable parameters
    // -------------------------------------------------------------------------
    QMenu* settingsMenu = menuBar->addMenu("Settings");

    // --- Max Analyzer Frames ---
    QMenu* maxFramesMenu = settingsMenu->addMenu("Max Analyzer Frames");
    QActionGroup* maxFramesGroup = new QActionGroup(this);
    auto addFrameOption = [&](const QString& label, int value, bool isDefault) {
        QAction* act = maxFramesMenu->addAction(label);
        act->setCheckable(true);
        act->setChecked(isDefault);
        maxFramesGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, value]() {
            messageModel->setMaxFrames(value);
        });
    };
    addFrameOption("500",       500,   false);
    addFrameOption("1,000",     1000,  false);
    addFrameOption("5,000",     5000,  false);
    addFrameOption("10,000",    10000, true);
    addFrameOption("Unlimited", 0,     false);

    settingsMenu->addSeparator();

    // --- Max Log Entries ---
    QMenu* maxLogMenu = settingsMenu->addMenu("Max Log Entries");
    QActionGroup* maxLogGroup = new QActionGroup(this);
    auto addLogOption = [&](const QString& label, int value, bool isDefault) {
        QAction* act = maxLogMenu->addAction(label);
        act->setCheckable(true);
        act->setChecked(isDefault);
        maxLogGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, value]() {
            MAXLOGHISTORY = value;
            while (logHistory.size() > MAXLOGHISTORY)
                logHistory.removeLast();
            updateLogDisplay();
        });
    };
    addLogOption("100",    100,   true);
    addLogOption("500",    500,   false);
    addLogOption("1,000",  1000,  false);
    addLogOption("5,000",  5000,  false);
    addLogOption("10,000", 10000, false);

    settingsMenu->addSeparator();

    QMenu* frameMenu = settingsMenu->addMenu("Frame");

    // --- Periodic Timer Resolution ---
    // Also enforces the minimum frame interval — no frame can fire faster than the timer tick.
    QMenu* timerMenu = frameMenu->addMenu("Periodic Timer Resolution");
    QActionGroup* timerGroup = new QActionGroup(this);
    auto addTimerOption = [&](const QString& label, int ms, bool isDefault) {
        QAction* act = timerMenu->addAction(label);
        act->setCheckable(true);
        act->setChecked(isDefault);
        timerGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, ms]() {
            m_timerResolutionMs = ms;
            server->setTimerInterval(ms);
            client->setTimerInterval(ms);
            for (auto* widget : std::as_const(frameWidgets))
                widget->setMinInterval(ms);
        });
    };
    addTimerOption("10 ms  (High Accuracy)", 10,  true);
    addTimerOption("50 ms",                  50,  false);
    addTimerOption("100 ms",                 100, false);
    addTimerOption("200 ms",                 200, false);
    addTimerOption("500 ms",                 500, false);
    addTimerOption("1 s",                    1000, false);

    frameMenu->addSeparator();

    // --- Max Periodic Interval ---
    QMenu* maxIntervalMenu = frameMenu->addMenu("Max Periodic Interval");
    QActionGroup* maxIntervalGroup = new QActionGroup(this);
    auto addMaxIntervalOption = [&](const QString& label, int ms, bool isDefault) {
        QAction* act = maxIntervalMenu->addAction(label);
        act->setCheckable(true);
        act->setChecked(isDefault);
        maxIntervalGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, ms]() {
            m_maxIntervalMs = ms;
            for (auto* widget : std::as_const(frameWidgets))
                widget->setMaxInterval(ms);
        });
    };
    addMaxIntervalOption("1,000 ms  (1 s)",     1000,  false);
    addMaxIntervalOption("5,000 ms  (5 s)",     5000,  false);
    addMaxIntervalOption("10,000 ms (Default)", 10000, true);
    addMaxIntervalOption("60,000 ms (1 min)",   60000, false);
    addMaxIntervalOption("Unlimited",           0,     false);

    settingsMenu->addSeparator();

    // --- Auto-scroll Analyzer ---
    QAction* autoScrollAction = settingsMenu->addAction("Auto-scroll Analyzer");
    autoScrollAction->setCheckable(true);
    autoScrollAction->setChecked(false);
    connect(autoScrollAction, &QAction::toggled, this, [this](bool checked) {
        m_autoScroll = checked;
    });
}


