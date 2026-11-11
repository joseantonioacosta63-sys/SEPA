/*
 * registro.c
 *
 *  Created on: 11 nov. 2026
 *      Author: josea
 */


#include "registro.h"
#include "driverlib2.h"
#include <stddef.h>
#include <stdbool.h>

#define DIRECCION_REGISTRO 1024U
#define MAX_REGISTROS 10

// Identificador para reconocer nuestro historial
#define FIRMA_REGISTRO 0x52454731UL

static RegistroToma historial[MAX_REGISTROS];
static uint8_t numeroRegistros = 0;

// Indica si la EEPROM pudo inicializarse
static bool eepromDisponible = false;

// Datos que queremos conservar en EEPROM
typedef struct
{
    uint32_t firma;
    uint32_t cantidad;
    RegistroToma tomas[MAX_REGISTROS];

} HistorialPersistente;


// Buffer alineado a palabras de 32 bits
typedef union
{
    HistorialPersistente datos;
    uint32_t palabras[(sizeof(HistorialPersistente) + 3U) / 4U];

} BufferEEPROM;

static BufferEEPROM bufferEEPROM;

// Guarda el historial completo en EEPROM
static bool guardarHistorialEEPROM(void)
{
    uint8_t i;

    if (!eepromDisponible)
    {
        return false;
    }

    bufferEEPROM.datos.firma = FIRMA_REGISTRO;
    bufferEEPROM.datos.cantidad = numeroRegistros;

    for (i = 0; i < MAX_REGISTROS; i++)
    {
        bufferEEPROM.datos.tomas[i] = historial[i];
    }

    return EEPROMProgram(bufferEEPROM.palabras,DIRECCION_REGISTRO,sizeof(bufferEEPROM.palabras)) == 0U;
}


// Inicializa y recupera los registros anteriores
void inicializarRegistro(void)
{
    uint8_t i;
    uint32_t intentos;

    numeroRegistros = 0;
    eepromDisponible = false;

    // Habilitar EEPROM0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0);

    // Esperar a que el periferico este disponible
    for (intentos = 0; intentos < 100000U; intentos++)
    {
        if (SysCtlPeripheralReady(SYSCTL_PERIPH_EEPROM0))
        {
            break;
        }
    }

    if (intentos == 100000U)
    {
        return;
    }

    // Inicializar EEPROM
    if (EEPROMInit() != EEPROM_INIT_OK)
    {
        return;
    }

    // Comprobar que hay espacio suficiente
    if (EEPROMSizeGet() <
        DIRECCION_REGISTRO + sizeof(bufferEEPROM.palabras))
    {
        return;
    }

    eepromDisponible = true;

    // Recuperar los datos almacenados
    EEPROMRead(bufferEEPROM.palabras,DIRECCION_REGISTRO,sizeof(bufferEEPROM.palabras));

    // Validar la cabecera antes de usar los datos
    if (bufferEEPROM.datos.firma != FIRMA_REGISTRO || bufferEEPROM.datos.cantidad > MAX_REGISTROS)
    {
        return;
    }

    numeroRegistros = (uint8_t)bufferEEPROM.datos.cantidad;

    // Restaurar las tomas en RAM
    for (i = 0; i < numeroRegistros; i++)
    {
        historial[i] = bufferEEPROM.datos.tomas[i];
    }
}

// Añade un registro y actualiza EEPROM
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
        // Historial lleno: descartar el mas antiguo
        for (i = 1; i < MAX_REGISTROS; i++)
        {
            historial[i - 1] = historial[i];
        }

        // Guardar el nuevo al final
        historial[MAX_REGISTROS - 1] = nuevaToma;
    }

    // Actualizar la copia persistente
    guardarHistorialEEPROM();
}


// Devuelve cuantos registros tenemos
uint8_t obtenerNumeroRegistros(void)
{
    return numeroRegistros;
}


// Devuelve un registro del historial
const RegistroToma *obtenerRegistro(uint8_t indice)
{
    if (indice >= numeroRegistros)
    {
        return NULL;
    }

    return &historial[indice];
}


