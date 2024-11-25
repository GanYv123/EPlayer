#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_EPlayerClient.h"

class EPlayerClient : public QMainWindow
{
    Q_OBJECT

public:
    EPlayerClient(QWidget *parent = nullptr);
    ~EPlayerClient();

private:
    Ui::EPlayerClientClass ui;
};
