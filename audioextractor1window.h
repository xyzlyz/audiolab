#ifndef AUDIOEXTRACTOR1WINDOW_H
#define AUDIOEXTRACTOR1WINDOW_H

#include <QWidget>
#include <QPointer>
#include <QProcess>

class QCloseEvent;

namespace Ui {
class AudioExtractor1Window;
}

class AudioExtractor1Window : public QWidget
{
    Q_OBJECT

public:
    explicit AudioExtractor1Window(QWidget *parent = nullptr);
    ~AudioExtractor1Window();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_btnSelectVideo_clicked();

    void on_btnExtractAudio_clicked();

    void on_audio_route_change_clicked();

private:
    void stopActiveExtraction();

    Ui::AudioExtractor1Window *ui;
    QPointer<QProcess> m_ffmpegProcess;
    bool m_isStopping = false;
};

#endif // AUDIOEXTRACTOR1WINDOW_H
