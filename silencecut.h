#ifndef SILENCECUT_H
#define SILENCECUT_H

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

private:
    Ui::silencecut *ui;
};

#endif // SILENCECUT_H
