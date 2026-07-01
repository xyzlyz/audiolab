#include "silencecut.h"
#include "ui_silencecut.h"
#include <QFileDialog>
#include <QStandardPaths>

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

    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("选择要切分的音频文件"),
        "",
        tr("音频文件 (*.mp3 *.wav *.m4a *.flac);;所有文件 (*.*)")//音频格式可选，其他为灰
        );

    if (!filePath.isEmpty()) {
        // 显示输入路径
        ui->input_address->setText(filePath);

        // 在TextLog中打印日志
        ui->TextLog->append(QString("<font color='#007AFF'><b>[输入]</b></font> 已选择音频：%1").arg(filePath));
    }
    else{
        ui->TextLog->append(QString("请选择音频"));
    }
}

