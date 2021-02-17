#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#define CMD_STATUS          0x00
#define	CMD_RESET           0xB1

#define CMD_READ_BUFFER     0xB2
#define CMD_WRITE_PAGE      0xB3
#define CMD_CALC_CS_GET_PREV		0xB4

#define	CMD_REBOOT          0xB5
#define	CMD_EXIT            0xB6
#define CMD_COPY            0xB7
#define CMD_CLEAR_FULL		0xB8	// erase full flash
#define CMD_CLEAR_0_63		0xB9	// erase first half of flash
#define CMD_CLEAR_64_128	0xBA	// erase second half of flash
//#define CMD_ERASE_PAGE	0xBF // erase SINGLE page



#endif /* PROTOCOL_H_ */