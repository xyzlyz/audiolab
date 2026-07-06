#ifndef AUDIOEXTRACTORWINDOW_H
#define AUDIOEXTRACTORWINDOW_H

#include <QWidget>
#include <QPointer>
#include <QThread>
#include "batchextractorworker.h"

class QCloseEvent;

namespace Ui {
class AudioExtractorWindow;
}

class AudioExtractorWindow : public QWidget
{
    Q_OBJECT

public:
    explicit AudioExtractorWindow(QWidget *parent = nullptr);
    ~AudioExtractorWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_btnSelectVideo_clicked();

    void on_btnExtractAudio_clicked();

    void on_audio_route_change_clicked();

private:
    bool batchIsRunning() const;
    void requestBatchCancel();

    Ui::AudioExtractorWindow *ui;
    QPointer<QThread> m_batchThread;
    QPointer<BatchExtractorWorker> m_batchWorker;
    bool m_closeAfterCancel = false;
};

#endif // AUDIOEXTRACTORWINDOW_H
