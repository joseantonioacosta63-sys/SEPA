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

/* Escribe una linea con etiqueta, hora y nombre de una toma. */
static void dibujarResumenToma(char *etiqueta, int y, bool encontrada,
                               uint8_t indiceMedicamento, uint8_t indiceHorario)
{
    char texto[56];
    const MedicConfig *medicamento;
    const HorarioConfig *horario;

    if (!encontrada)
    {
        sprintf(texto, "%s: --", etiqueta);
    }
    else
    {
        medicamento = obtenerMedicamentoConfigurado(indiceMedicamento);
        horario = obtenerHorarioConfigurado(indiceMedicamento, indiceHorario);
        sprintf(texto, "%s: %02u:%02u  %s", etiqueta,
                (unsigned int)horario->hora, (unsigned int)horario->minuto,
                medicamento->nombre);
    }

    ComColor(0, 0, 0);
    ComTXT(180, y, 26, 0, texto);
}

/* Dibuja una fila del cuidador para uno de los tres medicamentos de prueba. */
static void dibujarFilaCuidador(uint8_t indiceMedicamento, int y,
                                int *botonConfirmar)
{
    char texto[48];
    const MedicConfig *medicamento;
    const HorarioConfig *horario;
    const EstadoDiarioHorario *estado;

    *botonConfirmar = 0;
    medicamento = obtenerMedicamentoConfigurado(indiceMedicamento);
    horario = obtenerHorarioConfigurado(indiceMedicamento, 0);
    estado = obtenerEstadoDiarioHorario(indiceMedicamento, 0);

    if (medicamento == NULL || horario == NULL || estado == NULL)
    {
        return;
    }

    ComColor(229, 233, 239);
    ComRect(12, y, 468, y + 40, true);
    sprintf(texto, "%02u:%02u  %s", (unsigned int)horario->hora,
            (unsigned int)horario->minuto, medicamento->nombre);
    ComColor(0, 0, 0);
    ComTXT(25, y + 7, 26, 0, texto);

    if (estado->estado == CONFIRMADA)
    {
        ComColor(20, 130, 55);
        ComTXT(250, y + 22, 26, 0, "CONFIRMADA");
        ComColor(155, 190, 130);
        Boton(365, y + 3, 95, 34, 18, "TOMADA");
    }
    else if (puedeConfirmarToma(indiceMedicamento, 0))
    {
        ComColor(180, 100, 0);
        ComTXT(250, y + 22, 26, 0,
               (estado->estado == PENDIENTE) ? "PENDIENTE" : "PREPARADA");
        ComColor(20, 150, 55);
        *botonConfirmar = Boton(365, y + 3, 95, 34, 18, "CONFIRMAR");
    }
    else
    {
        ComColor(90, 90, 90);
        ComTXT(250, y + 22, 26, 0,
               (estado->estado == PENDIENTE) ? "PENDIENTE" : "PROGRAMADA");
        ComColor(170, 170, 170);
        Boton(365, y + 3, 95, 34, 18,
              (estado->estado == PENDIENTE) ? "PLAZO FIN" : "ESPERAR");
    }
}

void dibujarPantallaInicio(void)
{
    uint8_t horas;
    uint8_t minutos;
    uint8_t segundos;
    uint8_t medicamentoAnterior;
    uint8_t horarioAnterior;
    uint8_t medicamentoSiguiente;
    uint8_t horarioSiguiente;
    uint8_t medicamentoPosterior;
    uint8_t horarioPosterior;
    char textoCabecera[24];
    char textoTemperatura[20];
    char *saludo;
    const char *dia;

    obtenerHora(&horas, &minutos, &segundos);
    switch (obtenerDiaSemana())
    {
    case DIA_MARTES: dia = "MARTES"; break;
    case DIA_MIERCOLES: dia = "MIERCOLES"; break;
    case DIA_JUEVES: dia = "JUEVES"; break;
    case DIA_VIERNES: dia = "VIERNES"; break;
    case DIA_SABADO: dia = "SABADO"; break;
    case DIA_DOMINGO: dia = "DOMINGO"; break;
    default: dia = "LUNES"; break;
    }

    saludo = (horas >= 6U && horas < 12U) ? "BUENOS DIAS" :
             (horas < 21U) ? "BUENAS TARDES" : "BUENAS NOCHES";
    sprintf(textoCabecera, "%s  %02u:%02u", dia,
            (unsigned int)horas, (unsigned int)minutos);

    Nueva_pantalla(248, 249, 251);
    ComColor(44, 92, 142);
    ComRect(0, 0, 480, 50, true);
    ComColor(255, 255, 255);
    ComTXT(18, 15, 29, 0, textoCabecera);
    ComTXT(455, 15, 29, OPT_RIGHTX, saludo);

    ComColor(255, 255, 255);
    ComRect(8, 58, 472, 190, true);
    ComColor(202, 212, 224);
    ComRect(8, 58, 472, 190, false);
    dibujarImagenPastillero(22, 72);

    dibujarResumenToma("ANTERIOR", 78,
                       obtenerTomaAnterior(&medicamentoAnterior, &horarioAnterior),
                       medicamentoAnterior, horarioAnterior);
    dibujarResumenToma("SIGUIENTE", 117,
                       obtenerTomaFuturaPorOrden(0U, &medicamentoSiguiente,
                                                  &horarioSiguiente),
                       medicamentoSiguiente, horarioSiguiente);
    dibujarResumenToma("DESPUES", 156,
                       obtenerTomaFuturaPorOrden(1U, &medicamentoPosterior,
                                                  &horarioPosterior),
                       medicamentoPosterior, horarioPosterior);

    ComColor(232, 238, 245);
    ComRect(8, 198, 350, 266, true);
    sprintf(textoTemperatura, "TEMPERATURA: %.1f C", T_act);
    ComColor(0, 0, 0);
    ComTXT(20, 208, 27, 0, textoTemperatura);
    if (T_act >= 30.0f)
    {
        ComColor(190, 45, 35);
        ComTXT(20, 238, 27, 0, "HACE CALOR: ABRA LA VENTANA");
    }
    else if (T_act <= 20.0f)
    {
        ComColor(35, 80, 170);
        ComTXT(20, 238, 27, 0, "HACE FRIO: ABRIGUESE");
    }
    else
    {
        ComColor(25, 115, 65);
        ComTXT(20, 238, 27, 0, "TEMPERATURA CONFORTABLE");
    }

    ComColor(224, 184, 65);
    BotonCuidador = Boton(360, 205, 110, 55, 26, "CUIDADOR");
    Dibuja();
}

void dibujarPantallaCuidador(void)
{
    BotonConfirmarA = 0;
    BotonConfirmarB = 0;
    BotonConfirmarC = 0;

    Nueva_pantalla(248, 249, 251);
    ComColor(44, 92, 142);
    ComRect(0, 0, 480, 44, true);
    ComColor(255, 255, 255);
    ComTXT(240, 12, 29, OPT_CENTERX, "MODO CUIDADOR");
    ComColor(0, 0, 0);
    ComTXT(18, 52, 26, 0, "MEDICAMENTOS DE HOY");

    dibujarFilaCuidador(0, 78, &BotonConfirmarA);
    dibujarFilaCuidador(1, 124, &BotonConfirmarB);
    dibujarFilaCuidador(2, 170, &BotonConfirmarC);

    ComColor(205, 75, 65);
    BotonVolver = Boton(10, 218, 220, 44, 26, "VOLVER A INICIO");
    /* La configuracion se implementara dentro de este modo en una fase posterior. */
    ComColor(155, 155, 155);
    (void)Boton(245, 218, 225, 44, 26, "CONFIGURAR PROX.");
    Dibuja();
}


