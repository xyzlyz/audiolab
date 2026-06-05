#include "audioextractorwindow.h"
#include "ui_audioextractorwindow.h"
#include "audioextractorwindow.h"
#include "ui_audioextractorwindow.h"
#include <QFileDialog>
#include <QProcess>
#include <QCoreApplication>
#include <QDebug>

AudioExtractorWindow::AudioExtractorWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AudioExtractorWindow)
{
    ui->setupUi(this);
    setWindowTitle("音视频分离工具");
}

AudioExtractorWindow::~AudioExtractorWindow()
{
    delete ui;
}

void AudioExtractorWindow::on_btnSelectVideo_clicked()
{
    QString videoPath = QFileDialog::getOpenFileName(
        this,
        "选择要提取音频的视频",
        "/Users/tanyixiao",
        "视频文件 (*.mp4 *.mkv *.avi *.mov)"
        );

    if (!videoPath.isEmpty()) {
        ui->lineEditVideo->setText(videoPath);

        // 自动生成同名 mp3 路径
        QString audioPath = videoPath;
        int lastDot = audioPath.lastIndexOf('.');
        if (lastDot != -1) {
            audioPath.replace(lastDot, audioPath.length() - lastDot, ".mp3");
        }
        ui->lineEditAudio->setText(audioPath);

        ui->txtLog->append("已选择视频: " + videoPath);
    }

}


void AudioExtractorWindow::on_btnExtractAudio_clicked()
{
    QString videoPath = ui->lineEditVideo->text();
    QString audioPath = ui->lineEditAudio->text();

    //判断是否选择了视频文件
    if (videoPath.isEmpty() || audioPath.isEmpty()) {
        ui->txtLog->append("⚠️ 错误：请先选择视频文件！");
        return;
    }

    // 寻找ffmpeg的路径
    QString buildPath = QCoreApplication::applicationDirPath();
    QString ffmpegPath = "";

    // 探测路径方案 1：如果在 .app 内部的 MacOS 文件夹下 (方案B)
    if (QFile::exists(buildPath + "/ffmpeg")) {
        ffmpegPath = buildPath + "/ffmpeg";
    }
    // 探测路径方案 2：如果是往外跳三级，在 build 根目录下 (方案A)
    else if (QFile::exists(buildPath + "/../../../ffmpeg")) {
        ffmpegPath = buildPath + "/../../../ffmpeg";
    }
    // 探测路径方案 3：以防万一，如果只隔了两层
    else if (QFile::exists(buildPath + "/../../ffmpeg")) {
        ffmpegPath = buildPath + "/../../ffmpeg";
    }

    // 如果全灭，说明文件真的没放对地方
    if (ffmpegPath.isEmpty()) {
        ui->txtLog->append("❌ 严重错误：在以下位置均未找到 ffmpeg 二进制文件！");
        ui->txtLog->append("当前程序运行位置: " + buildPath);
        ui->txtLog->append("请确保 ffmpeg 放入了 build 根目录或 .app 包内的 MacOS 文件夹中。");
        return;
    }
    //--------寻找结束--------

    ui->txtLog->append("🎬 寻路成功！FFmpeg 绝对路径：" + ffmpegPath);
    ui->txtLog->append("🎬 开始提取音频，请稍候...");
    QProcess *ffmpegProcess = new QProcess(this);

    // 2. 组装参数
    QStringList arguments;
    arguments << "-i" << videoPath
              << "-vn"
              << "-acodec" << "libmp3lame"
              << "-q:a" << "2"
              << "-y"
              << audioPath;

    // 💡 核心升级 1：把 FFmpeg 的实时标准错误输出流（FFmpeg 的所有进度日志默认都在标准错误流里）
    // 绑定到我们的日志文本框中。这样它只要有一丁点动静，我们都能立刻看到！
    connect(ffmpegProcess, &QProcess::readyReadStandardError, this, [=](){
        QString errorOutput = QString::fromLocal8Bit(ffmpegProcess->readAllStandardError());
        ui->txtLog->append(errorOutput); // 实时滚动打印 FFmpeg 的内部进度
    });

    // 💡 核心升级 2：防止进程因为某些特殊原因死在那，监测它启动失败的信号
    connect(ffmpegProcess, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error){
        ui->txtLog->append( "❌ 进程启动时发生错误，代码: " + QString::number(error) );
        ui->txtLog->append( "系统提示: " + ffmpegProcess->errorString() );
    });

    // 3. 启动后台进程
    ffmpegProcess->start(ffmpegPath, arguments);

    // 4. 绑定结束信号
    connect(ffmpegProcess, &QProcess::finished, this, [=](int exitCode){
        if (exitCode == 0) {
            ui->txtLog->append("\n🎉🎉🎉 音频提取成功！");
            ui->txtLog->append("文件保存在：" + audioPath);
        } else {
            ui->txtLog->append("\n❌ 提取中止，FFmpeg 退出码: " + QString::number(exitCode));
        }
        ffmpegProcess->deleteLater();
    });
}

