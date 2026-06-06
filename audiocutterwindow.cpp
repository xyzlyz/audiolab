#include "audiocutterwindow.h"
#include "ui_audiocutterwindow.h"

#include <QFileDialog>
#include <QProcess>
#include <QDebug>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QTime>

static QString timeToFfmpegString(const QTime &time)
{
    return time.toString("HH:mm:ss");
}

audiocutterwindow::audiocutterwindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::audiocutterwindow)
{
    ui->setupUi(this);
    setWindowTitle("音频剪辑工具");

    ui->timeEditStart->setDisplayFormat("HH:mm:ss");
    ui->timeEditEnd->setDisplayFormat("HH:mm:ss");
    ui->timeEditStart->setTime(QTime(0, 0, 0));
    ui->timeEditEnd->setTime(QTime(0, 1, 0));
}

audiocutterwindow::~audiocutterwindow()
{
    delete ui;
}

void audiocutterwindow::on_btnSelectVideo_clicked()
{
    QString audioPath = QFileDialog::getOpenFileName(
        this,
        "选择要剪辑的音频",
        QDir::homePath(),
        "音频文件 (*.mp3 *.wav *.aac *.m4a *.flac *.ogg);;所有文件 (*.*)"
        );

    if (!audioPath.isEmpty()) {
        ui->lineEditVideo->setText(audioPath);

        QFileInfo fileInfo(audioPath);
        QString outputPath = fileInfo.absolutePath()
                             + "/"
                             + fileInfo.completeBaseName()
                             + "_cut.mp3";
        ui->lineEditAudio->setText(outputPath);

        ui->txtLog->append("已选择音频: " + audioPath);
        ui->txtLog->append("已自动生成输出路径: " + outputPath);
    }
}

void audiocutterwindow::on_btnExtractAudio_clicked()
{
    QString inputAudioPath = ui->lineEditVideo->text().trimmed();
    QString outputAudioPath = ui->lineEditAudio->text().trimmed();
    QTime startTime = ui->timeEditStart->time();
    QTime endTime = ui->timeEditEnd->time();

    if (inputAudioPath.isEmpty() || outputAudioPath.isEmpty()) {
        ui->txtLog->append("⚠️ 错误：请先选择输入音频文件和输出路径！");
        return;
    }

    if (!QFileInfo::exists(inputAudioPath)) {
        ui->txtLog->append("⚠️ 错误：输入音频文件不存在！");
        return;
    }

    if (startTime >= endTime) {
        ui->txtLog->append("⚠️ 错误：结束时间必须晚于开始时间！");
        return;
    }

    QString ffmpegPath = QStandardPaths::findExecutable(
        "ffmpeg",
        { "/usr/local/bin", "/opt/homebrew/bin", "C:/ffmpeg/bin" }
        );

    if (ffmpegPath.isEmpty()) {
        ui->txtLog->append("❌ 未找到 FFmpeg，请确认已安装并配置到 PATH。");
        return;
    }

    QString startString = timeToFfmpegString(startTime);
    QString endString = timeToFfmpegString(endTime);

    ui->txtLog->append("🎬 FFmpeg 路径：" + ffmpegPath);
    ui->txtLog->append("🎧 开始剪辑音频，请稍候...");
    ui->txtLog->append("剪辑区间：" + startString + " ~ " + endString);

    QProcess *ffmpegProcess = new QProcess(this);

    QStringList arguments;
    arguments << "-ss" << startString
              << "-to" << endString
              << "-i" << inputAudioPath
              << "-vn"
              << "-acodec" << "libmp3lame"
              << "-q:a" << "2"
              << "-y"
              << outputAudioPath;

    connect(ffmpegProcess, &QProcess::readyReadStandardError, this, [=]() {
        QString errorOutput = QString::fromLocal8Bit(ffmpegProcess->readAllStandardError());
        // ui->txtLog->append(errorOutput);
    });

    connect(ffmpegProcess, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error) {
        ui->txtLog->append("❌ 进程启动时发生错误，代码: " + QString::number(error));
        ui->txtLog->append("系统提示: " + ffmpegProcess->errorString());
    });

    connect(ffmpegProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [=](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus);
                if (exitCode == 0) {
                    ui->txtLog->append("\n🎉🎉🎉 音频剪辑成功！");
                    ui->txtLog->append("文件保存在：" + outputAudioPath);
                } else {
                    ui->txtLog->append("\n❌ 剪辑失败，FFmpeg 退出码: " + QString::number(exitCode));
                }
                ffmpegProcess->deleteLater();
            });

    ffmpegProcess->start(ffmpegPath, arguments);
}

void audiocutterwindow::on_audio_route_change_clicked()
{
    QString defaultPath = ui->lineEditAudio->text();

    if (defaultPath.isEmpty()) {
        defaultPath = QDir::homePath() + "/output_cut.mp3";
    }

    QString customAudioPath = QFileDialog::getSaveFileName(
        this,
        "选择剪辑后音频的保存位置",
        defaultPath,
        "MP3 音频 (*.mp3);;WAV 音频 (*.wav);;AAC 音频 (*.aac);;所有文件 (*.*)"
        );

    if (!customAudioPath.isEmpty()) {
        ui->lineEditAudio->setText(customAudioPath);
        ui->txtLog->append("📝 用户更改保存路径为: " + customAudioPath);
    }
}
