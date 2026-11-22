/*
 * registro.c
 *
 *  Created on: 11 nov. 2026
 *      Author: josea
 */


#include "registro.h"
#include <stddef.h>

/* Historial circular sencillo en RAM: evita desgastar la EEPROM con incidencias. */
#define MAX_REGISTROS 30

static RegistroToma historial[MAX_REGISTROS];
static uint8_t numeroRegistros = 0;

/* Al reiniciar la placa no se conserva el historial; la configuración sí está en EEPROM. */
void inicializarRegistro(void)
{
    numeroRegistros = 0;
}

/* Inserta una toma; al llenarse, desplaza el búfer y reemplaza la más antigua. */
void agregarRegistro(RegistroToma nuevaToma)
{
    uint8_t i;

    if (numeroRegistros < MAX_REGISTROS)
    {
        historial[numeroRegistros] = nuevaToma;
        numeroRegistros++;
    }
    else
    {
        /* Historial lleno: descartar el más antiguo para conservar las últimas entradas. */
        for (i = 1; i < MAX_REGISTROS; i++)
        {
            historial[i - 1] = historial[i];
        }

        /* Guardar el nuevo al final tras liberar su posición. */
        historial[MAX_REGISTROS - 1] = nuevaToma;
    }
}


/* Devuelve cuántos registros válidos se pueden consultar. */
uint8_t obtenerNumeroRegistros(void)
{
    return numeroRegistros;
}


/* Devuelve una referencia de solo lectura o NULL para un índice fuera de rango. */
const RegistroToma *obtenerRegistro(uint8_t indice)
{
    if (indice >= numeroRegistros)
    {
        return NULL;
    }

    return &historial[indice];
}


