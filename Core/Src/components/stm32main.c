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

struct CV_Configuration_S cvConfiguration;
struct CA_Configuration_S caConfiguration;
struct Data_S data;

#define EN_Pin				GPIO_PIN_5
#define EN_GPIO_Port		GPIOA

void setup(struct Handles_S *handles) {

	MASB_COMM_S_setUart(handles->huart);
    MASB_COMM_S_waitForMessage(); //Espera al primer byte

    HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, 1); //PMU habilitada

    I2C_Init(&hi2c1); // Inicializamos libreria I2C

    // Potenciometro

    AD5280_Handle_T hpot = NULL;

    hpot = AD5280_Init();

    AD5280_ConfigSlaveAddress(hpot, 0x2C);
    AD5280_ConfigNominalResistorValue(hpot, 50e3f);
    AD5280_ConfigWriteFunction(hpot, I2C_Write);

    // Fijamos la resistencia de, por ejemplo, 10kohms.
    AD5280_SetWBResistance(hpot, 10e3f);

    // DAC

   MCP4725_Handle_T hdac = NULL;

   hdac = MCP4725_Init();

   MCP4725_ConfigSlaveAddress(hdac, 0x60); //Dirección I2C en binario 1100000
   MCP4725_ConfigVoltageReference(hdac, 4.0f);
   MCP4725_ConfigWriteFunction(hdac, I2C_Write);
}

void loop(void) {
	if (MASB_COMM_S_dataReceived()) { // Si se ha recibido un mensaje...

			switch(MASB_COMM_S_command()) { // Miramos que comando hemos recibido

				case START_CV_MEAS: // Si hemos recibido START_CV_MEAS

	                // Leemos la configuracion que se nos ha enviado en el mensaje y
	                // la guardamos en la variable cvConfiguration
					cvConfiguration = MASB_COMM_S_getCvConfiguration();

	 				__NOP(); // Esta instruccion no hace nada y solo sirve para poder anadir un breakpoint

	 				// Aqui iria todo el codigo de gestion de la medicion que hareis en el proyecto
	                // si no quereis implementar el comando de stop.

	 				break;

				case START_CA_MEAS:
					caConfiguration = MASB_COMM_S_getCaConfiguration();


					_NOP();


					// Fijar Vcell = eDC

					float VDAC;
					VDAC = 1.65 - (caConfiguration.eDC)/2; // VDAC = 1.65 - VCELL/2
					MCP4725_SetOutputVoltage(hdac, VDAC); // En vez de 0.0f metemos VDAC


					//Cerramos el relé

					HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_PIN, 1);

					HAL_TIM_Base_Start_IT(&htim3); //Iniciar el funcionamiento del timer con interrupciones
						// Este código va aquí???


					// El siguiente código iria en el main.c?? /USER CODE BEGIN 4/
					int contador = 0;

					void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {

						//Si time == samplingPeriodMs:
							// ADCvalor = Vadc/Vref (2^bits-1)
						// Cómo leemos ADCvalor?
						// VREF es el valor del pin REF? En que momento fijamos este valor VREF?
						// Y los bits??

							// Medir Vcell y Icell

							// data.point = Numero de la muestra
						// PONEMOS EL VALOR DE LA VARIABLE contador??
							// data.timeMs = lo sacamos del timer
						//COMO ESCRIBIMOS EL TIEMPO DEL TIMER???
							// data.voltage = (1.65 - Vadc)*2
							// data.current = (Vadc - 1.65)*2/10000 //RTIA de 10kOhms
							contador = contador + 1;
							if (contador == caConfiguration.measurementTime){
								HAL_TIM_Base_Stop_IT(&htim3);
								HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_PIN, 0);
							}

					}

					// Preguntamos si el measurement time entra bien en esta funcion.


					// Enviar datos al host
					MASB_COMM_S_sendData(data);


					//While time < caConfiguration.measurementTime:
					// Si time == caConfiguration.samplingPeriodMs:
						// Medir Vcell y Icell ???
						// Enviar datos al host
					// if time = caConfiguration.measurementTime:
						// RELAY = 0; //Abrimos el relé


					break;


				case STOP_MEAS: // Si hemos recibido STOP_MEAS

	 				/*
	 				 * Debemos de enviar esto desde CoolTerm:
	 				 * 020300
	 				 */
	 				__NOP(); // Esta instruccion no hace nada y solo sirve para poder anadir un breakpoint

	 				// Aqui iria el codigo para tener la medicion si implementais el comando stop.

	 				break;

	 			default: // En el caso que se envia un comando que no exista


	                // Enviamos los datos
	 				MASB_COMM_S_sendData(data);

	 				break;

	 		}

	       // Una vez procesado los comando, esperamos el siguiente
	 		MASB_COMM_S_waitForMessage();

	 	}

	 	// Aqui es donde deberia de ir el codigo de control de las mediciones si se quiere implementar
	   // el comando de STOP.

}
