// author Fiskov Vladimir  fiskov@gmail.com
// basis is taken from  https://github.com/kuro68k/kboot
// ATXmega128A4 

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>
#include <asf.h>
#include <avr/wdt.h>
#include "common_defines.h"
#include "common_types.h"
#include "sp_driver.h"
#include "protocol.h"

static uint8_t page_buffer[PAGE_SIZE], page_buffer_pos = 0;
static uint8_t feature_response[UDI_HID_REPORT_FEATURE_SIZE] = {0};
	
#define LED_MCU_SET(X) do{ PORTE.OUTSET = PIN1_bm; } while(0)
#define LED_MCU_CLEAR(X) do{ PORTE.OUTCLR = PIN1_bm; } while(0)
#define LED_MCU_TOGGLE(X) do{ PORTE.OUTTGL = PIN1_bm; } while(0)

static uint16_t ControlSum(uint32_t startAddr, uint8_t pagesCnt);
static void eraseFlash(uint8_t eraseType);
static void checkAndWriteNewFirmware(void);
static void exitBootloader(void);

static int8_t exitTmr = 20, USB_SOF_Cnt=0, USB_SOF_Check=0;
static uint32_t csStartAddr = 0, csPagesCnt = 0;
static uint16_t csResult = 0;
flags_struct_t flags = {0};

int main(void)
{
	PORTE.DIRSET = PIN1_bm; // LED_MCU
	
	CCP = CCP_IOREG_gc;				// unlock IVSEL
	PMIC.CTRL |= PMIC_IVSEL_bm;		// set interrupt vector table to bootloader section	
		
	sysclk_init();
	irq_initialize_vectors();
	cpu_irq_enable();
	
	udc_start();
	udc_attach();
	wdt_disable();
	_delay_ms(400); //time for detecting USB-connection
	for(;;) { 		
		LED_MCU_TOGGLE();   _delay_ms(200);
		
		//Exit condition : timeout; command; USB not connected
		if (--exitTmr <= 0 || flags.restart || !USB_SOF_Check) 
			exitBootloader(); 	
				
		USB_SOF_Check = 0;
		
		// long time functions
		if (flags.erase_0_63)	{ eraseFlash(1);	flags.erase_0_63 = 0; }
		if (flags.erase_64_128) { eraseFlash(2);	flags.erase_64_128 = 0; }
		if (flags.eraseFull)	{ eraseFlash(0xFF);	flags.eraseFull = 0; }
		if (flags.copyFlash)	{ checkAndWriteNewFirmware(); flags.copyFlash = 0; }		
		if (flags.calcControlSum) {			
			csResult = ControlSum(csStartAddr, csPagesCnt); //5ms
			flags.calcControlSum = 0;
		}		
	}
}

// https://habr.com/ru/post/278171/
static uint16_t ControlSum(uint32_t startAddr, uint8_t pagesCnt){
	uint16_t cs=0;

	while (pagesCnt--) {
		memcpy_PF(page_buffer, (uint_farptr_t)(startAddr), PAGE_SIZE);
		startAddr += PAGE_SIZE;
		
		for (uint16_t i=0; i<PAGE_SIZE; i++){
			cs += page_buffer[i]*44111;
			cs = cs ^ (cs >> 8);
		}
	}
	return cs;
}

static void exitBootloader(void){// exit bootloader
	LED_MCU_CLEAR();
	udc_detach();
	udc_stop();
	
	CCP = CCP_IOREG_gc;		// unlock IVSEL
	PMIC.CTRL = 0;			// disable interrupts, set vector table to app section
	EIND = 0;				// indirect jumps go to app section
	RAMPZ = 0;				// LPM uses lower 64k of flash
	_delay_ms(500);	//wait USB-detaching
	
	void (*jump) (void) = (void (*)(void))(0);
	jump();
}

// flash erasing
static void eraseFlash(uint8_t eraseType) {
	uint32_t adr = 0;
	uint8_t cnt = 0;
	
	switch (eraseType) {
		default: break;
		
		case 2: adr = 0x10000; // second half
		
		case 1: // first half
		do {
			// erase 256 pages (256*256=65536 bytes)
			SP_EraseApplicationPage(adr);
			SP_WaitForSPM();
			adr += 256;
		} while (--cnt);
		break;
		
		case 0xFF: // full flash
		SP_EraseApplicationSection();
		break;
	}
}

// copy new program to place of main program 
static void checkAndWriteNewFirmware(void){
	
	// Last page contains: writing flag, size, checkSum
	memcpy_PF(page_buffer, (uint_farptr_t)(APP_SECTION_SIZE - PAGE_SIZE),
	sizeof(page_buffer));
	
	if (page_buffer[0] == 0) {
		uint16_t size = (page_buffer[1] << 8) + page_buffer[2];
		uint16_t cs = (page_buffer[3] << 8) + page_buffer[4];
		
		// verify checkSum of new program
		if (cs != ControlSum(FLASH_NEW_START, size >> 8)) return;
		
		// erase+prepare first half of flash-memory
		eraseFlash(1);
		
		// read and write by page
		for (uint16_t adr=0; adr < size; adr += 256) {
			memcpy_PF(page_buffer, (uint_farptr_t)(FLASH_NEW_START+adr), PAGE_SIZE);
			
			SP_LoadFlashPage(page_buffer);
			SP_WriteApplicationPage(adr);
			SP_WaitForSPM();
		}
		
		// verify checkSum main program
		if (cs != ControlSum(0, size >> 8)) return;
		
		// if all ok, then write flag
		memset(page_buffer, 0xFF, PAGE_SIZE);
		page_buffer[0] = 0xAA;
		page_buffer[1] = size >> 8;
		page_buffer[2] = size;
		page_buffer[3] = cs >> 8;
		page_buffer[4] = cs;
		
		SP_LoadFlashPage(page_buffer);
		SP_EraseWriteApplicationPage( APP_SECTION_SIZE - PAGE_SIZE );
		SP_WaitForSPM();
	}
}


// all data from report_out write to pageBuffer (it will be write to flash_page by command)
void HID_report_out(uint8_t *report) {
	memcpy(&page_buffer[ page_buffer_pos ], report, UDI_HID_REPORT_OUT_SIZE);
	page_buffer_pos += 0x40; // 0, 64, 128, 192 bytes
}


// query feature-report
bool HID_get_feature_report_out(uint8_t **payload, uint16_t *size) {
	exitTmr = 20; // timeout 20*0.2 s
	
	uint8_t status = 0xB0;
	//non-volatile memory controller is busy
	if (NVM.STATUS & (~NVM_BUSY_bits)) 	status |= 0x01;			
	if (flags.calcControlSum)		status |= 0x02;			
	if (flags.erase_0_63 || flags.erase_64_128 || flags.eraseFull || flags.copyFlash)
		status |= 0x04;
			
	feature_response[0] = status;
	feature_response[1] = csResult >> 8;
	feature_response[2] = csResult;
	feature_response[3] = USB_SOF_Cnt;
	
	*payload = feature_response;
	*size = sizeof(feature_response);
	return true;
}

// feature-report processing
void HID_set_feature_report_out(uint8_t *report) {
	static uint8_t response[UDI_HID_REPORT_OUT_SIZE] = {0};
	cmd_t *cmd = (cmd_t *)report;
	LED_MCU_TOGGLE(); 	
	union_uint_t addr = {.u8[3]=0, 
						 .u8[2] = report[1],
						 .u8[1] = report[2],
						 .u8[0] = report[3] };
		
	switch( cmd->id )
	{
		case CMD_STATUS:
			break;
			
		// запись массива page_buffer в PageBuffer и сохранение во флеш-память
		case CMD_WRITE_PAGE:			
			SP_LoadFlashPage(page_buffer);
			SP_WriteApplicationPage( (uint32_t)addr.u32 );
			SP_WaitForSPM();
			// clear buffer after writing
		case CMD_RESET:	// reset buffer
			page_buffer_pos = 0;	
			memset(page_buffer, 0xFF, sizeof(page_buffer));								
			break;
			
		// read 64 bytes from flash at specified address
		case CMD_READ_BUFFER:
			memcpy_PF(response, (uint_farptr_t)(addr.u32), sizeof(response));
			break;
			
		// get checkSum of flash (address shows the end of range)
		// retrun last checkSum - it is also in feature_report
		case CMD_CALC_CS_GET_PREV:
			response[0] = csResult >> 8;
			response[1] = csResult;
			csStartAddr = FLASH_NEW_START;
			
			csPagesCnt = report[1]; // pages count of checkSum
			flags.calcControlSum = 1;
			break;	

		case CMD_REBOOT:
			// reset MCU by WatchDog
			LED_MCU_SET();
			ccp_write_io((void *)&WDT.CTRL, WDT_PER_128CLK_gc | WDT_WEN_bm | WDT_CEN_bm);				
			break;
			
		case CMD_EXIT:
			flags.restart = 1;
			break;
			
		case CMD_COPY:
			// copy new program to main program place
			flags.copyFlash = 1;
			break;
			
		case CMD_CLEAR_FULL:
			// erase full flash
			flags.eraseFull = 1;
			break;
			
		case CMD_CLEAR_0_63:
			// erase first half of flash
			flags.erase_0_63 = 1;
		break;		
			
		case CMD_CLEAR_64_128:
			// erase second half of flash
			flags.erase_64_128 = 1;
		break;			

		// unknown command
		default:
			break;
	}

	udi_hid_generic_send_report_in(response);	
}

void user_callback_sof_action(void){
	USB_SOF_Cnt++;
	USB_SOF_Check = 1;
}

	
