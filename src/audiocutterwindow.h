#ifndef AUDIOCUTTERWINDOW_H
#define AUDIOCUTTERWINDOW_H

#include <QWidget>

namespace Ui {
class audiocutterwindow;
}

class audiocutterwindow : public QWidget
{
    Q_OBJECT

public:
    explicit audiocutterwindow(QWidget *parent = nullptr);
    ~audiocutterwindow();

    // 外部窗口可调用：设置待剪辑音频，并自动生成默认输出路径
    void setInputAudioPath(const QString &audioPath);

private slots:
    void on_btnSelectVideo_clicked();

    void on_btnExtractAudio_clicked();

    void on_audio_route_change_clicked();

private:
    Ui::audiocutterwindow *ui;
};

#endif // AUDIOCUTTERWINDOW_H
