/*
 * tracer.c
 *
 *  Created on: Sep 22, 2018
 *      Author: peter
 */

#include "tracer.h"
#include <stdbool.h>

// flags for state machine
TRACE_FLAGS sm;

// UART receive buffer for trace packets
uint8_t trace_rx[10000];
// UART receive buffer for cmd packets
uint8_t cmd_rx[12];
// UART transmit buffer for trace data to CMD computer
uint8_t tx_buf[15010];
// command settable parameters
uint32_t trace_baud_rate = 921600;
uint32_t trace_buffer = 5000;

// TIM2 input capture buffer for trace timestamps
// There is a timestamp for every 2 bytes of receive data. Add a little as buffer
uint32_t trace_ts[5010];
uint32_t ts_idx = 0;

// pointers to peripherals
TIM_HandleTypeDef *tim1;
TIM_HandleTypeDef *tim2;
UART_HandleTypeDef *uart1;
UART_HandleTypeDef *uart2;
CRC_HandleTypeDef *crc;

void tracer(TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim2,
		UART_HandleTypeDef *huart1, UART_HandleTypeDef *huart2,
		CRC_HandleTypeDef *hcrc) {

	bool blink = false;

	// reset flags
	sm.flags = 0;

	// keep track of peripherals
	tim1 = htim1;
	tim2 = htim2;
	uart1 = huart1;
	uart2 = huart2;
	crc = hcrc;

	// Set up UART2 receive to circular DMA commands
	HAL_UART_Receive_DMA(uart2, &cmd_rx[0], 12);

	while (1) {

		// process incoming commands
		// any incoming commands will reset the tracer
		if (sm.cmd_rx_low) {
			process_cmd(&cmd_rx[0]);
			sm.cmd_rx_low = 0;
			blink = true;
		}
		if (sm.cmd_rx_high) {
			process_cmd(&cmd_rx[6]);
			sm.cmd_rx_high = 0;
			blink = true;
		}

		// enable/disable if commanded
		if (sm.trace_disable && sm.trace_on_off == 1) {
			trace_disable();
			sm.trace_on_off = 0;
			sm.trace_disable = 0;
		}
		if (sm.trace_enable && sm.trace_on_off == 0) {
			trace_enable();
			sm.trace_on_off = 1;
			sm.trace_enable = 0;
		}

		// process and send out trace info
		if (sm.tracer_rx_low) {
			process_trace(&trace_rx[0]);
			sm.tracer_rx_low = 0;
			blink = true;
		}
		if (sm.tracer_rx_high) {
			process_trace(&trace_rx[trace_buffer]);
			sm.tracer_rx_high = 0;
			blink = true;
		}

		// send out timer overflow message
		if (sm.timer_overflow) {
			process_overflow();
			sm.timer_overflow = 0;
			blink = true;
		}

		// several events can cause the LED to blink
		if (blink) {
			blink = false;
			HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
		}
	}

}

void process_cmd(uint8_t *cmd) {

	CMD_PACKET *packet;
	bool cycle = false;

	// overlay CMD_PACKET on raw data
	packet = (CMD_PACKET *) cmd;

	if (packet->sync != 0xAA) {
		return;
	}

	switch (packet->reg) {
	case 1:
		sm.trace_enable = 1;
		// if we are already on, reset
		if (sm.trace_on_off) {
			sm.trace_disable = 1;
		}
		break;
	case 2:
		sm.trace_disable = 1;
		break;
	case 3:
		trace_baud_rate = packet->val;
		cycle = true;
		break;
	case 4:
		trace_buffer = packet->val;
		cycle = true;
		break;
	case 254:
		while (sm.cmd_tx_busy)
			;
		tx_buf[5] = packet->raw[2];
		tx_buf[6] = packet->raw[3];
		tx_buf[7] = packet->raw[4];
		tx_buf[8] = packet->raw[5];
		send_packet(254, 4);
	default:
		break;
	}

	if (cycle) {
		// if trace is currently enabled, cycle it
		if (sm.trace_on_off) {
			sm.trace_disable = 1;
			sm.trace_enable = 1;
		}
	}

}

void trace_enable(void) {

	TIM_OnePulse_InitTypeDef tconf;

	// reset flags
	sm.tracer_rx_high = 0;
	sm.tracer_rx_low = 0;

	// set up TIM1 for one-pulse mode
	tconf.OCMode = TIM_OCMODE_TOGGLE;
	tconf.Pulse = 1;
	tconf.OCPolarity = TIM_OCPOLARITY_HIGH;
	tconf.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	tconf.OCIdleState = TIM_OCIDLESTATE_RESET;
	tconf.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	tconf.ICSelection = TIM_ICSELECTION_DIRECTTI;
	tconf.ICFilter = 0;
	HAL_TIM_OnePulse_ConfigChannel(tim1, &tconf, TIM_CHANNEL_1,
	TIM_CHANNEL_2);

	// configure tracer UART
	uart1->Init.BaudRate = trace_baud_rate;
	if (HAL_UART_Init(uart1) != HAL_OK) {
		_Error_Handler(__FILE__, __LINE__);
	}

	// configure TIM1 period based on baud rate of trace data
	// 20 bits (2 bytes)
	tim1->Init.Period = (20 * HAL_RCC_GetPCLK1Freq()) / trace_baud_rate;
	if (HAL_TIM_Base_Init(tim1) != HAL_OK) {
		_Error_Handler(__FILE__, __LINE__);
	}

	// start counting on TIM2
	HAL_TIM_Base_Start_IT(tim2);
	// set up TIM2 input capture for circular DMA
	HAL_TIM_IC_Start_DMA(tim2, TIM_CHANNEL_1, &trace_ts[0], 5010);

	// set up UART1 receive to circular DMA
	HAL_UART_Receive_DMA(uart1, &trace_rx[0], 2 * trace_buffer);

	// start triggering TIM1 on incoming data
	HAL_TIM_OnePulse_Start(tim1, TIM_CHANNEL_1);

}

void trace_disable(void) {

	// disable UART1
	HAL_UART_DMAStop(uart1);

	// disable TIM1 triggering
	HAL_TIM_OnePulse_Stop(tim1, TIM_CHANNEL_1);

	// disable TIM2 timestamp counter
	HAL_TIM_Base_Stop(tim2);
	HAL_TIM_IC_Stop_DMA(tim2, TIM_CHANNEL_1);
}

void process_trace(uint8_t *data) {

	uint32_t cnt = trace_buffer/2;
	uint32_t i;
	uint8_t *ts;
	uint8_t *ptr = &tx_buf[5];

	while (cnt--) {

		// get pointer to first byte of timestamp
		ts = (uint8_t *) &trace_ts[ts_idx++];

		// wrap timestamp pointer if necessary
		if (ts_idx >= 5010) {
			ts_idx = 0;
		}

		// copy timestamp to buffer
		for (i = 0; i < 4; i++) {
			*ptr++ = *ts++;
		}

		// copy trace info to buffer
		*ptr++ = *data++;
		*ptr++ = *data++;
	}

	send_packet(2, 3 * trace_buffer);

}

void process_overflow(void) {
	send_packet(1, 0);
}

void send_packet(uint8_t type, uint16_t length) {

	// wait for previous DMA to complete
	while (sm.cmd_tx_busy) {

	}

	// now we have control!
	sm.cmd_tx_busy = 1;

	tx_buf[0] = 0xAA;	// sync char. Always 0xAA
	tx_buf[1] = (uint8_t) (length & 0xFF);
	tx_buf[2] = (uint8_t) ((length >> 8) & 0xFF);
	tx_buf[3]++;	// incrementing counter
	tx_buf[4] = type;	// msg type
	append_crc(length + 5);	// header length is 5
	HAL_UART_Transmit_DMA(uart2, &tx_buf[0], length + 9);	// header + crc

}

uint32_t append_crc(uint32_t packet_length) {

	static uint32_t i;
	static WORD_VAL calc_crc;

	calc_crc.word = HAL_CRC_Calculate(crc, (uint32_t *) &tx_buf[0],
			packet_length);

	for (i = 0; i < 4; i++) {
		tx_buf[packet_length + i] = calc_crc.byte[3 - i];
	}

	return (packet_length + 4);
}

/**
 * This function is called when TIM2, the timestamp timer rolls over back to zero
 * the value it counts to is currently set to 2^32-1. The timer runs at 40Mhz so
 * it rolls over every 107.3741 seconds. The control computer should simply increment
 * it's 64-bit timestamp when the roll-over occurs.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	sm.timer_overflow = 1;
}

/**
 * This function is called when the first half of the tracer_rx buffer has been filled
 * with new packet information. The DMA is now filling the second half so it is safe
 * to read and process tracer_rx[0-1999]
 *
 * Also called when the control computer has sent a control packet (6 bytes)
 * which has been filled into cmd_rx[0-5]
 */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart) {

	// UART1 = TRACER
	if (huart == uart1) {
		sm.tracer_rx_low = 1;
	}

	// UART2 = CONTROL
	if (huart == uart2) {
		sm.cmd_rx_low = 1;
	}

}

/**
 * This function is called when the second half of the tracer_rx buffer has been filled
 * with new packet information. The DMA has now wrapped back to the beginning of the
 * buffer so it is safe to read and process tracer_rx[2000-3999]
 *
 * Also called when the control computer has sent a control packet (6 bytes)
 * which has been filled into cmd_rx[6-12]
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {

	// UART1 = TRACER
	if (huart == uart1) {
		sm.tracer_rx_high = 1;
	}

	// UART2 = CONTROL
	if (huart == uart2) {
		sm.cmd_rx_high = 1;
	}
}

/**
 * This function is called when the DMA is finished sending data to the command computer
 * indicating that the DMA is free to load up again with new data!
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
	sm.cmd_tx_busy = 0;
}
