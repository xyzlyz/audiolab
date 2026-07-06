#include "batchextractorworker.h"
#include <QUrl>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QSet>

namespace {
constexpr int kProgressReportIntervalMs = 3000;

struct ExtractProgressState
{
    qint64 durationMs = -1;
    qint64 currentMs = -1;
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

void updateProgressFromFfmpegOutput(const QString &output, ExtractProgressState &state)
{
    static const QRegularExpression durationRe(R"(Duration:\s*(\d+:\d+:\d+(?:\.\d+)?))");
    static const QRegularExpression timeRe(R"(time=\s*(\d+:\d+:\d+(?:\.\d+)?))");

    if (state.durationMs < 0) {
        const QRegularExpressionMatch durationMatch = durationRe.match(output);
        if (durationMatch.hasMatch()) {
            state.durationMs = ffmpegTimeToMs(durationMatch.captured(1));
        }
    }

    QRegularExpressionMatchIterator it = timeRe.globalMatch(output);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        state.currentMs = ffmpegTimeToMs(match.captured(1));
    }
}

QString progressText(const ExtractProgressState &state)
{
    if (state.durationMs > 0 && state.currentMs >= 0) {
        const double percent = qMin(100.0, state.currentMs * 100.0 / state.durationMs);
        return QString("当前文件进度：%1 / %2（%3%）")
            .arg(formatMs(state.currentMs), formatMs(state.durationMs))
            .arg(QString::number(percent, 'f', 1));
    }

    if (state.currentMs >= 0) {
        return QString("当前文件进度：已处理到 %1，总时长未知").arg(formatMs(state.currentMs));
    }

    return "当前文件状态：FFmpeg 正在分析输入/准备提取";
}

QString outputNameForInput(const QString &input, int index)
{
    if (isRemoteInput(input)) {
        const QUrl url(input);
        QFileInfo urlInfo(url.path());
        QString baseName = urlInfo.completeBaseName();
        if (baseName.isEmpty()) {
            baseName = QString("url_audio_%1").arg(index + 1, 3, 10, QChar('0'));
        }
        return baseName + ".mp3";
    }

    QFileInfo videoInfo(input);
    QString baseName = videoInfo.completeBaseName();
    if (baseName.isEmpty()) {
        baseName = QString("audio_%1").arg(index + 1, 3, 10, QChar('0'));
    }
    return baseName + ".mp3";
}

QString uniqueOutputPath(const QString &outputDir, const QString &preferredName, QSet<QString> &reservedPaths)
{
    QFileInfo preferredInfo(preferredName);
    QString baseName = preferredInfo.completeBaseName();
    QString suffix = preferredInfo.suffix();

    if (baseName.isEmpty()) {
        baseName = "audio";
    }
    if (suffix.isEmpty()) {
        suffix = "mp3";
    }

    int duplicateIndex = 0;
    while (true) {
        const QString fileName = duplicateIndex == 0
            ? QString("%1.%2").arg(baseName, suffix)
            : QString("%1_%2.%3").arg(baseName).arg(duplicateIndex).arg(suffix);
        const QString candidatePath = QDir(outputDir).filePath(fileName);
        const QString normalizedPath = QDir::cleanPath(candidatePath).toCaseFolded();

        if (!reservedPaths.contains(normalizedPath) && !QFileInfo::exists(candidatePath)) {
            reservedPaths.insert(normalizedPath);
            return candidatePath;
        }

        ++duplicateIndex;
    }
}

QString displayNameForInput(const QString &input)
{
    if (isRemoteInput(input)) {
        const QUrl url(input);
        QFileInfo urlInfo(url.path());
        const QString fileName = urlInfo.fileName();
        return fileName.isEmpty() ? input : fileName;
    }

    return QFileInfo(input).fileName();
}
}

BatchExtractorWorker::BatchExtractorWorker(const QStringList& videoFiles, const QString& outputDir, const QString& httpHeaders)
    : m_videoFiles(videoFiles), m_outputDir(outputDir), m_httpHeaders(normalizeHttpHeaders(httpHeaders))
{
}

void BatchExtractorWorker::startBatchProcess()
{
    int totalFiles = m_videoFiles.size();
    if (totalFiles == 0) {
        emit finishedAll();
        return;
    }

    // 寻找ffmpeg的路径
    QString ffmpegPath = QStandardPaths::findExecutable("ffmpeg", { "/usr/local/bin", "/opt/homebrew/bin", "C:/ffmpeg/bin" });
    if (ffmpegPath.isEmpty()) {
        emit logMessage("❌ 错误：未找到 FFmpeg 绝对路径，无法开始转换！");
        emit finishedAll();
        return;
    }

    emit logMessage("🎬 寻路成功！FFmpeg 绝对路径：" + ffmpegPath);
    emit logMessage(QString("⏱️ 批量提取过程中将每 %1 秒输出一次当前状态与进度。").arg(kProgressReportIntervalMs / 1000));
    if (!m_httpHeaders.isEmpty()) {
        emit logMessage("🌐 URL 输入会携带自定义 HTTP Header。");
    }

    QSet<QString> reservedOutputPaths;

    // 开始批量循环
    for (int i = 0; i < totalFiles; ++i) {
        QString videoPath = m_videoFiles[i].trimmed();
        const bool remoteInput = isRemoteInput(videoPath);

        if (videoPath.isEmpty()) {
            emit logMessage(QString("⚠️ [%1/%2] 跳过空输入").arg(i + 1).arg(totalFiles));
            continue;
        }

        if (!remoteInput && !QFileInfo::exists(videoPath)) {
            emit logMessage(QString("❌ [%1/%2] 本地文件不存在，已跳过：%3").arg(i + 1).arg(totalFiles).arg(videoPath));
            continue;
        }

        // 自动计算输出的音频路径：保持原文件名/URL文件名，后缀改为 .mp3，放入用户指定的输出目录。
        // 若同一批次或输出目录中已有同名文件，则自动追加 _1、_2...，避免覆盖。
        QString preferredAudioName = outputNameForInput(videoPath, i);
        QString audioPath = uniqueOutputPath(m_outputDir, preferredAudioName, reservedOutputPaths);
        QString audioName = QFileInfo(audioPath).fileName();
        if (audioName != preferredAudioName) {
            emit logMessage(QString("🛡️ [%1/%2] 检测到同名输出，已改用：%3").arg(i + 1).arg(totalFiles).arg(audioName));
        }

        emit logMessage(QString("🎬 [%1/%2] 开始提取：%3 -> %4").arg(i + 1).arg(totalFiles).arg(displayNameForInput(videoPath), audioName));
        emit progressUpdated(i, totalFiles);

        // 创建进程
        QProcess *ffmpegProcess = new QProcess();

        // 组装参数。注意：-headers 必须放在 -i 之前，才会作用于输入 URL。
        QStringList arguments;
        if (remoteInput && !m_httpHeaders.isEmpty()) {
            arguments << "-headers" << m_httpHeaders;
        }
        arguments << "-i" << videoPath
                  << "-vn"
                  << "-acodec" << "libmp3lame"
                  << "-b:a" << "32k"
                  << "-y"
                  << audioPath;

        ExtractProgressState progressState;
        QElapsedTimer elapsed;
        elapsed.start();
        qint64 lastReportMs = 0;

        connect(ffmpegProcess, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error){
            emit logMessage("❌ 进程启动时发生错误，代码: " + QString::number(error) + " -> " + ffmpegProcess->errorString());
        });

        // 启动进程
        ffmpegProcess->setReadChannel(QProcess::StandardError);
        ffmpegProcess->start(ffmpegPath, arguments);
        if (!ffmpegProcess->waitForStarted(5000)) {
            emit logMessage("❌ FFmpeg 启动失败：" + ffmpegProcess->errorString());
            ffmpegProcess->deleteLater();
            continue;
        }

        while (ffmpegProcess->state() == QProcess::Running) {
            if (ffmpegProcess->waitForReadyRead(500)) {
                const QString output = QString::fromLocal8Bit(ffmpegProcess->readAllStandardError());
                updateProgressFromFfmpegOutput(output, progressState);
            }

            const qint64 now = elapsed.elapsed();
            if (now - lastReportMs >= kProgressReportIntervalMs) {
                lastReportMs = now;
                emit logMessage(QString("⏳ [%1/%2] %3；总体进度：已完成 %4/%5，已运行 %6 秒")
                                    .arg(i + 1)
                                    .arg(totalFiles)
                                    .arg(progressText(progressState))
                                    .arg(i)
                                    .arg(totalFiles)
                                    .arg(now / 1000));
            }
        }

        const QString remainingOutput = QString::fromLocal8Bit(ffmpegProcess->readAllStandardError());
        if (!remainingOutput.isEmpty()) {
            updateProgressFromFfmpegOutput(remainingOutput, progressState);
        }

        emit logMessage(QString("⏳ [%1/%2] %3；总体进度：已完成 %4/%5")
                            .arg(i + 1)
                            .arg(totalFiles)
                            .arg(progressText(progressState))
                            .arg(i + 1)
                            .arg(totalFiles));

        if (ffmpegProcess->exitCode() == 0) {
            emit logMessage("🎉 提取成功 -> " + audioName);
        } else {
            emit logMessage("❌ 提取失败，退出码: " + QString::number(ffmpegProcess->exitCode()));
        }

        ffmpegProcess->deleteLater();
    }

    emit progressUpdated(totalFiles, totalFiles);
    emit logMessage("\n🎉🎉🎉 所有文件/URL批量音频提取完成！");
    emit finishedAll();
}
