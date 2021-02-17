#ifndef __COMMMON_TYPES_H
#define __COMMMON_TYPES_H

typedef union union_uint_t {uint8_t u8[4]; uint16_t u16[2]; uint32_t u32;} union_uint_t;
typedef struct cmd_t {uint8_t	id; union_uint_t param;} cmd_t;
	
typedef struct flags_struct_t {
	uint8_t restart;
	uint8_t calcControlSum; 
	uint8_t eraseFull;
	uint8_t erase_0_63;
	uint8_t erase_64_128;	
	uint8_t copyFlash;
} flags_struct_t;
	
#endif /*__COMMMON_TYPES_H */
