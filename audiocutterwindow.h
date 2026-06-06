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

private:
    Ui::audiocutterwindow *ui;
};

#endif // AUDIOCUTTERWINDOW_H
