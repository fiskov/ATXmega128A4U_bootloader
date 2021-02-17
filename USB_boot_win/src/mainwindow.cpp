#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <math.h>
#include "helper.h"
#include <QElapsedTimer>
#include <QApplication>
#include <QInputDialog>

#define	BUFFER_SIZE			(64+1)		// +1 for mandatory HID report ID
static uint8_t fw[FLASH_FULL_SIZE], bfr[BUFFER_SIZE], ftr[4+1], ftrIn[4+1];
static size_t fwSize=0;
bool isConnected = false, isBusy = false, isSending = false;

#define VID 0x03EB
#define PID 0xFFFE
hid_device *handle;
QLabel *lblConnection, *lblStatus;
QElapsedTimer tElapsed, tConnected;
QString fileName; //в него будет сохраняться считываемая прошивка

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setAcceptDrops(true);

    // устанавливаем специальную политику отображения меню
    ui->txtLog->setContextMenuPolicy(Qt::CustomContextMenu);
    // ждем сигнала для отображения меню
    connect(ui->txtLog, SIGNAL(customContextMenuRequested(QPoint)),
        this, SLOT(showContextMenu(QPoint)));

    lblConnection = new QLabel("Не подключено ");
    lblConnection->setMinimumWidth(150);

    lblStatus = new QLabel("00 00 00 00 00");
    lblStatus->setAlignment(Qt::AlignLeft);
    ui->statusbar->addWidget(lblConnection);
    ui->statusbar->addWidget(lblStatus, 1);
    ui->statusbar->setToolTip("Состояние подключение к USB (VID=0x03EB, PID=0xFFFE)");

    memset(fw, 0xFF, sizeof (fw));

    //таймер работы с USB
    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(slotTimerAlarm()));
    timer->start(250);

    //заполнение combobox с именами файлов
    QFile inputFile("pathFirmwares.txt");
    if (inputFile.open(QIODevice::ReadOnly))
    {
        QTextStream in(&inputFile);
        while (!in.atEnd())
            ui->cbFile->addItem(in.readLine());

        inputFile.close();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *e)
{
    QString fileName;
    foreach (const QUrl &url, e->mimeData()->urls()) {
        fileName = url.toLocalFile();
        ui->cbFile->setCurrentText(fileName);
    }

    MainWindow::on_btnLoad_clicked();
}

void MainWindow::showContextMenu(const QPoint &pos) {
  QPoint globalPos;
  if (sender()->inherits("QAbstractScrollArea"))
    globalPos = dynamic_cast<QAbstractScrollArea*>(sender())->viewport()->mapToGlobal(pos);
  else
    globalPos = dynamic_cast<QWidget*>(sender())->mapToGlobal(pos);

  QMenu menu;
  QAction *action1 = new QAction(tr("Очистить"), this);
  menu.addAction(action1);

  QAction *selectedItem = menu.exec(globalPos);

  if (selectedItem) {
      ui->txtLog->clear();
    }
}

void MainWindow::logAdd(QString message)
{
    ui->txtLog->appendPlainText(message);
}

void MainWindow::logEndData(int err){
    QString msg;
    if (err)
        msg = QString("Ошибка обмена USB. err=%1").arg(err);
    else
        msg = QString("Передача данных завершена. %1 мс").arg(tElapsed.elapsed());

    MainWindow::logAdd(msg);
}

//функция посылает страницу 256 байт
int sendPage(uint32_t addr, uint8_t data[256]){
    int res = 0;
    //запись в страничный буфер за 4 посылки по 64 байта
    bfr[0] = 0;
    for (int i=0; i<256; i+=64) {
        memcpy(&bfr[1], &data[i], 64);
        res = hid_write(handle, bfr, sizeof (bfr));
        if (res < 0) return -2;
    }

    //сохранить страницу
    ftr[1] = CMD_WRITE_PAGE;
    ftr[2] = addr >> 16;
    ftr[3] = addr >> 8;
    ftr[4] = addr;
    res = hid_send_feature_report(handle, ftr, sizeof(ftr));
    if (res < 0) return -3;

    return 0;
}

void MainWindow::on_btnSend_clicked()
{
    if (!handle) {logAdd("Устройство не подключено"); return; }

    int err = 0;

    tElapsed.restart();
    ui->progressBar->setValue(0);
    isSending = true;

    //очистка
    logAdd(tr("Очистка временной flash-памяти"));
    ftr[1] = CMD_CLEAR_64_128;    
    hid_send_feature_report(handle, ftr, sizeof(ftr));
    isBusy = true;
    do qApp->processEvents(); while (isBusy);

    //послать прошивку
    logAdd(tr("Отправка прошивки во временную память"));
    for (uint32_t addr = 0; addr < fwSize && err==0; addr += 256) {
        err = sendPage(addr + FLASH_FW_START, &fw[addr]);

        ui->progressBar->setValue( ceilf(100.0 * (addr+256) / fwSize) );
        qApp->processEvents();
    }

    //в последней странице флешки записать контрольную сумму и размер
    fw[FLASH_LAST_PAGE_ADDR] = 0; //флаг, что НОВАЯ прошивка
    fw[FLASH_LAST_PAGE_ADDR + 1] = fwSize >> 8;//два байта размер
    fw[FLASH_LAST_PAGE_ADDR + 2] = fwSize;

    uint16_t cs = helper::ControlSum(fw, 0, fwSize / FLASH_PAGE_SIZE);
    fw[FLASH_LAST_PAGE_ADDR + 3] = cs >> 8; //
    fw[FLASH_LAST_PAGE_ADDR + 4] = cs;

    err += sendPage(FLASH_LAST_PAGE_ADDR, &fw[FLASH_LAST_PAGE_ADDR]);
    isBusy = true;
    do qApp->processEvents(); while (isBusy);

    //скопировать прошивку
    logAdd("Копирование прошивки из временной в основную память устройства...");
    ftr[1] = CMD_COPY;
    hid_send_feature_report(handle, ftr, sizeof(ftr));
    isBusy = true;
    do qApp->processEvents(); while (isBusy);

    //всё закончено
    logEndData(err);
    isSending = false;
}

void MainWindow::on_btnOpen_clicked()
{
    QString fileNameOpen = QFileDialog::getOpenFileName(this,
        "Открыть файл прошивки", "", "hex-файл (*.hex);;All Files (*)");
    ui->cbFile->setCurrentText(fileNameOpen);

    MainWindow::on_btnLoad_clicked();
}

void MainWindow::slotTimerAlarm(){

    if (!ui->chkConnect->isChecked()) {
        isConnected = false;
    } else {
        static int errTime = 10;

        if (errTime ) { errTime--; return;}

        if (!isConnected) {
                handle = USBOpen(VID, PID);

            if (handle) {
                isConnected = true;
                tConnected.restart();
            } else errTime = 10;
        } else {
            if (!USBCheck(handle)) {
                isConnected = false;
                errTime = 30;

                logAdd(tr("Ошибка подключения (%1 c)").arg(tConnected.elapsed()/1000.0));
                logAdd("");
            }
        }
    }

    if (isConnected)
        lblConnection->setText("Подключено");
    else
        lblConnection->setText("Не подключено");
}

hid_device * MainWindow::USBOpen(uint16_t VID_, uint16_t PID_){
    // Открытие устройства, используя VID, PID и, опционально, серийный номер.
    hid_device * handle_ = hid_open(VID_, PID_, NULL);

    #define MAX_STR 255
    wchar_t wstr[MAX_STR];

    if (handle_) {
        // Чтение строки описания производителя (Manufacturer String)
        hid_get_manufacturer_string(handle_, wstr, MAX_STR);
        logAdd(QString("Manufacturer String: %1").arg(QString::fromWCharArray(wstr)));

        // Чтение строки описания продукта (Product String)
        hid_get_product_string(handle_, wstr, MAX_STR);
        logAdd(QString("Product String: %1").arg(QString::fromWCharArray(wstr)));
    }
    return handle_;
}



bool MainWindow::USBCheck(hid_device *handle){
    int cnt=0;

    //команды прием-отправка через feature, а прошивка отправка через Report
    memset(ftrIn, 0, sizeof (ftrIn));
    if (!isSending) {
        memset(ftr, 0, sizeof (ftr));
        if ( hid_send_feature_report(handle, ftr, sizeof(ftr))<0 ) return false;
    }

    cnt = hid_get_feature_report(handle, ftrIn, sizeof(ftrIn));

    QByteArray qa((char *)ftrIn, sizeof(ftrIn));
    isBusy = (qa[1] & 0xF);
    lblStatus->setText(qa.toHex(' ').toUpper() + (isBusy ? " занят" : ""));

    return (cnt >= 0);
}


void MainWindow::on_btnGet_clicked()
{
    isSending = true;
    logAdd("Считывание flash-памяти устройства");
    if (!handle) {logAdd("Устройство не подключено"); return; }

    uint8_t fwIn[FLASH_FULL_SIZE], bfr[65] = {0};
    fileName = QFileDialog::getSaveFileName(this,
        tr("Сохранение flash-памяти устройства"), "",
        tr("hex-файл (*.hex);;All Files (*)"));

    if (fileName.isEmpty())
        return;

    logAdd("[ "+fileName+" ]");
    lblStatus->setText("Ожидание");

    tElapsed.restart();
    memset(fwIn, 0xFF, sizeof(fwIn));
    QString s;

    int err = 0, restart = 0;
    uint32_t addr = 0;
    memset(ftr, 0, sizeof(ftr));
    ftr[1] = CMD_RESET;
    if ( hid_send_feature_report(handle, ftr, sizeof(ftr)) < 0) err++;

    // Прочитать flash-память по 64 байта
    while(addr < FLASH_FULL_SIZE && err==0) {
        ftr[1] = CMD_READ_BUFFER;
        ftr[2] = addr >> 16;
        ftr[3] = addr >> 8;
        ftr[4] = addr;
        int res = hid_send_feature_report(handle, ftr, sizeof(ftr));
        if (res < 0) {err = 10; break;}

        res = hid_read_timeout(handle, bfr, sizeof(bfr), 500);
        if (res < 0) {err = 11; break;}

        ui->progressBar->setValue( ceilf(100.0 * addr / FLASH_FULL_SIZE) );
        qApp->processEvents();

        memcpy(&fwIn[addr], bfr, res);
        addr += 64;

        //костыль, чтобы избежать нулей в начале считывания - повторно считываем с начала
        if (restart == 0 && addr >= 16*1024) {
            restart = 1;
            addr = 0;
        }
    }

    helper::saveHex(fileName, fwIn, FLASH_FULL_SIZE);

    logEndData(err);
    isSending = false;
}

void MainWindow::on_btnReboot_clicked()
{
    logAdd("Перезагрузка устройства...");
    if (handle) {
        ftr[1] = CMD_REBOOT;
        hid_send_feature_report(handle, ftr, sizeof(ftr));
    } else logAdd("Устройство не подключено");
    lblStatus->setText("Ожидание");
}

void MainWindow::on_btnExitBoot_clicked()
{
    logAdd("Выход из загрузчика...");
    if (handle) {
        ftr[1] = CMD_EXIT;
        hid_send_feature_report(handle, ftr, sizeof(ftr));
    } else logAdd("Устройство не подключено");
    lblStatus->setText("Ожидание");
}

void MainWindow::on_btnClear_clicked()
{
    logAdd("Очистка flash-памяти прошивки");
    if (handle) {
        ftr[1] = CMD_CLEAR_FULL;
        hid_send_feature_report(handle, ftr, sizeof(ftr));
    } else logAdd("Устройство не подключено");
    lblStatus->setText("Ожидание");
}

void MainWindow::on_btnLoad_clicked()
{
    QString fileName = ui->cbFile->currentText();
    if (!fileName.isEmpty()) {
        ui->cbFile->removeItem( ui->cbFile->findText(fileName) );
        ui->cbFile->insertItem(0, fileName);
        ui->cbFile->setCurrentIndex( 0 );

        QFile file("pathFirmwares.txt");
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream out(&file);

        for (int i = 0; i < ui->cbFile->count(); i++)
            out << ui->cbFile->itemText(i) << endl;

        file.close();

        logAdd(  helper::loadHex(fileName, fw, &fwSize) );
    }
}
