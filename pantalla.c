/*
 * pantalla.c
 *
 *  Created on: 21 oct. 2026
 *      Author: josea
 */
#include <stdint.h>
#include <stdbool.h>

#include "FT800_TIVA.h"
#include "pastillero_imagen.h"
#include "pantalla.h"
#include <stdio.h>
#include "reloj.h"

#define IMAGEN_DIRECCION 0
#define IMAGEN_RGB565    7
#define BLOQUE_JPG       256
#define ESPERA_MAX       2000000UL

static bool imagenCargada = false;
int BotonCuidador=0;

// Funcion auxiliar para controlar el FIFO
static bool esperarEspacio(uint16_t bytes)
{
    uint32_t lectura;
    uint32_t libre;
    uint32_t intentos;

    for (intentos = 0; intentos < ESPERA_MAX; intentos++)
        {
            lectura = Lee_Reg(REG_CMD_READ) & 0xFFF;

            if (lectura == 0xFFF)
            {
                return false;
            }

            libre = (lectura - CMD_Offset - 4U) & 0xFFF;

            if (libre >= bytes)
            {
                return true;
            }
        }

        return false;
}

static bool esperarFinImagen(void)
{
    uint32_t lectura;
    uint32_t intentos;

    for (intentos = 0; intentos < ESPERA_MAX; intentos++)
    {
        lectura = Lee_Reg(REG_CMD_READ) & 0xFFF;

        if (lectura == 0xFFF)
        {
            return false;
        }

        if (lectura == CMD_Offset)
        {
            return true;
        }
    }

    return false;
}

// Cargar la imagen en el FT800
void cargarImagenPastillero(void)
{
    uint32_t i = 0;
        uint32_t j;
        uint32_t cantidad;
        uint32_t padding;

        imagenCargada = false;

        // Esperar a que el FT800 termine lo anterior
        ComEsperaFin();

        // Enviar el comando de carga
        EscribeRam32(CMD_LOADIMAGE);

        // Direccion de destino en RAM_G
        EscribeRam32(IMAGEN_DIRECCION);

        // Opciones: RGB565 y no modificar la display list
        EscribeRam32(OPT_NODL);

        Ejecuta_Lista();

        // Enviar la imagen por bloques
        while (i < pastillero_jpg_len)
        {
            cantidad = pastillero_jpg_len - i;

            if (cantidad > BLOQUE_JPG)
            {
                cantidad = BLOQUE_JPG;
            }

            // Solo el ultimo bloque puede necesitar relleno
            padding = (4U - (cantidad % 4U)) % 4U;

            if (!esperarEspacio((uint16_t)(cantidad + padding)))
            {
                return;
            }

            for (j = 0; j < cantidad; j++)
            {
                EscribeRam8((char)pastillero_jpg[i + j]);
            }

            // Completar hasta multiplo de cuatro bytes
            for (j = 0; j < padding; j++)
            {
                EscribeRam8(0);
            }

            // Avisar al FT800 de los nuevos datos
            Ejecuta_Lista();

            i += cantidad;
        }

        // Esperar a que termine la descompresion
        if (!esperarFinImagen())
        {
            return;
        }

        imagenCargada = true;
}

// Dibujar la imagen en pantalla
void dibujarImagenPastillero(int x, int y)
{
    if (!imagenCargada)
        {
            return;
        }

        // Guardar contexto grafico
        EscribeRam32(CMD_SAVE_CONTEXT);

        // Seleccionar bitmap 0
        EscribeRam32(0x05000000);

        // Direccion de la imagen en RAM_G
        EscribeRam32(0x01000000 | IMAGEN_DIRECCION);

        // Formato, bytes por fila y altura
        EscribeRam32(
            0x07000000 |
            (IMAGEN_RGB565 << 19) |
            ((PASTILLERO_ANCHO * 2U) << 9) |
            PASTILLERO_ALTO
        );

        // Dimensiones del dibujo
        EscribeRam32(
            0x08000000 |
            (PASTILLERO_ANCHO << 9) |
            PASTILLERO_ALTO
        );

        // Dibujar bitmap
        ComColor(255, 255, 255);
        EscribeRam32(CMD_BEGIN_BMP);

        ComVertex2ff(x, y);

        EscribeRam32(CMD_END);

        // Recuperar contexto grafico
        EscribeRam32(CMD_RESTORE_CONTEXT);
}

void dibujarPantallaInicio(void)
{
    Nueva_pantalla(255,255,255);
    ComColor(10,98,250);
    ComRect(0,0,480,50,true);
    ComColor(255,255,255);
    ComTXT(240, 20, 22, OPT_CENTERX, "ASISTENTE DE MEDICACION");

    uint8_t horas, minutos, segundos;
    char textoHora[9];
    obtenerHora(&horas, &minutos, &segundos);
    sprintf(textoHora, "%02u:%02u:%02u", (unsigned int)horas, (unsigned int)minutos, (unsigned int)segundos);
    ComColor(0,0,0);
    ComTXT(170, 60, 29, OPT_CENTERX, textoHora);
    ComColor(148,147,130);
    if (horas<12 && horas>=6){
        ComTXT(330,60,29,OPT_CENTERX,"BUENOS DIAS");
    }
    else if (horas >= 12 && horas < 21){
        ComTXT(330,60,29,OPT_CENTERX,"BUENOS DIAS");
    }
    else {
        ComTXT(330,60,29,OPT_CENTERX,"BUENOS DIAS");
    }

    ComColor(9,237,16);
    ComRect(20,100,460,230,true);
    ComColor(99,212,101);
    ComRect(22,98,458,228,true);
    dibujarImagenPastillero(30, 105); // Imagen del pastillero






    //Botones
    BotonCuidador = Boton(310, 240, 100, 30, 20, "MODO CUIDADOR");


    Dibuja();
}

