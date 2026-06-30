#include "silencecut.h"
#include "ui_silencecut.h"

silencecut::silencecut(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::silencecut)
{
    ui->setupUi(this);
}

silencecut::~silencecut()
{
    delete ui;
}
