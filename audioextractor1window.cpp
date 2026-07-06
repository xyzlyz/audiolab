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
#include <QTimer>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <QTextBrowser>
#include <QCloseEvent>

namespace {
constexpr int kProgressReportIntervalMs = 3000;

struct ExtractProgressState
{
    qint64 durationMs = -1;
    qint64 currentMs = -1;
    QElapsedTimer elapsed;
    qint64 lastReportMs = 0;
};

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

qint64 ffmpegTimeToMs(const QString &timeText)
{
    static const QRegularExpression re(R"((\d+):(\d+):(\d+(?:\.\d+)?))");
    const QRegularExpressionMatch match = re.match(timeText);
    if (!match.hasMatch()) {
        return -1;
    }

    const int hours = match.captured(1).toInt();
    const int minutes = match.captured(2).toInt();
    const double seconds = match.captured(3).toDouble();
    return static_cast<qint64>(((hours * 3600) + (minutes * 60) + seconds) * 1000.0);
}

QString formatMs(qint64 ms)
{
    if (ms < 0) {
        return "未知";
    }

    const qint64 totalSeconds = ms / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

void updateProgressFromFfmpegOutput(const QString &output, const QSharedPointer<ExtractProgressState> &state)
{
    static const QRegularExpression durationRe(R"(Duration:\s*(\d+:\d+:\d+(?:\.\d+)?))");
    static const QRegularExpression timeRe(R"(time=\s*(\d+:\d+:\d+(?:\.\d+)?))");

    if (state->durationMs < 0) {
        const QRegularExpressionMatch durationMatch = durationRe.match(output);
        if (durationMatch.hasMatch()) {
            state->durationMs = ffmpegTimeToMs(durationMatch.captured(1));
        }
    }

    QRegularExpressionMatchIterator it = timeRe.globalMatch(output);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        state->currentMs = ffmpegTimeToMs(match.captured(1));
    }
}

QString progressText(const QSharedPointer<ExtractProgressState> &state)
{
    if (state->durationMs > 0 && state->currentMs >= 0) {
        const double percent = qMin(100.0, state->currentMs * 100.0 / state->durationMs);
        return QString("当前进度：%1 / %2（%3%）")
            .arg(formatMs(state->currentMs), formatMs(state->durationMs))
            .arg(QString::number(percent, 'f', 1));
    }

    if (state->currentMs >= 0) {
        return QString("当前进度：已处理到 %1，总时长未知").arg(formatMs(state->currentMs));
    }

    return QString("当前状态：FFmpeg 正在分析输入/准备提取，已运行 %1 秒")
        .arg(state->elapsed.isValid() ? state->elapsed.elapsed() / 1000 : 0);
}

void maybeReportProgress(QTextBrowser *log, const QSharedPointer<ExtractProgressState> &state, bool force = false)
{
    if (!state->elapsed.isValid()) {
        return;
    }

    const qint64 now = state->elapsed.elapsed();
    if (!force && now - state->lastReportMs < kProgressReportIntervalMs) {
        return;
    }

    state->lastReportMs = now;
    log->append("⏳ " + progressText(state));
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
    stopActiveExtraction();
    delete ui;
}

void AudioExtractor1Window::closeEvent(QCloseEvent *event)
{
    if (m_ffmpegProcess && m_ffmpegProcess->state() != QProcess::NotRunning) {
        if (ui) {
            ui->txtLog->append("⚠️ 正在关闭窗口，已请求中止当前音频提取...");
        }
        stopActiveExtraction();
    }

    QWidget::closeEvent(event);
}

void AudioExtractor1Window::stopActiveExtraction()
{
    if (!m_ffmpegProcess || m_isStopping) {
        return;
    }

    m_isStopping = true;
    QProcess *process = m_ffmpegProcess.data();

    // 窗口即将关闭时先断开所有与窗口/UI相关的信号，避免进程结束后继续访问已销毁控件。
    process->disconnect(this);
    disconnect(process, nullptr, this, nullptr);

    if (process->state() != QProcess::NotRunning) {
        process->terminate();
        if (!process->waitForFinished(3000)) {
            process->kill();
            process->waitForFinished(3000);
        }
    }

    process->deleteLater();
    m_ffmpegProcess.clear();
    m_isStopping = false;
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
    ui->txtLog->append(QString("将每 %1 秒输出一次当前状态与进度。").arg(kProgressReportIntervalMs / 1000));
    if (remoteInput && !headers.isEmpty()) {
        ui->txtLog->append("已为 URL 请求携带自定义 Header。");
    }

    if (m_ffmpegProcess && m_ffmpegProcess->state() != QProcess::NotRunning) {
        ui->txtLog->append("⚠️ 已有提取任务正在进行，请等待结束或关闭窗口中止任务。");
        return;
    }

    QProcess *ffmpegProcess = new QProcess(this);
    m_ffmpegProcess = ffmpegProcess;
    auto progressState = QSharedPointer<ExtractProgressState>::create();
    progressState->elapsed.start();

    // 组装参数。注意：-headers 必须放在 -i 之前，才会作用于输入 URL。
    QStringList arguments;
    if (remoteInput && !headers.isEmpty()) {
        arguments << "-headers" << headers;
    }
    arguments << "-i" << videoPath
              << "-vn"
              << "-acodec" << "libmp3lame"
              << "-b:a" << "32k"
              << "-y"
              << audioPath;

    QTimer *progressTimer = new QTimer(ffmpegProcess);
    progressTimer->setInterval(kProgressReportIntervalMs);
    connect(progressTimer, &QTimer::timeout, this, [=](){
        if (ffmpegProcess->state() == QProcess::Running) {
            maybeReportProgress(ui->txtLog, progressState, true);
        }
    });

    connect(ffmpegProcess, &QProcess::readyReadStandardError, this, [=](){
        const QString errorOutput = QString::fromLocal8Bit(ffmpegProcess->readAllStandardError());
        updateProgressFromFfmpegOutput(errorOutput, progressState);
        maybeReportProgress(ui->txtLog, progressState);
    });

    // 监测进程启动失败的信号
    connect(ffmpegProcess, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error){
        ui->txtLog->append( "进程启动时发生错误，代码: " + QString::number(error) );
        ui->txtLog->append( "系统提示: " + ffmpegProcess->errorString() );
    });

    // 启动后台进程
    ffmpegProcess->start(ffmpegPath, arguments);
    progressTimer->start();

    // 绑定结束信号
    connect(ffmpegProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [=](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus);

                progressTimer->stop();
                maybeReportProgress(ui->txtLog, progressState, true);

                if (m_ffmpegProcess == ffmpegProcess) {
                    m_ffmpegProcess.clear();
                }

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
