#ifndef AUDIOEXTRACTOR1WINDOW_H
#define AUDIOEXTRACTOR1WINDOW_H

#include <QWidget>

namespace Ui {
class AudioExtractor1Window;
}

class AudioExtractor1Window : public QWidget
{
    Q_OBJECT

public:
    explicit AudioExtractor1Window(QWidget *parent = nullptr);
    ~AudioExtractor1Window();

private slots:
    void on_btnSelectVideo_clicked();

    void on_btnExtractAudio_clicked();

    void on_audio_route_change_clicked();

private:
    Ui::AudioExtractor1Window *ui;
};

#endif // AUDIOEXTRACTOR1WINDOW_H
