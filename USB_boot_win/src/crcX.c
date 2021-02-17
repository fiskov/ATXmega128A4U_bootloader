#include "src/crcX.h"

/**************************************************************************************************
* XMEGA NVM compatible CRC32
*/
#define XMEGA_CRC32_POLY	0x0080001B	// Polynomial for use with Xmega devices

uint32_t xmega_nvm_crc32(uint8_t *buffer, uint16_t buffer_length)
{
	uint32_t	address;
	uint32_t	data_reg, help_a, help_b;
	uint32_t	crc_reg = 0;

	for (address = 0; address < buffer_length; address += 2)
	{
		help_a = crc_reg << 1;
		help_a &= 0x00FFFFFE;
		help_b = crc_reg & (1 << 23);

		if (help_b > 0)
			help_b = 0x00FFFFFF;

        data_reg = (uint32_t)(((buffer[address + 1] << 8) | buffer[address]) & 0xFFFF);

		crc_reg = (help_a ^ data_reg) ^ (help_b & XMEGA_CRC32_POLY);
		crc_reg = crc_reg & 0x00FFFFFF;
	}

	return crc_reg;
}
