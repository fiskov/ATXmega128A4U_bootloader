#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QTimer>
#include "src/hidapi.h"
#include "src/helper.h"
#include "src/crcX.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void logAdd(QString message);
    void logEndData(int err);
    hid_device * USBOpen(uint16_t VID, uint16_t PID);
    bool USBCheck(hid_device * handle);

private slots:
    void on_btnSend_clicked();
    void on_btnOpen_clicked();
    void slotTimerAlarm();

    void on_btnGet_clicked();

    void on_btnReboot_clicked();

    void on_btnExitBoot_clicked();

    void on_btnClear_clicked();

    void showContextMenu(const QPoint &pos);

    void on_btnLoad_clicked();

private:
    Ui::MainWindow *ui;
    QTimer *timer;

protected:
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *e);
};
#endif // MAINWINDOW_H
