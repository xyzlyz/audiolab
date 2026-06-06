#include "audiocutterwindow.h"
#include "ui_audiocutterwindow.h"

audiocutterwindow::audiocutterwindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::audiocutterwindow)
{
    ui->setupUi(this);
}

audiocutterwindow::~audiocutterwindow()
{
    delete ui;
}
