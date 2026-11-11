/*
 * registro.h
 *
 *  Created on: 11 nov. 2026
 *      Author: josea
 */

#ifndef REGISTRO_H_
#define REGISTRO_H_
#include <stdint.h>

typedef struct //estructura que define los datos para los informes de medicacion por UART
{
    uint8_t horaProgramada;
    uint8_t minutoProgramada;
    uint8_t minutoConfirmada;
    uint8_t horaConfirmada;
    char medicamento;
    uint32_t diaToma;

} RegistroToma;

void inicializarRegistro(void);
void agregarRegistro(RegistroToma nuevaToma);
uint8_t obtenerNumeroRegistros(void);
const RegistroToma *obtenerRegistro(uint8_t indice);

#endif /* REGISTRO_H_ */
