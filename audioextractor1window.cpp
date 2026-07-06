#include "audioextractor1window.h"
#include "ui_audioextractor1window.h"
#include "audiocutterwindow.h"
#include <QFileDialog>
#include <QProcess>
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QFileInfo>
#include <QUrl>
#include <QRegularExpression>

namespace {
bool isRemoteInput(const QString &input)
{
    const QUrl url(input.trimmed());
    return url.isValid() && (url.scheme() == "http" || url.scheme() == "https");
}

QString normalizeHttpHeaders(const QString &rawHeaders)
{
    QStringList normalizedLines;
    const QStringList lines = rawHeaders.split(QRegularExpression("\\r?\\n"), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        if (!line.isEmpty()) {
            normalizedLines << line;
        }
    }

    if (normalizedLines.isEmpty()) {
        return QString();
    }

    return normalizedLines.join("\r\n") + "\r\n";
}
}

AudioExtractor1Window::AudioExtractor1Window(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AudioExtractor1Window)
{
    ui->setupUi(this);
    setWindowTitle("音视频分离工具");
    ui->lineEditVideo->setPlaceholderText("可选择本地视频，也可直接输入 http/https URL");
    ui->textEditHeaders->setPlaceholderText("可选。每行一个 Header，例如：\nAuthorization: Bearer xxxxx\nReferer: https://example.com");
}

AudioExtractor1Window::~AudioExtractor1Window()
{
    delete ui;
}

void AudioExtractor1Window::on_btnSelectVideo_clicked()
{
    QString videoPath = QFileDialog::getOpenFileName(
        this,
        "选择要提取音频的视频",
        "",
        "视频文件 (*.mp4 *.mkv *.avi *.mov);;所有文件 (*.*)"
        );

    if (!videoPath.isEmpty()) {
        ui->lineEditVideo->setText(videoPath);

        // 自动生成同名 mp3 路径
        QString audioPath = videoPath;
        int lastDot = audioPath.lastIndexOf('.');
        if (lastDot != -1) {
            audioPath.replace(lastDot, audioPath.length() - lastDot, ".mp3");
        } else {
            audioPath += ".mp3";
        }
        ui->lineEditAudio->setText(audioPath);

        ui->txtLog->append("已选择本地视频: " + videoPath);
    }

}


void AudioExtractor1Window::on_btnExtractAudio_clicked()
{
    QString videoPath = ui->lineEditVideo->text().trimmed();
    QString audioPath = ui->lineEditAudio->text().trimmed();
    const bool remoteInput = isRemoteInput(videoPath);
    const QString headers = normalizeHttpHeaders(ui->textEditHeaders->toPlainText());

    //判断是否选择了视频文件或填写 URL
    if (videoPath.isEmpty() || audioPath.isEmpty()) {
        ui->txtLog->append(" 错误：请先选择视频文件/填写URL，并设置输出音频路径！");
        return;
    }

    if (!remoteInput && !QFileInfo::exists(videoPath)) {
        ui->txtLog->append(" 错误：本地视频文件不存在！");
        return;
    }

    // 寻找ffmpeg的路径
    QString ffmpegPath = QStandardPaths::findExecutable("ffmpeg", { "/usr/local/bin", "/opt/homebrew/bin", "C:/ffmpeg/bin" });

    if (ffmpegPath.isEmpty()) {
        ui->txtLog->append("未找到 FFmpeg，请确认已安装并配置到 PATH。");
        qWarning() << "ffmpeg not found in PATH";
        return;
    } else {
        qDebug() << "ffmpeg path:" << ffmpegPath;
    }
    //--------寻找结束--------

    ui->txtLog->append("寻路成功！FFmpeg 绝对路径：" + ffmpegPath);
    ui->txtLog->append(remoteInput ? "开始从 URL 提取音频，请稍候..." : "开始提取本地音频，请稍候...");
    if (remoteInput && !headers.isEmpty()) {
        ui->txtLog->append("已为 URL 请求携带自定义 Header。");
    }

    QProcess *ffmpegProcess = new QProcess(this);

    // 组装参数。注意：-headers 必须放在 -i 之前，才会作用于输入 URL。
    QStringList arguments;
    if (remoteInput && !headers.isEmpty()) {
        arguments << "-headers" << headers;
    }
    arguments << "-i" << videoPath
              << "-vn"
              << "-acodec" << "libmp3lame"
              << "-q:a" << "2"
              << "-y"
              << audioPath;

    connect(ffmpegProcess, &QProcess::readyReadStandardError, this, [=](){
        QString errorOutput = QString::fromLocal8Bit(ffmpegProcess->readAllStandardError());
        Q_UNUSED(errorOutput);
        //ui->txtLog->append(errorOutput); // 实时滚动打印 FFmpeg 的内部进度
    });

    // 监测进程启动失败的信号
    connect(ffmpegProcess, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error){
        ui->txtLog->append( "进程启动时发生错误，代码: " + QString::number(error) );
        ui->txtLog->append( "系统提示: " + ffmpegProcess->errorString() );
    });

    // 启动后台进程
    ffmpegProcess->start(ffmpegPath, arguments);

    // 绑定结束信号
    connect(ffmpegProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [=](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus);

                if (exitCode == 0) {
                    ui->txtLog->append("\n🎉🎉🎉 音频提取成功！");
                    ui->txtLog->append("文件保存在：" + audioPath);

                    if (ui->checkBox->isChecked()) {
                        ui->txtLog->append("已勾选分离后进入音频剪辑，正在打开剪辑窗口...");

                        audiocutterwindow *cutterWin = new audiocutterwindow();
                        cutterWin->setAttribute(Qt::WA_DeleteOnClose);
                        cutterWin->setInputAudioPath(audioPath);
                        cutterWin->show();

                        this->close();
                    }
                } else {
                    ui->txtLog->append("提取中止，FFmpeg 退出码: " + QString::number(exitCode));
                }

                ffmpegProcess->deleteLater();
            });
}


void AudioExtractor1Window::on_audio_route_change_clicked()
{
    // 获取当前 lineEditAudio 中已经有的路径，作为打开对话框时的默认推荐位置
    QString defaultPath = ui->lineEditAudio->text();
    Q_UNUSED(defaultPath);


    QString customAudioPath = QFileDialog::getSaveFileName(
        this,
        "选择音频保存位置",      // 对话框标题
        "",
        "音频文件 (*.mp3)"       // 限制保存格式为 MP3
        );

    if (!customAudioPath.isEmpty()) {
        ui->lineEditAudio->setText(customAudioPath);
        ui->txtLog->append("📝 用户更改保存路径为: " + customAudioPath);
    }
}
