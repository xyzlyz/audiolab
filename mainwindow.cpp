#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QProcess>
#include <QStandardPaths>
#include <QDebug>
#include <QCoreApplication>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "audioextractorwindow.h"
#include "silencecut.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 全局按钮样式，所有窗口按钮共用
    this->setStyleSheet(R"(
        QPushButton {
            background-color: #ecf5c4;
            color: black;
            border-radius: 6px;
            padding: 6px 14px;
            font-size: 13px;
            box-shadow: 0 3px 4px rgba(0,0,0,0.25);
        }
        QPushButton:hover {
            background-color: #bdc791;
            box-shadow: 0 4px 5px rgba(0,0,0,0.3);
        }
        QPushButton:pressed {
            background-color: #858f57;
            box-shadow: 0 1px 2px rgba(0,0,0,0.2);
            transform: translateY(2px);
        }
    )");

}
//测试ffmpeg的版本以及调用路径
void MainWindow::testFFmpeg(){
    QString buildPath = QCoreApplication::applicationDirPath();
    qDebug() << "=== 搜索 ffmpeg 路径 ===";
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
        ui->textLog->clear();
        ui->textLog->append("=== FFMPEG 调用成功！===");
        qDebug() <<output.left(300);
    } else {
        ui->textLog->append("======= FFMPEG 调用失败！=======");

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


void MainWindow::on_To_audiocut_clicked()
{
    // 判断没有窗口则创建新窗口的实例
    if (cutterWin == nullptr){
        cutterWin = new audiocutterwindow();

        // 设置当窗口关闭时，自动释放内存，防止内存泄漏
        cutterWin->setAttribute(Qt::WA_DeleteOnClose);
        //修改判断
        connect(cutterWin, &QObject::destroyed, this, [=](){
            cutterWin = nullptr;
        });
        //打开新窗口
        cutterWin->show();

    }
    else {
        cutterWin->raise();         // 把它提到窗口最上层
    }
}


void MainWindow::on_pushButton_clicked()
{
    // 判断没有窗口则创建新窗口的实例
    if (extractor1Win == nullptr){
        extractor1Win = new AudioExtractor1Window();

        // 设置当窗口关闭时，自动释放内存，防止内存泄漏
        extractor1Win->setAttribute(Qt::WA_DeleteOnClose);
        //修改判断
        connect(extractor1Win, &QObject::destroyed, this, [=](){
            extractor1Win = nullptr;
        });
        //打开新窗口
        extractor1Win->show();

    }
    else {
        extractor1Win->raise();         // 把它提到窗口最上层
    }

}


void MainWindow::on_scut_clicked()
{
    // 判断没有窗口则创建新窗口的实例
    if (silencecutWin == nullptr){
        silencecutWin = new silencecut();

        // 设置当窗口关闭时，自动释放内存，防止内存泄漏
        silencecutWin->setAttribute(Qt::WA_DeleteOnClose);
        //修改判断
        connect(silencecutWin, &QObject::destroyed, this, [=](){
            silencecutWin = nullptr;
        });
        //打开新窗口
        silencecutWin->show();

    }
    else {
        silencecutWin->raise();         // 把它提到窗口最上层
    }
}

