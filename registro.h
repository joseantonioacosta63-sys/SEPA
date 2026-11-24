/*
 * registro.h
 *
 *  Created on: 11 nov. 2026
 *      Author: josea
 */

#ifndef REGISTRO_H_
#define REGISTRO_H_
#include <stdint.h>

/* Distingue una toma confirmada de una toma que vencio sin confirmarse. */
typedef enum
{
    REGISTRO_CONFIRMACION,
    REGISTRO_EXPIRACION
} TipoRegistroToma;

/* Registro temporal que se muestra por UART; no se almacena en EEPROM. */
typedef struct
{
    uint8_t horaProgramada;
    uint8_t minutoProgramada;
    uint8_t minutoConfirmada;
    uint8_t horaConfirmada;
    char medicamento;
    uint32_t diaToma;
    TipoRegistroToma tipo;

} RegistroToma;

/* Vacia el historial que reside en RAM al iniciar el programa. */
void inicializarRegistro(void);
/* Añade una entrada y descarta la mas antigua cuando el bufer esta lleno. */
void agregarRegistro(RegistroToma nuevaToma);
/* Informa de cuantos registros validos contiene el historial. */
uint8_t obtenerNumeroRegistros(void);
/* Devuelve una entrada de solo lectura o NULL si el indice no existe. */
const RegistroToma *obtenerRegistro(uint8_t indice);

#endif /* REGISTRO_H_ */
