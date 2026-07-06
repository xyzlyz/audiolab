#include "batchextractorworker.h"
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
    if (!m_httpHeaders.isEmpty()) {
        emit logMessage("🌐 URL 输入会携带自定义 HTTP Header。");
    }

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

        // 自动计算输出的音频路径：保持原文件名/URL文件名，后缀改为 .mp3，放入用户指定的输出目录
        QString audioName = outputNameForInput(videoPath, i);
        QString audioPath = m_outputDir + QDir::separator() + audioName;

        emit logMessage(QString("🎬 [%1/%2] 开始提取：%3").arg(i + 1).arg(totalFiles).arg(displayNameForInput(videoPath)));
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
                  << "-q:a" << "2"
                  << "-y"
                  << audioPath;

        // 绑定错误和日志信号
        connect(ffmpegProcess, &QProcess::readyReadStandardError, this, [=](){
        });

        connect(ffmpegProcess, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error){
            emit logMessage("❌ 进程启动时发生错误，代码: " + QString::number(error) + " -> " + ffmpegProcess->errorString());
        });

        // 启动进程
        ffmpegProcess->start(ffmpegPath, arguments);

        ffmpegProcess->waitForFinished(-1);

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
