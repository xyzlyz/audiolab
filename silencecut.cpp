#include "silencecut.h"
#include "ui_silencecut.h"
#include <QFileDialog>
#include <QStandardPaths>

silencecut::silencecut(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::silencecut)
{
    ui->setupUi(this);
    setWindowTitle("智能静音分割工具");
}

silencecut::~silencecut()
{
    delete ui;
}

void silencecut::on_audio_select_botton_clicked()
{

    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("选择要切分的音频文件"),
        "",
        tr("音频文件 (*.mp3 *.wav *.m4a *.flac);;所有文件 (*.*)")//音频格式可选，其他为灰
        );

    if (!filePath.isEmpty()) {
        // 显示输入路径
        ui->input_address->setText(filePath);
        ui->output_address->setText(QFileInfo(filePath).absolutePath());

        // 在TextLog中打印日志
        ui->TextLog->append(QString("<font color='#007AFF'><b>[输入]</b></font> 已选择音频：%1").arg(filePath));
        ui->TextLog->append("已自动生成输出路径: " + filePath);
    }
}


void silencecut::on_output_address_select_clicked()
{
    QString out_dirPath = QFileDialog::getExistingDirectory(
        this,
        tr("选择要存储的文件夹位置"),
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    if (!out_dirPath.isEmpty()) {
        // 把选好的文件夹路径显示到输入框里
        ui->output_address->setText(out_dirPath);
    }
}


void silencecut::on_start_cutting_clicked()
{
    QString input_address = ui->input_address->text();
    QString output_address = ui->output_address->text();

    //确保一定选择了音频
    if(input_address.isEmpty()){
        ui->TextLog->append("请选择音频");
    }
    else{
        //找到ffmpeg的路径
        QString ffmpegPath = QStandardPaths::findExecutable(
            "ffmpeg",
            { "/usr/local/bin", "/opt/homebrew/bin", "C:/ffmpeg/bin" }
            );

        if (ffmpegPath.isEmpty()) {
            ui->TextLog->append("❌ 未找到 FFmpeg，请确认已安装并配置到 PATH。");
            return;
        }

        m_ffmpegPath = ffmpegPath;

        ui->TextLog->append("<b>[第一步]</b> 正在寻找扫描静音区域，请稍候...");
        silenceStartPoints.clear();

        detectProcess = new QProcess(this);

        connect(detectProcess, &QProcess::readyReadStandardError, this, &silencecut::onCaptureSilenceLog);

        connect(detectProcess, &QProcess::finished, this, [this](int exitCode) {
            this->onDetectFinished(exitCode);
        });

        QStringList arguments;
        arguments << "-i" << input_address
                  << "-af" << "silencedetect=noise=-35dB:d=0.5" //判定标准：低于-30分贝且持续0.5秒以上
                  << "-f" << "null"
                  << "-";

        detectProcess->start(ffmpegPath, arguments);
    }
}

void silencecut::onCaptureSilenceLog()
{
    if (!detectProcess) return;

    // 存储ffmpeg输出
    QString rawLog = QString::fromUtf8(detectProcess->readAllStandardError());

    // 正则表达式提取时间
    QRegularExpression regex("silence_start: (\\d+\\.\\d+)");
    QRegularExpressionMatchIterator it = regex.globalMatch(rawLog);

    // 循环匹配
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();

        double timePoint = match.captured(1).toDouble();

        silenceStartPoints.append(timePoint);

        ui->TextLog->append(QString("📍 <font color='#007AFF'><b>自动捕获静音点：</b></font> 在第 %1 秒处发现静音").arg(timePoint));
    }
}

void silencecut::onDetectFinished(int exitCode)
{
    ui->TextLog->append("<br>🎉 <b>[扫描完成]</b>");

    if (silenceStartPoints.isEmpty()) {
        ui->TextLog->append("<font color='#FF9500'>提示：这段音频里好像非常连贯，没有发现明显的静音期。</font>");
    } else {
        ui->TextLog->append(QString("<font color='#34C759'><b>扫描完毕：</b></font> 整个音频共发现 <b>%1</b> 个静音切分点。")
                               .arg(silenceStartPoints.size()));

        ui->TextLog->append("下一步：我们将根据这些时间点，开始全自动批量裁剪音频。");

        startActualCutting();
    }

    // 释放内存
    if (detectProcess) {
        detectProcess->deleteLater();
        detectProcess = nullptr;
    }
}

void silencecut::startActualCutting()
{
    QString inputPath = ui->input_address->text();
    QString outputDir = ui->output_address->text();

    // 准备时间切片阵列
    QVector<double> cuts = silenceStartPoints;
    cuts.prepend(0.0); // 塞入起点

    // 开始循环裁剪
    for (int i = 0; i < cuts.size(); ++i) {
        double startTime = cuts[i];

        // 自动生成输出文件名
        QString fileName = QString("split_part_%1.mp3").arg(i + 1);
        QString outputFileName = QDir(outputDir).filePath(fileName);

        QProcess *cutter = new QProcess(this);
        QStringList args;

        args << "-ss" << QString::number(startTime);

        if (i < cuts.size() - 1) {
            double endTime = cuts[i + 1];

            // 过滤靠得太近的连续静音点
            if (endTime - startTime < 1.5) {
                cutter->deleteLater();
                continue;
            }

            args << "-to" << QString::number(endTime + 1.5);
        }

        args << "-i"       << inputPath
             << "-vn"
             << "-acodec"  << "libmp3lame"
             << "-q:a"     << "2"
             << "-y"       << outputFileName;

        // 打印日志
        if (i < cuts.size() - 1) {
            ui->TextLog->append(QString("✂️ 正在裁剪第 %1 段：[%2秒 -> %3秒]...")
                                    .arg(i + 1).arg(startTime).arg(cuts[i + 1]));
        } else {
            ui->TextLog->append(QString("✂️ 正在裁剪最后一段：[%1秒 -> 结尾]...").arg(startTime));
        }

        cutter->start(m_ffmpegPath, args);
        cutter->waitForFinished();

        //过滤音频结束后的空片段
        QFileInfo checkFile(outputFileName);
        if (checkFile.exists()) {
            if (checkFile.size() < 1024) { // 小于 1KB 判定为无效
                QFile::remove(outputFileName);
                ui->TextLog->append(QString("<font color='#FF9500'>⚠️ 过滤空白/无效片段: %1</font>").arg(fileName));
            } else {
                ui->TextLog->append(QString("<font color='#34C759'>✅ 成功生成: %1 (%2 KB)</font>")
                                        .arg(fileName).arg(checkFile.size() / 1024));
            }
        }

        cutter->deleteLater();
    }

    ui->TextLog->append("<br><font color='#34C759'><b>剪辑完成！</b></font>");
}
