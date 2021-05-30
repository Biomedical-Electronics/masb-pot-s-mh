/**
  ******************************************************************************
  * @file		masb_comm_s.c
  * @brief		Gestión de la comunicación con el host. Versión simplificada de
  *             MASB-COMM.
  * @author		Albert Álvarez Carulla
  * @copyright	Copyright 2020 Albert Álvarez Carulla. All rights reserved.
  ******************************************************************************
  */


#include "components/masb_comm_s.h"
#include "components/cobs.h"
#include "main.h"

static ADC_HandleTypeDef *hadc1;
static I2C_HandleTypeDef *hi2c1;
static TIM_HandleTypeDef *htim3;
static UART_HandleTypeDef *huart;

static _Bool dataReceived = FALSE;
uint8_t rxIndex = 0;
uint8_t rxBuffer[UART_BUFF_SIZE] = { 0 };
uint8_t rxBufferDecoded[UART_BUFF_SIZE] = { 0 };
uint8_t txBuffer[UART_BUFF_SIZE] = { 0 };
uint8_t txBufferDecoded[UART_BUFF_SIZE] = { 0 };

static double saveByteArrayAsDoubleFromBuffer(uint8_t *buffer, uint8_t index);
static uint32_t saveByteArrayAsLongFromBuffer(uint8_t *buffer, uint8_t index);
static void saveLongAsByteArrayIntoBuffer(uint8_t *buffer, uint8_t index, uint32_t longVal);
static void saveDoubleAsByteArrayIntoBuffer(uint8_t *buffer, uint8_t index, double doubleVal);


void MASB_COMM_S_setUart(UART_HandleTypeDef *newHuart) {
	huart = newHuart;
}

void MASB_COMM_S_waitForMessage(void) {

	dataReceived = FALSE;
	rxIndex = 0;
	HAL_UART_Receive_IT(huart, &rxBuffer[rxIndex], 1);

}

_Bool MASB_COMM_S_dataReceived(void) {

	if (dataReceived) {

		COBS_decode(rxBuffer, rxIndex, rxBufferDecoded);

 	}

 	return dataReceived;
}

uint8_t MASB_COMM_S_command(void) {

	return rxBufferDecoded[0];

}

struct CV_Configuration_S MASB_COMM_S_getCvConfiguration(void) {
	struct CV_Configuration_S cvConfiguration;

 	cvConfiguration.eBegin = saveByteArrayAsDoubleFromBuffer(rxBufferDecoded, 1);
 	cvConfiguration.eVertex1 = saveByteArrayAsDoubleFromBuffer(rxBufferDecoded, 9);
 	cvConfiguration.eVertex2 = saveByteArrayAsDoubleFromBuffer(rxBufferDecoded, 17);
 	cvConfiguration.cycles = rxBufferDecoded[25];
 	cvConfiguration.scanRate = saveByteArrayAsDoubleFromBuffer(rxBufferDecoded, 26);
 	cvConfiguration.eStep = saveByteArrayAsDoubleFromBuffer(rxBufferDecoded, 34);

 	return cvConfiguration;

}

struct CA_Configuration_S MASB_COMM_S_getCaConfiguration(void) {
	struct CA_Configuration_S caConfiguration;

 	caConfiguration.eDC = saveByteArrayAsDoubleFromBuffer(rxBufferDecoded, 1);
 	caConfiguration.samplingPeriodMs = saveByteArrayAsLongFromBuffer(rxBufferDecoded, 9);
 	caConfiguration.measurementTime = saveByteArrayAsLongFromBuffer(rxBufferDecoded, 13);

 	return caConfiguration;

}

static double saveByteArrayAsDoubleFromBuffer(uint8_t *buffer, uint8_t index) {

	union Double_Converter {
		double d;
		uint8_t b[8];
	} doubleConverter;

		for (uint8_t i = 0; i < 8; i++) {

				doubleConverter.b[i] = buffer[i + index];

		}

		return doubleConverter.d;

 }

static uint32_t saveByteArrayAsLongFromBuffer(uint8_t *buffer, uint8_t index) {

	union Long_Converter {
		long l;
		uint8_t b[4];
	} longConverter;

	for (uint8_t i = 0; i < 4; i++) {

		longConverter.b[i] = buffer[i + index];
	}

	return longConverter.l;

 }


void MASB_COMM_S_sendData(struct Data_S data) {

 	saveLongAsByteArrayIntoBuffer(txBufferDecoded, 0, data.point);
 	saveLongAsByteArrayIntoBuffer(txBufferDecoded, 4, data.timeMs);
 	saveDoubleAsByteArrayIntoBuffer(txBufferDecoded, 8, data.voltage);
 	saveDoubleAsByteArrayIntoBuffer(txBufferDecoded, 16, data.current);

 	uint32_t txBufferLenght = COBS_encode(txBufferDecoded, 24, txBuffer);

 	txBuffer[txBufferLenght] = UART_TERM_CHAR;
 	txBufferLenght++;

 	HAL_UART_Transmit_IT(huart, txBuffer, txBufferLenght);

}

static void saveLongAsByteArrayIntoBuffer(uint8_t *buffer, uint8_t index, uint32_t longVal) {

    union Long_Converter {
       long l;
       uint8_t b[4];
    } longConverter;

 		longConverter.l = longVal;

 		for (uint8_t i = 0; i < 4; i++) {

 			buffer[i + index] = longConverter.b[i];

 		}

}

static void saveDoubleAsByteArrayIntoBuffer(uint8_t *buffer, uint8_t index, double doubleVal) {

   union Double_Converter {
       double d;
       uint8_t b[8];
   } doubleConverter;

 		doubleConverter.d = doubleVal;

 		for (uint8_t i = 0; i < 8; i++) {

 			buffer[i + index] = doubleConverter.b[i];

 		}

}

void HAL_UART_RxCpltCallback (UART_HandleTypeDef *huart) {

	if (rxBuffer[rxIndex] == UART_TERM_CHAR) {
 			dataReceived = TRUE;
 	} else {
 			rxIndex++;
 			HAL_UART_Receive_IT(huart, &rxBuffer[rxIndex], 1);

 	}
}
