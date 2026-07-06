#include "audioextractorwindow.h"
#include "ui_audioextractorwindow.h"
#include "audiocutterwindow.h"
#include <QFileDialog>
#include <QProcess>
#include <QCoreApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QFileInfo>
#include <QThread>
#include <QUrl>
#include <QRegularExpression>
#include "batchextractorworker.h"
#include <QCloseEvent>
#include <QTimer>
#include <QSettings>

AudioExtractorWindow::AudioExtractorWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AudioExtractorWindow)
{
    ui->setupUi(this);
    setWindowTitle("批量音视频分离工具");
    ui->textEditUrls->setPlaceholderText("可选。每行一个 http/https 视频 URL，点击开始时会并入批量列表");
    ui->textEditHeaders->setPlaceholderText("可选，每行一个 Header。\n例如：Authorization: Bearer xxxxx\nReferer: https://example.com");
    loadSettings();

}

AudioExtractorWindow::~AudioExtractorWindow()
{
    saveSettings();

    if (batchIsRunning()) {
        requestBatchCancel();
        if (m_batchThread) {
            m_batchThread->quit();
            m_batchThread->wait(5000);
        }
    }
    delete ui;
}


void AudioExtractorWindow::loadSettings()
{
    QSettings settings("AudioLab", "AudioLab");
    settings.beginGroup("AudioExtractorWindow");

    ui->checkRememberHeaders->setChecked(settings.value("rememberHeaders", false).toBool());
    if (ui->checkRememberHeaders->isChecked()) {
        ui->textEditHeaders->setPlainText(settings.value("headers").toString());
    }

    ui->lineEditAudio->setText(settings.value("outputAudioDir").toString());
    settings.endGroup();
}

void AudioExtractorWindow::saveSettings()
{
    if (!ui) {
        return;
    }

    QSettings settings("AudioLab", "AudioLab");
    settings.beginGroup("AudioExtractorWindow");

    const bool rememberHeaders = ui->checkRememberHeaders->isChecked();
    settings.setValue("rememberHeaders", rememberHeaders);
    if (rememberHeaders) {
        settings.setValue("headers", ui->textEditHeaders->toPlainText());
    } else {
        settings.remove("headers");
    }

    settings.setValue("outputAudioDir", ui->lineEditAudio->text().trimmed());
    settings.endGroup();
}

bool AudioExtractorWindow::batchIsRunning() const
{
    return m_batchThread && m_batchThread->isRunning();
}

void AudioExtractorWindow::requestBatchCancel()
{
    if (m_batchWorker) {
        m_batchWorker->requestCancel();
    }
}

void AudioExtractorWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();

    if (batchIsRunning()) {
        if (!m_closeAfterCancel) {
            m_closeAfterCancel = true;
            ui->txtLog->append("⚠️ 正在关闭窗口，已请求中止批量音频提取，请等待当前 FFmpeg 进程退出...");
            ui->btnExtractAudio->setEnabled(false);
            requestBatchCancel();
        } else {
            requestBatchCancel();
        }

        event->ignore();
        return;
    }

    QWidget::closeEvent(event);
}

void AudioExtractorWindow::on_btnSelectVideo_clicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        "选择要分离音频的视频文件（可多选）",
        "",
        "视频文件 (*.mp4 *.mkv *.avi *.mov *.flv)"
        );

    if (!files.isEmpty()) {
        ui->listVideos->addItems(files); // 直接把选中的多个路径塞进列表
        ui->txtLog->append(QString("📝 已添加 %1 个视频到待处理列表").arg(files.size()));

        // 获取用户选择的第一个视频的绝对路径
        QString firstVideoPath = files.first();

        // 使用 QFileInfo 分析这个路径
        QFileInfo fileInfo(firstVideoPath);

        // 提取出它所在的文件夹目录
        QString videoDir = fileInfo.absolutePath();

        // 把这个目录自动设置到导出的 lineEditAudio 中
        ui->lineEditAudio->setText(videoDir);

        ui->txtLog->append("📁 已自动将导出目录同步为视频所在文件夹: " + videoDir);
    }

}


void AudioExtractorWindow::on_btnExtractAudio_clicked()
{
    //读取所有被添加进来的视频路径
    QStringList videoFiles;
    for (int i = 0; i < ui->listVideos->count(); ++i) {
        videoFiles << ui->listVideos->item(i)->text();
    }

    QStringList urlLines = ui->textEditUrls->toPlainText().split(QRegularExpression("\\r?\\n"), Qt::SkipEmptyParts);
    for (QString url : urlLines) {
        url = url.trimmed();
        if (!url.isEmpty()) {
            videoFiles << url;
        }
    }

    QString outputDir = ui->lineEditAudio->text();

    // 判断是否有效
    if (videoFiles.isEmpty()) {
        ui->txtLog->append("⚠️ 错误：请先添加本地视频文件或填写 URL！");
        return;
    }
    if (outputDir.isEmpty()) {
        ui->txtLog->append("⚠️ 错误：请选择音频输出目录！");
        return;
    }

    if (batchIsRunning()) {
        ui->txtLog->append("⚠️ 批量提取正在进行中，请等待结束或关闭窗口中止任务。");
        return;
    }

    ui->txtLog->append(QString("🎬 开始批量处理，共 %1 个文件...").arg(videoFiles.size()));
    ui->btnExtractAudio->setEnabled(false); // 禁用按钮防止重复点击
    m_closeAfterCancel = false;

    // 创建 Qt 多线程
    QThread* thread = new QThread(this);
    BatchExtractorWorker* worker = new BatchExtractorWorker(videoFiles, outputDir, ui->textEditHeaders->toPlainText());
    m_batchThread = thread;
    m_batchWorker = worker;
    worker->moveToThread(thread);

    // 线程启动时，让 worker 开始工作
    connect(thread, &QThread::started, worker, &BatchExtractorWorker::startBatchProcess);

    // 更新主界面的 UI
    connect(worker, &BatchExtractorWorker::logMessage, ui->txtLog, &QTextEdit::append);

    connect(worker, &BatchExtractorWorker::progressUpdated, this, [=](int current, int total){
    });

    // 结束后自动清理线程和内存，恢复按钮状态
    connect(worker, &BatchExtractorWorker::finishedAll, this, [=](){
        ui->btnExtractAudio->setEnabled(true); // 恢复按钮
    });

    // 销毁线程
    connect(worker, &BatchExtractorWorker::finishedAll, thread, &QThread::quit);
    connect(worker, &BatchExtractorWorker::finishedAll, worker, &BatchExtractorWorker::deleteLater);
    connect(thread, &QThread::finished, this, [=](){
        if (m_batchThread == thread) {
            m_batchThread.clear();
        }
        if (m_batchWorker == worker) {
            m_batchWorker.clear();
        }

        if (m_closeAfterCancel) {
            m_closeAfterCancel = false;
            QTimer::singleShot(0, this, &QWidget::close);
        }
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    // 线程启动
    thread->start();
}


void AudioExtractorWindow::on_audio_route_change_clicked()
{
    QString defaultPath = ui->lineEditAudio->text();
    if (defaultPath.isEmpty()) {
        defaultPath = "/Users/tanyixiao/Downloads";
    }

    QString customDir = QFileDialog::getExistingDirectory(
        this,
        "选择音频批量输出目录",
        defaultPath
        );

    if (!customDir.isEmpty()) {
        ui->lineEditAudio->setText(customDir); // 此时 lineEditAudio 存的是目录路径了
        ui->txtLog->append("📝 批量输出目录更改为: " + customDir);
    }
}

