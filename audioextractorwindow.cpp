#include "audioextractorwindow.h"
#include "ui_audioextractorwindow.h"
#include "audioextractorwindow.h"
#include "ui_audioextractorwindow.h"
#include <QFileDialog>
#include <QProcess>
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>

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
    QString ffmpegPath = QStandardPaths::findExecutable("ffmpeg", { "/usr/local/bin", "/opt/homebrew/bin", "C:/ffmpeg/bin" });

    if (ffmpegPath.isEmpty()) {

        qWarning() << "ffmpeg not found in PATH";

    } else {

        qDebug() << "ffmpeg path:" << ffmpegPath;

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


void AudioExtractorWindow::on_audio_route_change_clicked()
{
    // 获取当前 lineEditAudio 中已经有的路径，作为打开对话框时的默认推荐位置
    QString defaultPath = ui->lineEditAudio->text();

    // 如果目前还没有路径，就默认打开 Mac 的下载目录或用户目录
    if (defaultPath.isEmpty()) {
        defaultPath = "/Users/tanyixiao/Downloads/output.mp3";
    }

    // 💡 弹出“保存文件”对话框
    QString customAudioPath = QFileDialog::getSaveFileName(
        this,
        "选择音频保存位置",      // 对话框标题
        defaultPath,            // 默认文件名和路径
        "音频文件 (*.mp3)"       // 限制保存格式为 MP3
        );

    // 如果用户没有点取消（即路径不为空），就把用户选的自定义路径更新到输入框里
    if (!customAudioPath.isEmpty()) {
        ui->lineEditAudio->setText(customAudioPath);
        ui->txtLog->append("📝 用户更改保存路径为: " + customAudioPath);
    }
}

