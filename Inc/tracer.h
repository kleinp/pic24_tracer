/*
 * tracer.h
 *
 *  Created on: Sep 22, 2018
 *      Author: peter
 */

#ifndef TRACER_H_
#define TRACER_H_

#include "stm32l4xx_hal.h"

typedef struct {
	union {
		uint32_t flags;
		struct {
			uint32_t cmd_rx_low :1;	// there is a command to parse in cmd_rx[0-5]
			uint32_t cmd_rx_high :1;// there is a command to parse in cmd_rx[6-12]
			uint32_t tracer_rx_low :1;	// first half of tracer_rx is full
			uint32_t tracer_rx_high :1;	// second half of tracer_rx is full
			uint32_t timer_overflow :1;	// timestamp timer overflow occurred

			uint32_t trace_enable :1;	// enable trace
			uint32_t trace_disable :1;	// disable trace
			uint32_t trace_on_off :1;	// 0=off, 1=on
			uint32_t cmd_tx_busy :1;// currently busy transmitting data to CMD
		};
	};
} TRACE_FLAGS;

/**
 * Received command structure
 * sync 	0xAA
 * reg	 	0 - reserved
 * 			1 - enable
 * 			2 - disable
 * 			3 - trace baud rate
 * 			4 - trace buffer size
 * 			254 - echo!
 * val		32-bit value
 */
#pragma pack (1)
typedef struct {
	union {
		uint8_t raw[6];
		struct {
			uint8_t sync;
			uint8_t reg;
			uint32_t val;
		};
	};
} CMD_PACKET;

typedef union {
	uint8_t byte[4];
	uint32_t word;
} WORD_VAL;

/**
 * Transmitted data structure
 * sync 	0xAA
 * length	16-bit packet data length (i.e not including sync, lenght, or CRC)
 * counter	8-bit incrementing value
 * type		0 - reserved
 * 			1 - timer overflow
 * 			2 - trace data
 * 			254 - echo
 * data		<variable>
 * CRC		32-bit CRC
 */

void tracer(TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2,
		UART_HandleTypeDef *huart1, UART_HandleTypeDef *huart2,
		CRC_HandleTypeDef *hcrc);

void process_cmd(uint8_t *cmd);
void trace_enable(void);
void trace_disable(void);
void process_trace(uint8_t *data);
void process_overflow(void);
void send_packet(uint8_t type, uint16_t length);
uint32_t append_crc(uint32_t packet_length);

// Interrupts
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

#endif /* TRACER_H_ */
