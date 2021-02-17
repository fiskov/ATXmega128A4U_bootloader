#ifndef __HELPER_H
#define __HELPER_H

#define	FLASH_SIZE 0x20000
#define FLASH_FULL_SIZE (0x20000+8192)
#define FLASH_FW_START 0x10000
#define	FLASH_PAGE_SIZE 256
#define PAGE_SIZE 256
#define FLASH_LAST_PAGE_ADDR (FLASH_SIZE-FLASH_PAGE_SIZE)

#define CMD_STATUS          0x00
#define	CMD_RESET           0xB1

#define CMD_READ_BUFFER     0xB2
#define CMD_WRITE_PAGE      0xB3
#define CMD_CALC_CS_GET_PREV		0xB4

#define	CMD_REBOOT          0xB5
#define	CMD_EXIT            0xB6
#define CMD_COPY            0xB7
#define CMD_CLEAR_FULL		0xB8	//очистить всю Flash-память
#define CMD_CLEAR_0_63		0xB9	//очистить первую половину Flash-памяти
#define CMD_CLEAR_64_128	0xBA	//очистить вторую половину Flash-памяти
//#define CMD_ERASE_PAGE	0xBF //стереть только ОДНУ страницу

#include "mainwindow.h"

class helper {

public:
    static void qSleep(int ms);

    static QString loadHex(QString fileName, uint8_t *bfr, size_t *size);
    static QString saveHex(QString fileName, uint8_t *bfr, size_t size);

    static size_t roundUp(size_t numToRound, size_t multiple);
    static QString toHex2(int value);
    static QString toHex4(int value);
    static QString toHex6(int value);
    static uint16_t ControlSum(uint8_t *bfr, uint32_t startAddr, uint8_t pagesCnt);
private:
    static QString getHexCC(QString line);
};

#endif
