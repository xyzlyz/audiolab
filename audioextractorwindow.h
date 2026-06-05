#ifndef AUDIOEXTRACTORWINDOW_H
#define AUDIOEXTRACTORWINDOW_H

#include <QWidget>

namespace Ui {
class AudioExtractorWindow;
}

class AudioExtractorWindow : public QWidget
{
    Q_OBJECT

public:
    explicit AudioExtractorWindow(QWidget *parent = nullptr);
    ~AudioExtractorWindow();

private slots:
    void on_btnSelectVideo_clicked();

    void on_btnExtractAudio_clicked();

private:
    Ui::AudioExtractorWindow *ui;
};

#endif // AUDIOEXTRACTORWINDOW_H
