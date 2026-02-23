/**
 * @file mainwindow_menubar.cpp
 * @brief Menu bar construction and theme application for MainWindow.
 *
 * Contains:
 *  - MainWindow::setupMenuBar()  — builds the full menu bar (CANBridge, View, Settings)
 *  - MainWindow::applyTheme()    — applies dark/light stylesheet to the application
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

    connect(autoThemeAction,  &QAction::triggered, this, [this]() { applyTheme(false); });
    connect(lightThemeAction, &QAction::triggered, this, [this]() { applyTheme(false); });
    connect(darkModeAction,   &QAction::triggered, this, [this]() { applyTheme(true);  });

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
    addTimerOption("1 ms  (High Accuracy)", 1,  false);
    addTimerOption("5 ms",                  5,  false);
    addTimerOption("10 ms (Default)",       10, true);
    addTimerOption("50 ms (Low CPU)",       50, false);

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

// ============================================================================
// Theme
// ============================================================================

/**
 * Apply a dark or light theme to the entire application via stylesheet.
 * Dark theme uses a custom QSS stylesheet; light theme clears it to
 * restore Qt's default platform look.
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
