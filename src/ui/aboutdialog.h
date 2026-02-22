/**
 * @file aboutdialog.h
 * @brief Definition of the AboutDialog class.
 *
 * A simple modal dialog that displays application information:
 * name, version, description, technologies used, and author credits.
 */

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>

#include "version.h"

/**
 * @class AboutDialog
 * @brief Modal dialog showing application metadata and author information.
 */
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

#endif // ABOUTDIALOG_H
