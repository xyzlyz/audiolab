#include "batchextractorworker.h"

BatchExtractorWorker::BatchExtractorWorker(const QStringList& videoFiles, const QString& outputDir)
    : m_videoFiles(videoFiles), m_outputDir(outputDir)
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

    // 开始批量循环
    for (int i = 0; i < totalFiles; ++i) {
        QString videoPath = m_videoFiles[i];

        // 自动计算输出的音频路径：保持原文件名，后缀改为 .mp3，放入用户指定的输出目录
        QFileInfo videoInfo(videoPath);
        QString audioName = videoInfo.completeBaseName() + ".mp3";
        QString audioPath = m_outputDir + QDir::separator() + audioName;

        emit logMessage(QString("🎬 [%1/%2] 开始提取：%3").arg(i + 1).arg(totalFiles).arg(videoInfo.fileName()));
        emit progressUpdated(i, totalFiles);

        // 创建进程
        QProcess *ffmpegProcess = new QProcess();

        // 组装参数
        QStringList arguments;
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
    emit logMessage("\n🎉🎉🎉 所有文件批量音频提取完成！");
    emit finishedAll();
}