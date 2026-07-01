#include "silencecut.h"
#include "ui_silencecut.h"

silencecut::silencecut(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::silencecut)
{
    ui->setupUi(this);
    setWindowTitle("智能静音分割工具");
}

silencecut::~silencecut()
{
    delete ui;
}

void silencecut::on_audio_select_botton_clicked()
{

}

