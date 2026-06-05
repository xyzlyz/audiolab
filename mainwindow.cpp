#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QProcess>
#include <QDebug>
#include <QCoreApplication>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "audioextractorwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

}
//测试ffmpeg的版本以及调用路径
void MainWindow::testFFmpeg(){
    QString buildPath = QCoreApplication::applicationDirPath();
    qDebug() << "=== 程序当前期待 ffmpeg 的绝对路径为 ===";
    qDebug() << buildPath;
    QString ffmpegPath = buildPath + "/ffmpeg";

    QProcess *process = new QProcess(this);
    process->start(ffmpegPath, QStringList() << "-version");

    if (process->waitForFinished(3000)) {
        QString output = QString::fromLocal8Bit(process->readAllStandardOutput());
        qDebug() << "======= FFMPEG 联调成功！=======";
        qDebug() << output.left(300);
    } else {
        qDebug() << "======= FFMPEG 调用失败！=======";
        qDebug() << "错误代码:" << process->error();
        qDebug() << "系统提示:" << process->errorString();
    }
}
MainWindow::~MainWindow()
{
    delete ui;
}

//转到音频提取窗口
void MainWindow::on_To_audioextract_clicked()
{
    // 创建新窗口的实例（加上 this，让主窗口作为其父组件管理内存）
    AudioExtractorWindow *extractorWin = new AudioExtractorWindow();

    // 设置当窗口关闭时，自动释放内存，防止内存泄漏
    extractorWin->setAttribute(Qt::WA_DeleteOnClose);

    //打开新窗口
    extractorWin->show();
}
//测试ffmpeg的版本
void MainWindow::on_To_ffmpeg_test_clicked()
{
    //运行之前定义好的ffmpeg检查函数
    testFFmpeg();
}

