#pragma once

#include <QObject>
#include <QStringList>
#include <QProcess>
#include <QStandardPaths>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <atomic>

class BatchExtractorWorker : public QObject
{
    Q_OBJECT
public:
    // 构造函数：把主线程收集到的视频/URL列表传进来
    BatchExtractorWorker(const QStringList& videoFiles, const QString& outputDir, const QString& httpHeaders = QString());

public slots:
    // 线程启动后的核心循环入口
    void startBatchProcess();
    void requestCancel();

signals:
    // 用信号把日志和进度传回给主线程 UI
    void logMessage(const QString& message);
    void progressUpdated(int current, int total);
    void finishedAll();

private:
    QStringList m_videoFiles;
    QString m_outputDir; // 统一的输出目录
    QString m_httpHeaders; // URL 输入时携带的 HTTP Header
    std::atomic_bool m_cancelRequested { false };
};
