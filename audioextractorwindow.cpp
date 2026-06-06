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
#include "batchextractorworker.h" // 记得引入刚才写的新头文件

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
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        "选择要分离音频的视频文件（可多选）",
        "",
        "视频文件 (*.mp4 *.mkv *.avi *.mov *.flv)"
        );

    if (!files.isEmpty()) {
        ui->listVideos->addItems(files); // 直接把选中的多个路径塞进列表
        ui->txtLog->append(QString("📝 已添加 %1 个视频到待处理列表").arg(files.size()));

        // 获取用户选择的第一个视频的绝对路径（例如：/Users/tanyixiao/Movies/test.mp4）
        QString firstVideoPath = files.first();

        // 使用 QFileInfo 分析这个路径
        QFileInfo fileInfo(firstVideoPath);

        // 提取出它所在的文件夹目录（例如：/Users/tanyixiao/Movies）
        QString videoDir = fileInfo.absolutePath();

        // 把这个目录自动设置到导出的 lineEditAudio 中
        ui->lineEditAudio->setText(videoDir);

        ui->txtLog->append("📁 已自动将导出目录同步为视频所在文件夹: " + videoDir);
    }

}


void AudioExtractorWindow::on_btnExtractAudio_clicked()
{
    // 1. 从你的 QListWidget 中读取所有被添加进来的视频路径
    QStringList videoFiles;
    for (int i = 0; i < ui->listVideos->count(); ++i) {
        videoFiles << ui->listVideos->item(i)->text();
    }

    QString outputDir = ui->lineEditAudio->text();

    // 判断是否有效
    if (videoFiles.isEmpty()) {
        ui->txtLog->append("⚠️ 错误：请先往列表中添加视频文件！");
        return;
    }
    if (outputDir.isEmpty()) {
        ui->txtLog->append("⚠️ 错误：请选择音频输出目录！");
        return;
    }

    ui->txtLog->append(QString("🎬 开始批量处理，共 %1 个文件...").arg(videoFiles.size()));
    ui->btnExtractAudio->setEnabled(false); // 禁用按钮防止重复点击

    // 2. 创建标准的 Qt 多线程
    QThread* thread = new QThread(this);
    BatchExtractorWorker* worker = new BatchExtractorWorker(videoFiles, outputDir);
    worker->moveToThread(thread);

    // 3. 核心连接：线程启动时，让 worker 开始工作
    connect(thread, &QThread::started, worker, &BatchExtractorWorker::startBatchProcess);

    // 4. 连接 Worker 传回来的信号，更新主界面的 UI
    connect(worker, &BatchExtractorWorker::logMessage, ui->txtLog, &QTextEdit::append);

    connect(worker, &BatchExtractorWorker::progressUpdated, this, [=](int current, int total){
        // 如果你有进度条（QProgressBar），可以在这里更新它
        // ui->progressBar->setMaximum(total);
        // ui->progressBar->setValue(current);
    });

    // 5. 结束后自动清理线程和内存，恢复按钮状态
    connect(worker, &BatchExtractorWorker::finishedAll, this, [=](){
        ui->btnExtractAudio->setEnabled(true); // 恢复按钮
    });

    // 这三行是 Qt 线程安全销毁的标准公式：
    connect(worker, &BatchExtractorWorker::finishedAll, thread, &QThread::quit);
    connect(worker, &BatchExtractorWorker::finishedAll, worker, &BatchExtractorWorker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    // 6. 线程启动
    thread->start();
}


void AudioExtractorWindow::on_audio_route_change_clicked()
{
    QString defaultPath = ui->lineEditAudio->text();
    if (defaultPath.isEmpty()) {
        defaultPath = "/Users/tanyixiao/Downloads";
    }

    // 💡 弹出“选择文件夹”对话框
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

