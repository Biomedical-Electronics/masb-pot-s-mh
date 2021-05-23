/*
 * stm32main.c
 *
 *  Created on: May 10, 2021
 *      Author: Helena
 */
#include "components/ad5280_driver.h"
#include "components/mcp4725_driver.h"
#include "components/i2c_lib.h"
#include "components/stm32main.h"
#include "components/masb_comm_s.h"
#include "main.h"

struct CV_Configuration_S cvConfiguration;
struct CA_Configuration_S caConfiguration;
struct Data_S data;
AD5280_Handle_T hpot = NULL;
MCP4725_Handle_T hdac = NULL;

extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart2;

_Bool Get_CAMeasure = FALSE;
_Bool Get_CVMeasure = FALSE;

float VDAC;
uint32_t ADCval_Vcell = 0;
uint32_t ADCval_Icell = 0;
double Vadc, Vadc1, Vadc2;

void setup(struct Handles_S *handles) {

	MASB_COMM_S_setUart(handles->huart);


	MASB_COMM_S_waitForMessage(); //Espera al primer byte

	HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, 1); //PMU habilitada

	I2C_Init(handles->hi2c1); // Inicializamos libreria I2C

	// Potenciometro

	hpot = AD5280_Init();

	AD5280_ConfigSlaveAddress(hpot, 0x2C);
	AD5280_ConfigNominalResistorValue(hpot, 50e3f);
	AD5280_ConfigWriteFunction(hpot, I2C_Write);

	// Fijamos la resistencia de, por ejemplo, 12kohms.
	AD5280_SetWBResistance(hpot, 12e3f);

	// DAC

	hdac = MCP4725_Init();

	MCP4725_ConfigSlaveAddress(hdac, 0x66); //Dirección I2C en binario 1100000
	MCP4725_ConfigVoltageReference(hdac, 4.0f);
	MCP4725_ConfigWriteFunction(hdac, I2C_Write);

}

void loop(void) {
	if (MASB_COMM_S_dataReceived()) { // Si se ha recibido un mensaje...

		switch (MASB_COMM_S_command()) {
			case START_CV_MEAS: // Si hemos recibido START_CV_MEAS

				// Leemos la configuracion que se nos ha enviado en el mensaje y
				// la guardamos en la variable cvConfiguration
				cvConfiguration = MASB_COMM_S_getCvConfiguration();

				// Fijar VCELL a eBegin

				double vObjetivo;

				VDAC = 1.65 - (cvConfiguration.eBegin / 2); // VDAC = 1.65 - VCELL/2
				MCP4725_SetOutputVoltage(hdac, VDAC); // En vez de 0.0f metemos VDAC

				vObjetivo = cvConfiguration.eVertex1;
				double Vcell = cvConfiguration.eVertex1;

				//Cerramos el relé

				HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_SET);

				double period = (cvConfiguration.eStep / cvConfiguration.scanRate)* 1000; //periodo en ms

				__HAL_TIM_SET_AUTORELOAD(&htim3, period * 10);
				__HAL_TIM_SET_COUNTER(&htim3, 0); //Reiniciamos el contador del timer a cero

				HAL_TIM_Base_Start_IT(&htim3); //Iniciar el funcionamiento del timer con interrupciones

				double counter = 0;
				int i = 0;

				Get_CVMeasure = FALSE;

				while (counter <= cvConfiguration.cycles) {

					if (Get_CVMeasure){

						// ADCvalor = Vadc/Vref (2^bits-1).
						HAL_ADC_Start(&hadc1);
						HAL_ADC_PollForConversion(&hadc1, 200); //esperamos que finalice la conversion
						ADCval_Vcell = HAL_ADC_GetValue(&hadc1);

						Vadc1 = ADCval_Vcell * 3.3 / (2 ^ 12 - 1); //12 bits, Vref es 3.3V

						HAL_ADC_Start(&hadc1);
						HAL_ADC_PollForConversion(&hadc1, 200); //esperamos que finalice la conversion
						ADCval_Icell = HAL_ADC_GetValue(&hadc1);
						Vadc2 = ADCval_Icell * 3.3 / (2 ^ 12 - 1);

						data.point = i; //Numero de la muestra
						data.timeMs = __HAL_TIM_GetCounter(&htim3); //tiempo del timer
						data.voltage = (1.65 - Vadc1) * 2;
						data.current = (Vadc2 - 1.65) * 2 / 10000; //RTIA de 10kOhms

						// Enviar datos al host
						MASB_COMM_S_sendData(data);

						i++;
						Get_CVMeasure = FALSE;

						if (Vcell == vObjetivo) { //YO PONDRIA UN WHILE
							if (vObjetivo == cvConfiguration.eVertex1) {
								vObjetivo = cvConfiguration.eVertex2;

							} else if (vObjetivo == cvConfiguration.eVertex2) {
								vObjetivo = cvConfiguration.eBegin;
								counter = counter + 1; //Ha transcurrido un ciclo

							} else if (counter == cvConfiguration.cycles) { //si es el ultimo
								break;

							} else {
								vObjetivo = cvConfiguration.eVertex1;
							}

						} else {

							if ((Vcell + cvConfiguration.eStep) > vObjetivo) {
								VDAC = 1.65 - (vObjetivo / 2); // VDAC = 1.65 - VCELL/2
								MCP4725_SetOutputVoltage(hdac, VDAC);
							} else {
								Vcell = Vcell - cvConfiguration.eStep;
							}


						}


					}
				}

				HAL_TIM_Base_Stop_IT(&htim3);
				HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_RESET);

				break;

			case START_CA_MEAS:

				caConfiguration = MASB_COMM_S_getCaConfiguration();

				// Fijar Vcell = eDC
				VDAC = 1.65 - (caConfiguration.eDC / 2); // VDAC = 1.65 - VCELL/2
				MCP4725_SetOutputVoltage(hdac, VDAC); // En vez de 0.0f metemos VDAC

				//Cerramos el relé

				HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_SET);
				__HAL_TIM_SET_AUTORELOAD(&htim3, caConfiguration.samplingPeriodMs * 10);
				__HAL_TIM_SET_COUNTER(&htim3, 0);

				HAL_TIM_Base_Start_IT(&htim3); //Iniciar el funcionamiento del timer con interrupciones

				double time = 0;
				i = 0;

				// Medir Vcell y Icell

				// ADCvalor = Vadc/Vref (2^bits-1).
				HAL_ADC_Start(&hadc1);
				HAL_ADC_PollForConversion(&hadc1, 200); //esperamos que finalice la conversion
				ADCval_Vcell = HAL_ADC_GetValue(&hadc1);

				Vadc1 = ADCval_Vcell * 3.3 / (2^12 - 1); //12 bits, Vref es 3.3V

				HAL_ADC_Start(&hadc1);
				HAL_ADC_PollForConversion(&hadc1, 200); //esperamos que finalice la conversion
				ADCval_Icell = HAL_ADC_GetValue(&hadc1);

				Vadc2 = ADCval_Icell * 3.3 / (2^12 - 1);

				data.point = i; //Numero de la muestra
				data.timeMs = caConfiguration.samplingPeriodMs * i; //tiempo del timer
				data.voltage = (1.65 - Vadc1) * 2;
				data.current = (Vadc2 - 1.65) * 2 / 10000; //RTIA de 10kOhms

				// Enviar datos al host
				MASB_COMM_S_sendData(data);

				time = time + caConfiguration.samplingPeriodMs;
				i++;


				Get_CAMeasure = FALSE;

				while (time <= caConfiguration.measurementTime * 1000) { //Measurement time en ms
					if (Get_CAMeasure) {

						// Medir Vcell y Icell

							// ADCvalor = Vadc/Vref (2^bits-1).
							HAL_ADC_Start(&hadc1);
							HAL_ADC_PollForConversion(&hadc1, 200); //esperamos que finalice la conversion
							ADCval_Vcell = HAL_ADC_GetValue(&hadc1);

							Vadc1 = ADCval_Vcell * 3.3 / (2^12 - 1); //12 bits, Vref es 3.3V

							HAL_ADC_Start(&hadc1);
							HAL_ADC_PollForConversion(&hadc1, 200); //esperamos que finalice la conversion
							ADCval_Icell = HAL_ADC_GetValue(&hadc1);

							Vadc2 = ADCval_Icell * 3.3 / (2^12 - 1); //12 bits, Vref es 3.3V


							data.point = i; //Numero de la muestra
							data.timeMs = caConfiguration.samplingPeriodMs * i; //tiempo del timer
							data.voltage = (1.65 - Vadc1) * 2;
							data.current = (Vadc2 - 1.65) * 2 / 10000; //RTIA de 10kOhms

							// Enviar datos al host
							MASB_COMM_S_sendData(data);

							Get_CAMeasure = FALSE;
							time = time + caConfiguration.samplingPeriodMs;
							i++;
					}
				}

				HAL_TIM_Base_Stop_IT(&htim3);
				HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_RESET);

				break;

			case STOP_MEAS:
				break;

			default:

				break;

		}

		MASB_COMM_S_waitForMessage();

	}


}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	switch (MASB_COMM_S_command()) {

		case START_CV_MEAS:
			Get_CVMeasure = TRUE;

			break;

		case START_CA_MEAS:

			Get_CAMeasure = TRUE;

			break;
	}

}
