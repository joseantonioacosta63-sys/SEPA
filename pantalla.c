/*
 * pantalla.c
 *
 *  Created on: 21 oct. 2026
 *      Author: josea
 */
#include <stdint.h>
#include <stdbool.h>

#include "utils/uartstdio.h"

#include "FT800_TIVA.h"
#include "pastillero_imagen.h"
#include "pantalla.h"
#include <stdio.h>
#include "reloj.h"
#include "medicacion.h"

extern float T_act;

#define IMAGEN_DIRECCION 0
#define IMAGEN_RGB565    7
#define BLOQUE_JPG       256
#define ESPERA_MAX       2000000UL

static bool imagenCargada = false;
int BotonCuidador=0;
int BotonVolver=0;
int BotonConfirmarA=0;
int BotonConfirmarB=0;
int BotonConfirmarC=0;


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
    char textoTemperatura[30];
    char avisoCalor[30];
    char avisoFrio[30];
    Nueva_pantalla(255,255,255);
    ComColor(148,170,255);
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
        ComTXT(325,60,29,OPT_CENTERX,"BUENOS DIAS");
    }
    else if (horas >= 12 && horas < 21){
        ComTXT(325,60,29,OPT_CENTERX,"BUENOS TARDES");
    }
    else {
        ComTXT(325,60,29,OPT_CENTERX,"BUENOS NOCHES");
    }

    ComColor(9,237,16);
    ComRect(20,100,460,225,true);
    ComColor(99,212,101);
    ComRect(22,102,458,223,true);
    dibujarImagenPastillero(30, 105); // Imagen del pastillero
    ComColor(31,107,22);
    ComRect(210,110,410,140,true);
    ComColor(255,255,255);
    ComTXT(310,115,22,OPT_CENTERX,"PROXIMA PASTILLA:");

    const MedicConfig *medicamento;
    const HorarioConfig *horario;
    uint8_t indiceMedicamento;
    uint8_t indiceHorario;
    char proximaHora[9];

    if (obtenerProximaToma(&indiceMedicamento, &indiceHorario)){
        medicamento = obtenerMedicamentoConfigurado(indiceMedicamento);
        horario = obtenerHorarioConfigurado(indiceMedicamento, indiceHorario);
        ComColor(0,0,0);
        sprintf(proximaHora, "%02u:%02u", (unsigned int)horario->hora, (unsigned int)horario->minuto);
        ComTXT(310, 153, 30, OPT_CENTERX, proximaHora);
        ComTXT(310, 195, 28, OPT_CENTERX, medicamento->nombre);
    }
    else
    {
        ComColor(255, 0, 0);
        ComTXT(310, 160, 22, OPT_CENTERX, "SIN PROXIMA TOMA");
    }

    sprintf(textoTemperatura, "%.1f C", T_act);
    ComColor(0, 0, 0);
    ComTXT(50, 240, 22, OPT_CENTERX, textoTemperatura);

    if (T_act >= 30)
    {
        sprintf(avisoCalor, "HACE CALOR, PON EL AIRE", T_act);
        ComColor(255,3,0);
        ComTXT(80, 240, 22, 0, avisoCalor);
    }
    else
    {
        sprintf(avisoFrio, "HACE FRIO, PON LA ESTUFA", T_act);
        ComColor(0,61,194);
        ComTXT(80, 240, 22, 0, avisoFrio);
    }

    //Botones
    ComColor(224,184,65);
    BotonCuidador = Boton(330, 233, 100, 35, 20, "MODO CUIDADOR");

    Dibuja();
}

void dibujarPantallaCuidador(void){

    BotonConfirmarA = 0;
    BotonConfirmarB = 0;
    BotonConfirmarC = 0;
    const EstadoDiarioHorario *estadoA;
    const EstadoDiarioHorario *estadoB;
    const EstadoDiarioHorario *estadoC;
    estadoA = obtenerEstadoDiarioHorario(0, 0);
    estadoB = obtenerEstadoDiarioHorario(1, 0);
    estadoC = obtenerEstadoDiarioHorario(2, 0);
    Nueva_pantalla(255,255,255);
    ComColor(148,170,255);
    ComRect(0,0,480,50,true);
    ComColor(255,255,255);
    ComTXT(240, 20, 22, OPT_CENTERX, "MODO CUIDADOR");
    ComColor(0,0,0);
    ComTXT(40, 60, 22, 0, "MEDICACIONES DE HOY");
    ComColor(222,215,215);
    ComRect(15,90,465,125,true);
    ComRect(15,135,465,170,true);
    ComRect(15,180,465,215,true);
    ComColor(0,0,0);
    ComTXT(30, 95, 20, 0, "09:00 / MEDICACION A");
    if (estadoA != NULL)
    {
        if (estadoA->estado == CONFIRMADA)
        {
            ComColor(30,201,30);
            ComTXT(40, 115, 20, 0, "CONFIRMADA");
            ComColor(212,212,76);
            Boton(400, 90, 65, 35, 20, "TOMADA");
        }
        else if (estadoA->estado == PENDIENTE)
        {
            ComColor(204,124,0);
            ComTXT(40, 115, 20, 0, "PENDIENTE");
            ComColor(15,199,12);
            BotonConfirmarA = Boton(400, 90, 65, 35, 20, "CONFIRMAR");
            if (BotonConfirmarA != 0)
            {
                UARTprintf("BOTON A: X=%d, Y=%d\r\n", POSX, POSY);
            }
        }
        else
        {
            ComColor(148,135,135);
            ComTXT(40, 115, 20, 0, "PROGRAMADA");
            Boton(400, 90, 65, 35, 20, "ESPERAR");
        }
    }
    ComColor(0,0,0);
    ComTXT(30, 140, 20, 0, "14:00 / MEDICACION B");
    if (estadoB != NULL)
        {
            if (estadoB->estado == CONFIRMADA)
            {
                ComColor(30,201,30);
                ComTXT(40, 160, 20, 0, "CONFIRMADA");
                ComColor(212,212,76);
                Boton(400, 135, 65, 35, 20, "TOMADA");
            }
            else if (estadoB->estado == PENDIENTE)
            {
                ComColor(204,124,0);
                ComTXT(40, 160, 20, 0, "PENDIENTE");
                ComColor(15,199,12);
                BotonConfirmarB = Boton(400, 135, 65, 35, 20, "CONFIRMAR");
            }
            else
            {
                ComColor(148,135,135);
                ComTXT(40, 160, 20, 0, "PROGRAMADA");
                Boton(400, 135, 65, 35, 20, "ESPERAR");
            }
        }
    ComColor(0,0,0);
    ComTXT(30, 185, 20, 0, "21:00 / MEDICACION C");
    if (estadoC != NULL)
        {
            if (estadoC->estado == CONFIRMADA)
            {
                ComColor(30,201,30);
                ComTXT(40, 205, 20, 0, "CONFIRMADA");
                ComColor(212,212,76);
                Boton(400, 180, 65, 35, 20, "TOMADA");

            }
            else if (estadoC->estado == PENDIENTE)
            {
                ComColor(204,124,0);
                ComTXT(40, 205, 20, 0, "PENDIENTE");
                ComColor(15,199,12);
                BotonConfirmarC = Boton(400, 180, 65, 35, 20, "CONFIRMAR");
            }
            else
            {
                ComColor(148,135,135);
                ComTXT(40, 205, 20, 0, "PROGRAMADA");
                Boton(400, 180, 65, 35, 20, "ESPERAR");
            }
        }





    //Boton para volver a la primera interfaz
    ComColor(252,30,30);
    BotonVolver = Boton(210, 230, 90, 35, 20, "VOLVER A INICIO");
    Dibuja();
}


