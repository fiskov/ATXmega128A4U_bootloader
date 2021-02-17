#include "helper.h"
#include <QFile>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h> // for Sleep
#endif

QString helper::toHex2(int value) { return QString("%1").arg(value, 2, 16, QLatin1Char('0')).toUpper(); }
QString helper::toHex4(int value) { return QString("%1").arg(value, 4, 16, QLatin1Char('0')).toUpper(); }
QString helper::toHex6(int value) { return QString("%1").arg(value, 6, 16, QLatin1Char('0')).toUpper(); }


void helper::qSleep(int ms)
{
#ifdef Q_OS_WIN
    Sleep(uint(ms));
#else
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
    nanosleep(&ts, NULL);
#endif
}

//чтение формата intelHex
QString helper::loadHex(QString fileName, uint8_t *bfr, size_t *sizeOut)
{
    size_t size = 0;
    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
       return ( "Ошибка загрузки : " + file.errorString() );

    QTextStream in(&file);

    QString line;
    bool ok, endWhile=false;
    uint32_t offset = 0, addr, len;
    memset(bfr, 0xFF, 0xFFFF);

    while (in.readLineInto(&line) && endWhile==false) {
       if (line[0]==':') {
           switch ( line.mid(7,2).toInt(&ok, 16) ) {
           case 0:
               addr = line.mid(3,4).toInt(&ok, 16);
               len = line.mid(1,2).toInt(&ok, 16);

               //если адрес больше, чем 65 кБ, то выходим
               if (offset + addr < 0xFFFF-0x1FF) {
                   size_t sizeCurrent = offset + addr + len - 1;
                   if (sizeCurrent > size) size = sizeCurrent;

                   for (uint32_t i=0; i<len; i++)
                       bfr[offset + addr + i]=line.mid(9 + i * 2, 2).toInt(&ok, 16);
               } else endWhile=true;

               break;
           case 2:
               offset = line.mid(9, 4).toInt(&ok, 16) * 16;
               break;
           }
       }
    }

    //округление до 256 в большую сторону
    size = roundUp(size, FLASH_PAGE_SIZE);
    *sizeOut = size;

    return  ( QString("Загружен файл размером %2 байт = %3 страниц \n[ %1 ]").
              arg(fileName).arg(size).arg(size/256) );
}


//контрольная сумма (255-)
QString helper::getHexCC(QString line){
    bool ok;
    uint8_t cc;

    cc = 0;
    for (int i=1; i<line.length(); i+=2) {
        cc -= line.mid(i,2).toInt(&ok, 16);
    }

    return  toHex2(cc);
}


QString helper::saveHex(QString fileName, uint8_t *bfr, uint32_t size){
    QFile file(fileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
       return ( "Ошибка сохранения : " + file.errorString() );

    QTextStream out(&file);

    QString line;
    uint32_t addr = 0;

    while (addr < size){
        //offset
        if (addr > 0 && addr % 0x10000 == 0) {
            line = ":02000002" + toHex4(addr/16);
            out << line.toUpper() << getHexCC(line) << endl;
        }

        //data (16 байт в строке)  :LLAAAATTDD...CC
        int cnt = 0, addrStart = addr & 0xFFFF;
        line = "";
        while (addr <= size && cnt++ < 16)
            line += toHex2(bfr[addr++]);

        line = ":" + toHex2(cnt-1) + toHex4(addrStart) + "00" + line;

        out << line << getHexCC(line) << endl;
    }

    //endfile
    out << ":00000001FF" << endl;

    return  ( QString("Сохранено в файл [%1], размер %2 Байт = %3 страниц").
              arg(fileName).arg(size).arg(size/256) );
}

//функция округляет в большую сторону до ближашего multiple (например, кратное 256)
size_t helper::roundUp(size_t numToRound, size_t multiple)
{
    assert(multiple);
    uint32_t rem = numToRound % multiple;

    if (rem > 0)
        return ((numToRound - rem)/multiple + 1) * multiple;
    return numToRound;
}


uint8_t page_buffer[PAGE_SIZE]={0};

uint16_t helper::ControlSum(uint8_t *bfr, uint32_t startAddr, uint8_t pagesCnt){
    uint16_t cs=0;

    while (pagesCnt--) {
        memcpy(page_buffer,(uint8_t *) &bfr[startAddr], PAGE_SIZE);
        startAddr += PAGE_SIZE;

        for (uint16_t i=0; i<PAGE_SIZE; i++){
          cs += page_buffer[i]*44111;
          cs = cs ^ (cs >> 8);
        }
    }
    return cs;
}
