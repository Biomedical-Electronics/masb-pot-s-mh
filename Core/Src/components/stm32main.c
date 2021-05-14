/*
 * stm32main.c
 *
 *  Created on: May 10, 2021
 *      Author: Helena
 */

#include "components/stm32main.h"
#include "components/masb_comm_s.h"

struct CV_Configuration_S cvConfiguration;
struct CA_Configuration_S caConfiguration;
struct Data_S data;

void setup(struct Handles_S *handles) {
	EN = 1; //Habilitamos la PMU
	MASB_COMM_S_setUart(handles->huart);
    MASB_COMM_S_waitForMessage(); //Espera al primer byte
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

					VREF = caConfiguration.eDC; // Vcell = eDC
					RELAY = 1; //cerramos el relé

					//Si time == samplingPeriodMs:
					// ADCvalor = Vadc/Vref (2^bits-1) formula sacada de pract 4
					// Medir Vcell y Icell

					// data.point = Numero de la muestra
					// data.timeMs = lo sacamos del timer
					// data.voltage = (1.65 - Vadc)*2
					// data.current = (Vadc - 1.65)*2/10000 //RTIA de 10kOhms

					// Enviar datos al host
					//MASB_COMM_S_sendData(data);



					//While time < caConfiguration.measurementTime:
					// Si time == caConfiguration.samplingPeriodMs:
						// Medir Vcell y Icell ???
						// Enviar datos al host
					// if time = caConfiguration.measurementTime:
						// RELAY = 0; //Abrimos el relé

					//Utilizamos interrupciones: haremos una interrupcion para smpling time
					// si por ejemplo sampling es 1 seg y measurement es 10, cuando hayan pasado 11 sampling salimos del bucle
					//EN EL TIMER TENDRE QUE METER UN HAL_..._SET PERIOD CON EL PERIODOSAMPLING QUE HEMOS LEÍDO DEL CA CONFIGURATION

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
