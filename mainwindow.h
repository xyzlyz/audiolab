#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "audioextractorwindow.h"
#include "audiocutterwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:

    void on_To_audioextract_clicked();

    void on_To_ffmpeg_test_clicked();

    void on_To_audiocut_clicked();

private:
    Ui::MainWindow *ui;
    void testFFmpeg();

    AudioExtractorWindow *extractorWin = nullptr;
    audiocutterwindow * cutterWin = nullptr;
};
#endif // MAINWINDOW_H
