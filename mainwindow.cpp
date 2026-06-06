#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QProcess>
#include <QStandardPaths>
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
    qDebug() << "=== 程序当前期待 ffmpeg 的路径为 ===";
    qDebug() << "qt environment path: " << qEnvironmentVariable("PATH");
    QString ffmpegPath = QStandardPaths::findExecutable("ffmpeg", { "/usr/local/bin", "/opt/homebrew/bin", "C:/ffmpeg/bin" });

    if (ffmpegPath.isEmpty()) {

        qWarning() << "ffmpeg not found in PATH";

    } else {

        qDebug() << "ffmpeg path:" << ffmpegPath;

    }

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
    // 判断没有窗口则创建新窗口的实例
    if (extractorWin == nullptr){
        extractorWin = new AudioExtractorWindow();

        // 设置当窗口关闭时，自动释放内存，防止内存泄漏
        extractorWin->setAttribute(Qt::WA_DeleteOnClose);
        //修改判断
        connect(extractorWin, &QObject::destroyed, this, [=](){
            extractorWin = nullptr;
        });
        //打开新窗口
        extractorWin->show();

    }
    else {
        extractorWin->raise();         // 把它提到窗口最上层
    }

}
//测试ffmpeg的版本
void MainWindow::on_To_ffmpeg_test_clicked()
{
    //运行之前定义好的ffmpeg检查函数
    testFFmpeg();
}

