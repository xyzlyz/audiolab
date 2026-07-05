#ifndef SILENCECUT_H
#define SILENCECUT_H
#include <QProcess>
#include <QRegularExpression>
#include <QVector>

#include <QWidget>

namespace Ui {
class silencecut;
}

class silencecut : public QWidget
{
    Q_OBJECT

public:
    explicit silencecut(QWidget *parent = nullptr);
    ~silencecut();

private slots:
    void on_audio_select_botton_clicked();

    void on_output_address_select_clicked();

    void on_start_cutting_clicked();

    void onCaptureSilenceLog();

    void onDetectFinished(int exitCode);

    void startActualCutting();

private:
    Ui::silencecut *ui;
    QProcess *detectProcess = nullptr;
    QVector<double> silenceStartPoints;
    QString m_ffmpegPath;
};

#endif // SILENCECUT_H
