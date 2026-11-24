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
extern bool SensorDisponible;

#define IMAGEN_DIRECCION 0
#define IMAGEN_RGB565    7
#define BLOQUE_JPG       256
#define ESPERA_MAX       2000000UL
#define FILAS_CUIDADOR_VISIBLES 3U
#define NOMBRE_VISIBLE_MAXIMO   (MAX_NOMBRES_MEDICAMENTOS - 1U)

/* Estado de recursos y de interaccion tactil que solo pertenece a la interfaz. */
static bool imagenCargada = false;
int BotonCuidador=0;
int BotonVolver=0;
int BotonConfiguracion=0;
int BotonVolverConfiguracion=0;
int BotonCrearMedicamento=0;
int BotonEditarNombreMedicamento=0;
int BotonAceptarNombreMedicamento=0;
int BotonCancelarNombreMedicamento=0;
int BotonCambiarActividadMedicamento=0;
int BotonEditarMedicamento=0;
int BotonEliminarMedicamento=0;
int BotonVolverEdicionMedicamento=0;
int BotonAgregarHorario=0;
int BotonCambiarActividadHorario=0;
int BotonEditarDiasHorario=0;
int BotonVolverDiasHorario=0;
int BotonAlternarDiaHorario=0;
int BotonEliminarHorario=0;
int BotonGuardarMedicamento=0;
int BotonHoraMenos=0;
int BotonHoraMas=0;
int BotonMinutoMenos=0;
int BotonMinutoMas=0;
int BotonAceptarFechaHora=0;
int BotonConfirmarToma=0;
uint8_t IndiceMedicamentoConfirmar=0U;
uint8_t IndiceHorarioConfirmar=0U;
uint8_t IndiceMedicamentoEdicion=0U;
uint8_t IndiceHorarioEdicion=0U;
uint8_t DiaHorarioEdicion=0U;

static uint8_t diaEdicion;
static uint8_t mesEdicion;
static uint16_t anioEdicion;
static uint8_t horaEdicion;
static uint8_t minutoEdicion;
static bool estadoBotonFechaHora[11];
static uint8_t primeraTomaVisible = 0U;
static bool estadoBotonLista[5];
static bool estadoBotonEdicion[22];
static bool estadoBotonNavegacion[13];
static bool estadoBotonTeclado[17];
static bool confirmarBorradoMedicamento = false;
static bool confirmarBorradoHorario = false;
static char nombreEdicionMedicamento[MAX_NOMBRES_MEDICAMENTOS];
static uint8_t longitudNombreEdicion = 0U;
static uint8_t paginaTecladoNombre = 0U;

/* Convierte una pulsacion mantenida en un unico evento para cada boton. */
static int botonFechaHora(uint8_t indice, int x, int y, int ancho, int alto,
                           int fuente, char *texto)
{
    int pulsado = Boton(x, y, ancho, alto, fuente, texto);
    int evento = (pulsado != 0 && !estadoBotonFechaHora[indice]);

    estadoBotonFechaHora[indice] = (pulsado != 0);
    return evento;
}

/* Convierte las pulsaciones de desplazamiento y confirmacion en eventos unicos. */
static int botonLista(uint8_t indice, int x, int y, int ancho, int alto,
                      int fuente, char *texto)
{
    int pulsado = Boton(x, y, ancho, alto, fuente, texto);
    int evento = (pulsado != 0 && !estadoBotonLista[indice]);

    estadoBotonLista[indice] = (pulsado != 0);
    return evento;
}

/* Genera un unico evento para los botones exclusivos del editor. */
static int botonEdicion(uint8_t indice, int x, int y, int ancho, int alto,
                        int fuente, char *texto)
{
    int pulsado = Boton(x, y, ancho, alto, fuente, texto);
    int evento = (pulsado != 0 && !estadoBotonEdicion[indice]);

    estadoBotonEdicion[indice] = (pulsado != 0);
    return evento;
}

/* Para navegacion se genera el evento al soltar, evitando saltos entre pantallas al mantener pulsado. */
static int botonAlSoltar(uint8_t indice, int x, int y, int ancho, int alto,
                         int fuente, char *texto)
{
    int pulsado = Boton(x, y, ancho, alto, fuente, texto);
    int evento = (!pulsado && estadoBotonNavegacion[indice]);

    estadoBotonNavegacion[indice] = (pulsado != 0);
    return evento;
}

static int botonTeclado(uint8_t indice, int x, int y, int ancho, int alto,
                        int fuente, char *texto)
{
    int pulsado = Boton(x, y, ancho, alto, fuente, texto);
    int evento = (pulsado != 0 && !estadoBotonTeclado[indice]);

    estadoBotonTeclado[indice] = (pulsado != 0);
    return evento;
}

/* Añade un caracter al buffer del teclado sin superar el limite del nombre configurado. */
static void anadirCaracterNombre(char caracter)
{
    if (longitudNombreEdicion < (MAX_NOMBRES_MEDICAMENTOS - 1U))
    {
        nombreEdicionMedicamento[longitudNombreEdicion] = caracter;
        longitudNombreEdicion++;
        nombreEdicionMedicamento[longitudNombreEdicion] = '\0';
    }
}

static void dibujarFilaTeclasNombre(const char *letras, uint8_t cantidad,
                                    int y, uint8_t indiceInicial)
{
    uint8_t indice;
    int ancho = 58;
    int separacion = 9;
    int x = (480 - ((int)cantidad * ancho +
                    ((int)cantidad - 1) * separacion)) / 2;
    char texto[2];

    for (indice = 0U; indice < cantidad; indice++)
    {
        texto[0] = letras[indice];
        texto[1] = '\0';
        ComColor(44, 92, 142);
        if (botonTeclado((uint8_t)(indiceInicial + indice), x, y, ancho, 34,
                          26, texto))
        {
            anadirCaracterNombre(letras[indice]);
        }
        x += ancho + separacion;
    }
}

/* Corrige el dia seleccionado al cambiar mes o año para mantener una fecha valida. */
static void ajustarDiaEdicion(void)
{
    while (!fechaHoraEsValida(diaEdicion, mesEdicion, anioEdicion,
                              horaEdicion, minutoEdicion, 0U))
    {
        diaEdicion--;
    }
}

/* Prepara valores de partida editables; todavia no inicia el reloj. */
void inicializarPantallaFechaHora(void)
{
    uint8_t indice;

    diaEdicion = 1U;
    mesEdicion = 1U;
    anioEdicion = 2026U;
    horaEdicion = 0U;
    minutoEdicion = 0U;
    BotonAceptarFechaHora = 0;

    for (indice = 0U; indice < 11U; indice++)
    {
        estadoBotonFechaHora[indice] = false;
    }
}

/* Dibuja los controles obligatorios de fecha y hora antes de usar el asistente. */
void dibujarPantallaFechaHora(void)
{
    char texto[12];

    BotonAceptarFechaHora = 0;
    Nueva_pantalla(248, 249, 251);
    ComColor(44, 92, 142);
    ComRect(0, 0, 480, 36, true);
    ComColor(255, 255, 255);
    ComTXT(240, 10, 28, OPT_CENTERX, "INTRODUZCA LA FECHA Y HORA");
    ComColor(0, 0, 0);

    /* Cada dato se edita verticalmente: etiqueta, mas, valor y menos. */
    ComTXT(48, 45, 28, OPT_CENTERX, "DIA");
    ComTXT(144, 45, 28, OPT_CENTERX, "MES");
    /* La fuente interna no representa Ñ de forma fiable: la virgulilla se dibuja aparte. */
    ComTXT(240, 45, 28, OPT_CENTERX, "ANO");
    ComTXT(240, 34, 28, OPT_CENTERX, "~");
    ComTXT(336, 45, 28, OPT_CENTERX, "HORA");
    ComTXT(432, 45, 28, OPT_CENTERX, "MINUTO");
    ComColor(65, 120, 185);
    if (botonFechaHora(1U, 23, 75, 50, 29, 28, "+"))
    {
        diaEdicion++;
        if (!fechaHoraEsValida(diaEdicion, mesEdicion, anioEdicion, horaEdicion, minutoEdicion, 0U))
        {
            diaEdicion = 1U;
        }
    }
    if (botonFechaHora(3U, 119, 75, 50, 29, 28, "+"))
    {
        if (mesEdicion == 12U)
        {
            mesEdicion = 1U;
        }
        else
        {
            mesEdicion = (uint8_t)(mesEdicion + 1U);
        }
        ajustarDiaEdicion();
    }
    if (botonFechaHora(5U, 215, 75, 50, 29, 28, "+"))
    {
        if (anioEdicion == 2099U)
        {
            anioEdicion = 2000U;
        }
        else
        {
            anioEdicion = (uint16_t)(anioEdicion + 1U);
        }
        ajustarDiaEdicion();
    }
    if (botonFechaHora(7U, 311, 75, 50, 29, 28, "+"))
    {
        if (horaEdicion == 23U)
        {
            horaEdicion = 0U;
        }
        else
        {
            horaEdicion = (uint8_t)(horaEdicion + 1U);
        }
    }
    if (botonFechaHora(9U, 407, 75, 50, 29, 28, "+"))
    {
        if (minutoEdicion == 59U)
        {
            minutoEdicion = 0U;
        }
        else
        {
            minutoEdicion = (uint8_t)(minutoEdicion + 1U);
        }
    }
    ComColor(0, 0, 0);
    sprintf(texto, "%02u", (unsigned int)horaEdicion);
    ComTXT(336, 118, 28, OPT_CENTERX, texto);
    sprintf(texto, "%02u", (unsigned int)minutoEdicion);
    ComTXT(432, 118, 28, OPT_CENTERX, texto);
    sprintf(texto, "%02u", (unsigned int)diaEdicion);
    ComTXT(48, 118, 28, OPT_CENTERX, texto);
    sprintf(texto, "%02u", (unsigned int)mesEdicion);
    ComTXT(144, 118, 28, OPT_CENTERX, texto);
    sprintf(texto, "%u", (unsigned int)anioEdicion);
    ComTXT(240, 118, 28, OPT_CENTERX, texto);

    ComColor(65, 120, 185);
    if (botonFechaHora(0U, 23, 160, 50, 29, 28, "-"))
    {
        if (diaEdicion > 1U)
        {
            diaEdicion--;
        }
        else
        {
            diaEdicion = 31U;
            ajustarDiaEdicion();
        }
    }
    if (botonFechaHora(2U, 119, 160, 50, 29, 28, "-"))
    {
        if (mesEdicion == 1U)
        {
            mesEdicion = 12U;
        }
        else
        {
            mesEdicion = (uint8_t)(mesEdicion - 1U);
        }
        ajustarDiaEdicion();
    }
    if (botonFechaHora(4U, 215, 160, 50, 29, 28, "-"))
    {
        if (anioEdicion == 2000U)
        {
            anioEdicion = 2099U;
        }
        else
        {
            anioEdicion = (uint16_t)(anioEdicion - 1U);
        }
        ajustarDiaEdicion();
    }
    if (botonFechaHora(6U, 311, 160, 50, 29, 28, "-"))
    {
        if (horaEdicion == 0U)
        {
            horaEdicion = 23U;
        }
        else
        {
            horaEdicion = (uint8_t)(horaEdicion - 1U);
        }
    }
    if (botonFechaHora(8U, 407, 160, 50, 29, 28, "-"))
    {
        if (minutoEdicion == 0U)
        {
            minutoEdicion = 59U;
        }
        else
        {
            minutoEdicion = (uint8_t)(minutoEdicion - 1U);
        }
    }

    ComColor(35, 145, 75);
    if (botonAlSoltar(11U, 145, 220, 185, 35, 28, "ACEPTAR"))
    {
        if (establecerFechaHora(diaEdicion, mesEdicion, anioEdicion,
                                horaEdicion, minutoEdicion, 0U))
        {
            BotonAceptarFechaHora = 1;
        }
    }
    Dibuja();
}


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

/* Copia un nombre corto para que no invada la zona del boton de cada fila. */
static void copiarNombreVisible(char destino[NOMBRE_VISIBLE_MAXIMO + 4U],
                                const char *origen)
{
    uint8_t posicion = 0U;

    while (origen[posicion] != '\0' && posicion < NOMBRE_VISIBLE_MAXIMO)
    {
        destino[posicion] = origen[posicion];
        posicion++;
    }

    if (origen[posicion] != '\0')
    {
        destino[posicion++] = '.';
        destino[posicion++] = '.';
        destino[posicion++] = '.';
    }

    destino[posicion] = '\0';
}

/* Dibuja una toma concreta de la lista y conserva sus indices al confirmarla. */
static void dibujarFilaCuidador(uint8_t indiceMedicamento,
                                uint8_t indiceHorario, int y,
                                uint8_t indiceBoton)
{
    char texto[24];
    char nombreVisible[NOMBRE_VISIBLE_MAXIMO + 4U];
    char *estadoTexto;
    char *textoBoton;
    const MedicConfig *medicamento;
    const HorarioConfig *horario;
    const EstadoDiarioHorario *estado;

    medicamento = obtenerMedicamentoConfigurado(indiceMedicamento);
    horario = obtenerHorarioConfigurado(indiceMedicamento, indiceHorario);
    estado = obtenerEstadoDiarioHorario(indiceMedicamento, indiceHorario);

    if (medicamento == NULL || horario == NULL || estado == NULL)
    {
        return;
    }

    ComColor(229, 233, 239);
    ComRect(10, y, 405, y + 40, true);
    sprintf(texto, "%02u:%02u", (unsigned int)horario->hora,
            (unsigned int)horario->minuto);
    copiarNombreVisible(nombreVisible, medicamento->nombre);
    ComColor(0, 0, 0);
    ComTXT(18, y + 3, 28, 0, texto);
    ComTXT(92, y + 3, 26, 0, nombreVisible);

    if (estado->estado == CONFIRMADA)
    {
        ComColor(20, 130, 55);
        estadoTexto = "CONFIRMADA";
        ComTXT(92, y + 22, 26, 0, estadoTexto);
        ComColor(155, 190, 130);
        ComButton(290, y + 3, 105, 34, 26, 0, "TOMADA");
    }
    else if (puedeConfirmarToma(indiceMedicamento, indiceHorario))
    {
        ComColor(180, 100, 0);
        if (estado->estado == PENDIENTE)
        {
            estadoTexto = "PENDIENTE";
        }
        else
        {
            estadoTexto = "PREPARADA";
        }
        ComTXT(92, y + 22, 26, 0, estadoTexto);
        ComColor(20, 150, 55);
        if (botonLista(indiceBoton, 290, y + 3, 105, 34, 26, "CONFIRMAR"))
        {
            IndiceMedicamentoConfirmar = indiceMedicamento;
            IndiceHorarioConfirmar = indiceHorario;
            BotonConfirmarToma = 1;
        }
    }
    else
    {
        ComColor(90, 90, 90);
        if (estado->estado == EXPIRADA)
        {
            estadoTexto = "EXPIRADA";
            textoBoton = "VENCIDA";
        }
        else if (estado->estado == PENDIENTE)
        {
            estadoTexto = "PENDIENTE";
            textoBoton = "PLAZO FIN";
        }
        else
        {
            estadoTexto = "PROGRAMADA";
            textoBoton = "ESPERAR";
        }
        ComTXT(92, y + 22, 26, 0, estadoTexto);
        ComColor(170, 170, 170);
        ComButton(290, y + 3, 105, 34, 26, 0, textoBoton);
    }
}

/* Pantalla principal: resume fecha, tomas cercanas, temperatura y acceso al cuidador. */
void dibujarPantallaInicio(void)
{
    uint8_t horas;
    uint8_t minutos;
    uint8_t segundos;
    uint8_t dia;
    uint8_t mes;
    uint16_t anio;
    uint8_t medicamentoAnterior;
    uint8_t horarioAnterior;
    uint8_t medicamentoSiguiente;
    uint8_t horarioSiguiente;
    uint8_t medicamentoPosterior;
    uint8_t horarioPosterior;
    char textoCabecera[24];
    char textoTemperatura[20];
    char *saludo;

    obtenerHora(&horas, &minutos, &segundos);
    obtenerFecha(&dia, &mes, &anio);

    if (horas >= 6U && horas < 12U)
    {
        saludo = "BUENOS DIAS";
    }
    else if (horas < 21U)
    {
        saludo = "BUENAS TARDES";
    }
    else
    {
        saludo = "BUENAS NOCHES";
    }
    sprintf(textoCabecera, "%02u/%02u/%04u  %02u:%02u",
            (unsigned int)dia, (unsigned int)mes, (unsigned int)anio,
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
    if (!SensorDisponible)
    {
        ComColor(90, 90, 90);
        ComTXT(20, 220, 27, 0, "SENSOR NO DISPONIBLE");
    }
    else
    {
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
    }

    ComColor(224, 184, 65);
    BotonCuidador = botonAlSoltar(0U, 360, 205, 110, 55, 26, "CUIDADOR");
    Dibuja();
}

/* Lista desplazable de las tomas del dia con su estado y boton de confirmacion. */
void dibujarPantallaCuidador(void)
{
    uint8_t totalTomas;
    uint8_t ultimaPrimeraToma;
    uint8_t fila;
    uint8_t indiceMedicamento;
    uint8_t indiceHorario;
    uint8_t ultimaTomaMostrada;
    char textoPagina[16];

    BotonConfirmarToma = 0;
    totalTomas = contarTomasDelDia();

    if (totalTomas <= FILAS_CUIDADOR_VISIBLES)
    {
        primeraTomaVisible = 0U;
    }
    else
    {
        ultimaPrimeraToma = (uint8_t)(totalTomas - FILAS_CUIDADOR_VISIBLES);
        if (primeraTomaVisible > ultimaPrimeraToma)
        {
            primeraTomaVisible = ultimaPrimeraToma;
        }
    }

    Nueva_pantalla(248, 249, 251);
    ComColor(44, 92, 142);
    ComRect(0, 0, 480, 40, true);
    ComColor(255, 255, 255);
    ComTXT(240, 8, 30, OPT_CENTERX, "Medicamentos de hoy");


    /* Las flechas compactas desplazan una fila y sustituyen a la barra tactil. */
    if (primeraTomaVisible > 0U && botonLista(0U, 420, 67, 50, 36, 29, "^"))
    {
        primeraTomaVisible--;
    }
    else if (primeraTomaVisible == 0U)
    {
        ComColor(170, 170, 170);
        ComButton(420, 67, 50, 36, 29, 0, "^");
    }

    if (totalTomas > FILAS_CUIDADOR_VISIBLES && primeraTomaVisible < (uint8_t)(totalTomas - FILAS_CUIDADOR_VISIBLES) &&
            botonLista(1U, 420, 118, 50, 36, 28, "v"))
    {
        primeraTomaVisible++;
    }
    else if (totalTomas <= FILAS_CUIDADOR_VISIBLES || primeraTomaVisible >= (uint8_t)(totalTomas - FILAS_CUIDADOR_VISIBLES))
    {
        ComColor(170, 170, 170);
        ComButton(420, 118, 50, 36, 28, 0, "v");
    }

    for (fila = 0U; fila < FILAS_CUIDADOR_VISIBLES; fila++)
    {
        if (primeraTomaVisible + fila >= totalTomas ||!obtenerTomaDelDiaPorOrden((uint8_t)(primeraTomaVisible + fila),&indiceMedicamento, &indiceHorario))
        {
            continue;
        }

        dibujarFilaCuidador(indiceMedicamento, indiceHorario, 64 + (int)fila * 46, (uint8_t)(fila + 2U));
    }

    if (totalTomas == 0U)
    {
        ComColor(90, 90, 90);
        ComTXT(205, 118, 27, OPT_CENTERX, "SIN TOMAS PROGRAMADAS HOY");
    }
    else
    {
        ultimaTomaMostrada = primeraTomaVisible + FILAS_CUIDADOR_VISIBLES;
        if (ultimaTomaMostrada > totalTomas)
        {
            ultimaTomaMostrada = totalTomas;
        }

        sprintf(textoPagina, "%u-%u/%u",
                (unsigned int)(primeraTomaVisible + 1U),
                (unsigned int)ultimaTomaMostrada,
                (unsigned int)totalTomas);

        ComColor(0, 0, 0);
        ComTXT(445, 174, 26, OPT_CENTERX, textoPagina);
    }

    ComColor(205, 75, 65);
    BotonVolver = botonAlSoltar(2U, 10, 218, 215, 44, 26, "VOLVER");
    ComColor(44, 92, 142);
    BotonConfiguracion = botonAlSoltar(1U, 240, 218, 230, 44, 26,
                                        "CONFIGURAR");
    Dibuja();
}

/* Inicializa el buffer del teclado con el nombre actual o vacio para una nueva creacion. */
void iniciarEdicionNombreMedicamento(const char *nombreInicial)
{
    uint8_t indice;

    longitudNombreEdicion = 0U;
    nombreEdicionMedicamento[0] = '\0';
    paginaTecladoNombre = 0U;
    BotonAceptarNombreMedicamento = 0;
    BotonCancelarNombreMedicamento = 0;

    for (indice = 0U; indice < 17U; indice++)
    {
        estadoBotonTeclado[indice] = false;
    }

    if (nombreInicial != NULL)
    {
        while (nombreInicial[longitudNombreEdicion] != '\0' &&
               longitudNombreEdicion < (MAX_NOMBRES_MEDICAMENTOS - 1U))
        {
            nombreEdicionMedicamento[longitudNombreEdicion] =
                nombreInicial[longitudNombreEdicion];
            longitudNombreEdicion++;
        }
        nombreEdicionMedicamento[longitudNombreEdicion] = '\0';
    }
}

/* Devuelve el texto temporal del teclado; main.c decide si aplicarlo o descartarlo. */
const char *obtenerNombreEdicionMedicamento(void)
{
    return nombreEdicionMedicamento;
}

/* Teclado en pantalla de dos paginas para editar nombres sin necesidad de UART. */
void dibujarPantallaNombreMedicamento(void)
{
    const char *primeraFila;
    const char *segundaFila;
    char *textoCambioPagina;
    uint8_t cantidadSegundaFila;

    BotonAceptarNombreMedicamento = 0;
    BotonCancelarNombreMedicamento = 0;

    Nueva_pantalla(248, 249, 251);
    ComColor(44, 92, 142);
    ComRect(0, 0, 480, 36, true);
    ComColor(255, 255, 255);
    ComTXT(240, 7, 28, OPT_CENTERX, "NOMBRE MEDICAMENTO");

    ComColor(0, 0, 0);
    ComTXT(10, 45, 26, 0, "NOMBRE:");
    ComTXT(145, 45, 26, 0, nombreEdicionMedicamento);

    if (paginaTecladoNombre == 0U)
    {
        primeraFila = "ABCDEFG";
        segundaFila = "HIJKLMN";
        cantidadSegundaFila = 7U;
        textoCambioPagina = "O-Z";
    }
    else
    {
        primeraFila = "OPQRSTU";
        segundaFila = "VWXYZ";
        cantidadSegundaFila = 5U;
        textoCambioPagina = "A-N";
    }

    dibujarFilaTeclasNombre(primeraFila, 7U, 85, 0U);
    dibujarFilaTeclasNombre(segundaFila, cantidadSegundaFila, 126, 7U);

    ComColor(44, 92, 142);
    if (botonTeclado(14U, 10, 172, 135, 34, 26, textoCambioPagina))
    {
        if (paginaTecladoNombre == 0U)
        {
            paginaTecladoNombre = 1U;
        }
        else
        {
            paginaTecladoNombre = 0U;
        }
    }
    if (botonTeclado(15U, 155, 172, 150, 34, 26, "ESPACIO"))
    {
        anadirCaracterNombre(' ');
    }
    ComColor(205, 75, 65);
    if (botonTeclado(16U, 315, 172, 155, 34, 26, "BORRAR"))
    {
        if (longitudNombreEdicion > 0U)
        {
            longitudNombreEdicion--;
            nombreEdicionMedicamento[longitudNombreEdicion] = '\0';
        }
    }

    ComColor(120, 120, 120);
    BotonCancelarNombreMedicamento = botonAlSoltar(6U, 10, 220, 210, 40, 26,
                                                    "CANCELAR");
    if (longitudNombreEdicion == 0U)
    {
        ComColor(150, 150, 150);
        ComButton(260, 220, 210, 40, 26, 0, "ACEPTAR");
    }
    else
    {
        ComColor(35, 145, 75);
        BotonAceptarNombreMedicamento = botonAlSoltar(7U, 260, 220, 210, 40,
                                                       26, "ACEPTAR");
    }
    Dibuja();
}

/*
 * Pantalla de seleccion de medicamento.
 * Desde aqui el cuidador crea un hueco, activa/desactiva el medicamento completo
 * o entra en su editor de horarios. El borrado requiere una segunda pulsacion
 * para evitar eliminar tratamientos por error.
 */
void dibujarPantallaConfiguracion(void)
{
    const MedicConfig *medicamento;
    char texto[32];
    char nombreMedicamento[MAX_NOMBRES_MEDICAMENTOS];
    char *textoActividad;
    char *textoBotonActividad;
    int colorRojo;
    int colorVerde;
    int colorAzul;

    BotonCrearMedicamento = 0;
    BotonCambiarActividadMedicamento = 0;
    BotonEditarMedicamento = 0;
    BotonEliminarMedicamento = 0;

    Nueva_pantalla(248, 249, 251);

    ComColor(44, 92, 142);
    ComRect(0, 0, 480, 40, true);
    ComColor(255, 255, 255);
    ComTXT(240, 8, 30, OPT_CENTERX, "CONFIGURACION");

    /* Las flechas permiten recorrer tambien los huecos libres de los ocho slots. */
    ComColor(44, 92, 142);
    if (botonEdicion(0U, 10, 55, 55, 36, 28, "<"))
    {
        if (IndiceMedicamentoEdicion == 0U)
        {
            IndiceMedicamentoEdicion = MAX_MEDICAMENTOS - 1U;
        }
        else
        {
            IndiceMedicamentoEdicion =
                (uint8_t)(IndiceMedicamentoEdicion - 1U);
        }
        confirmarBorradoMedicamento = false;
    }
    if (botonEdicion(1U, 415, 55, 55, 36, 28, ">"))
    {
        IndiceMedicamentoEdicion =
            (uint8_t)((IndiceMedicamentoEdicion + 1U) % MAX_MEDICAMENTOS);
        confirmarBorradoMedicamento = false;
    }

    sprintf(texto, "MEDICAMENTO %u/%u",
            (unsigned int)(IndiceMedicamentoEdicion + 1U),
            (unsigned int)MAX_MEDICAMENTOS);
    ComColor(55, 55, 55);
    ComTXT(240, 58, 26, OPT_CENTERX, texto);

    medicamento = obtenerMedicamentoParaEdicion(IndiceMedicamentoEdicion);
    if (confirmarBorradoMedicamento && medicamento != NULL)
    {
        sprintf(nombreMedicamento, "%s", medicamento->nombre);
        ComColor(248, 249, 251);
        ComRect(25, 70, 455, 208, true);
        ComColor(44, 92, 142);
        ComRect(25, 70, 455, 208, false);
        ComColor(0, 0, 0);
        ComTXT(240, 82, 28, OPT_CENTERX, "QUIERE ELIMINAR?");
        ComTXT(240, 116, 29, OPT_CENTERX, nombreMedicamento);
        ComTXT(240, 145, 26, OPT_CENTERX, "TAMBIEN SUS HORARIOS");

        ComColor(120, 120, 120);
        if (botonEdicion(5U, 40, 160, 180, 38, 26, "CANCELAR"))
        {
            confirmarBorradoMedicamento = false;
        }
        ComColor(205, 75, 65);
        if (botonEdicion(6U, 260, 160, 180, 38, 26, "CONFIRMAR"))
        {
            BotonEliminarMedicamento = 1;
            confirmarBorradoMedicamento = false;
        }
    }
    else if (medicamento == NULL)
    {
        ComColor(90, 90, 90);
        ComTXT(240, 108, 29, OPT_CENTERX, "HUECO LIBRE");
        ComTXT(240, 140, 26, OPT_CENTERX, "SIN HORARIOS");
        ComColor(35, 145, 75);
        if (botonAlSoltar(5U, 120, 168, 240, 38, 26, "CREAR NUEVO"))
        {
            BotonCrearMedicamento = 1;
        }
    }
    else
    {
        /* ComTXT usa una API antigua que recibe char *, no const char *. */
        sprintf(nombreMedicamento, "%s", medicamento->nombre);
        ComColor(0, 0, 0);
        ComTXT(240, 96, 29, OPT_CENTERX, nombreMedicamento);
        sprintf(texto, "HORARIOS: %u", (unsigned int)medicamento->horariosUsados);
        ComColor(70, 70, 70);
        ComTXT(240, 127, 26, OPT_CENTERX, texto);
        if (medicamento->activo)
        {
            textoActividad = "ACTIVO";
            textoBotonActividad = "DESACTIVAR";
            colorRojo = 180;
            colorVerde = 100;
            colorAzul = 0;
        }
        else
        {
            textoActividad = "DESACTIVADO";
            textoBotonActividad = "ACTIVAR";
            colorRojo = 35;
            colorVerde = 145;
            colorAzul = 75;
        }

        ComTXT(240, 150, 26, OPT_CENTERX, textoActividad);
        ComColor(colorRojo, colorVerde, colorAzul);
        if (botonEdicion(2U, 10, 168, 145, 38, 26,
                          textoBotonActividad))
        {
            BotonCambiarActividadMedicamento = 1;
        }

        ComColor(44, 92, 142);
        if (botonAlSoltar(4U, 167, 168, 145, 38, 26, "EDITAR"))
        {
            BotonEditarMedicamento = 1;
        }

        ComColor(205, 75, 65);
        if (botonEdicion(4U, 324, 168, 145, 38, 26, "ELIMINAR"))
        {
            confirmarBorradoMedicamento = true;
        }
    }

    if (confirmarBorradoMedicamento)
    {
        ComColor(150, 150, 150);
        ComButton(125, 218, 230, 44, 26, 0, "VOLVER A CUIDADOR");
    }
    else
    {
        ComColor(205, 75, 65);
        BotonVolverConfiguracion = botonAlSoltar(3U, 125, 218, 230, 44, 26,
                                                  "VOLVER A CUIDADOR");
    }
    Dibuja();
}

/*
 * Editor de los hasta tres horarios de un medicamento.
 * Cada boton solo solicita una accion; main.c aplica el cambio, actualiza el
 * estado diario y persiste el resultado. Asi la pantalla no conoce EEPROM.
 */
void dibujarPantallaEdicionMedicamento(void)
{
    const MedicConfig *medicamento;
    const HorarioConfig *horario = NULL;
    bool horarioActivo;
    char texto[32];
    char nombreMedicamento[MAX_NOMBRES_MEDICAMENTOS];
    char *textoBotonActividad;
    int colorRojo;
    int colorVerde;
    int colorAzul;

    BotonAgregarHorario = 0;
    BotonCambiarActividadHorario = 0;
    BotonEditarDiasHorario = 0;
    BotonEditarNombreMedicamento = 0;
    BotonEliminarHorario = 0;
    BotonGuardarMedicamento = 0;
    BotonHoraMenos = 0;
    BotonHoraMas = 0;
    BotonMinutoMenos = 0;
    BotonMinutoMas = 0;

    Nueva_pantalla(248, 249, 251);
    ComColor(44, 92, 142);
    ComRect(0, 0, 480, 40, true);
    ComColor(255, 255, 255);
    ComTXT(240, 8, 30, OPT_CENTERX, "EDITAR HORARIOS");

    medicamento = obtenerMedicamentoParaEdicion(IndiceMedicamentoEdicion);
    if (medicamento == NULL)
    {
        /* No deberia ocurrir al entrar desde EDITAR; permite volver con seguridad. */
        ComColor(90, 90, 90);
        ComTXT(240, 110, 26, OPT_CENTERX, "MEDICAMENTO NO DISPONIBLE");
    }
    else
    {
        sprintf(nombreMedicamento, "%s", medicamento->nombre);
        ComColor(0, 0, 0);
        ComTXT(240, 45, 26, OPT_CENTERX, nombreMedicamento);

        ComColor(44, 92, 142);
        if (botonEdicion(4U, 10, 70, 55, 36, 28, "<"))
        {
            if (IndiceHorarioEdicion == 0U)
            {
                IndiceHorarioEdicion = MAX_HORARIOS_MEDICAMENTOS - 1U;
            }
            else
            {
                IndiceHorarioEdicion = (uint8_t)(IndiceHorarioEdicion - 1U);
            }
            confirmarBorradoHorario = false;
        }
        if (botonEdicion(5U, 415, 70, 55, 36, 28, ">"))
        {
            if (IndiceHorarioEdicion + 1U == MAX_HORARIOS_MEDICAMENTOS)
            {
                IndiceHorarioEdicion = 0U;
            }
            else
            {
                IndiceHorarioEdicion = (uint8_t)(IndiceHorarioEdicion + 1U);
            }
            confirmarBorradoHorario = false;
        }

        sprintf(texto, "HORARIO %u/%u",
                (unsigned int)(IndiceHorarioEdicion + 1U),
                (unsigned int)MAX_HORARIOS_MEDICAMENTOS);
        ComColor(55, 55, 55);
        ComTXT(240, 68, 26, OPT_CENTERX, texto);

        horario = obtenerHorarioParaEdicion(IndiceMedicamentoEdicion,
                                             IndiceHorarioEdicion);
        if (horario != NULL)
        {
                horarioActivo =
                    ((horario->diasSemana & HORARIO_DESACTIVADO) == 0U);
                sprintf(texto, "%02u:%02u", (unsigned int)horario->hora,
                        (unsigned int)horario->minuto);
                ComColor(0, 0, 0);
                ComTXT(240, 88, 31, OPT_CENTERX, texto);

                /* Los cuatro controles cambian una unidad y main.c conserva los dias. */
                ComColor(44, 92, 142);
                if (botonEdicion(6U, 10, 140, 105, 36, 26, "HORA -"))
                {
                    BotonHoraMenos = 1;
                }
                if (botonEdicion(7U, 125, 140, 105, 36, 26, "HORA +"))
                {
                    BotonHoraMas = 1;
                }
                if (botonEdicion(8U, 240, 140, 105, 36, 26, "MIN -"))
                {
                    BotonMinutoMenos = 1;
                }
                if (botonEdicion(9U, 355, 140, 115, 36, 26, "MIN +"))
                {
                    BotonMinutoMas = 1;
                }

                if (horarioActivo)
                {
                    textoBotonActividad = "DESACTIVAR";
                    colorRojo = 180;
                    colorVerde = 100;
                    colorAzul = 0;
                }
                else
                {
                    textoBotonActividad = "ACTIVAR";
                    colorRojo = 35;
                    colorVerde = 145;
                    colorAzul = 75;
                }

                ComColor(colorRojo, colorVerde, colorAzul);
                if (botonEdicion(10U, 10, 185, 145, 36, 26,
                                  textoBotonActividad))
                {
                    BotonCambiarActividadHorario = 1;
                }

                ComColor(44, 92, 142);
                if (botonAlSoltar(9U, 167, 185, 135, 36, 26, "DIAS"))
                {
                    BotonEditarDiasHorario = 1;
                }
                ComColor(44, 92, 142);
                if (botonAlSoltar(12U, 312, 185, 158, 36, 26, "NOMBRE"))
                {
                    BotonEditarNombreMedicamento = 1;
                }
        }
        else
        {
            ComColor(90, 90, 90);
            ComTXT(240, 110, 28, OPT_CENTERX, "HUECO LIBRE");
            ComColor(35, 145, 75);
            if (botonEdicion(12U, 120, 170, 240, 42, 26, "ANADIR HORARIO"))
            {
                BotonAgregarHorario = 1;
            }
        }
    }

    if (horario != NULL)
    {
        ComColor(205, 75, 65);
        if (!confirmarBorradoHorario)
        {
            if (botonEdicion(13U, 10, 228, 145, 36, 26, "BORRAR"))
            {
                confirmarBorradoHorario = true;
            }
        }
        else
        {
            if (botonEdicion(13U, 10, 228, 145, 36, 26, "CONFIRMAR?"))
            {
                BotonEliminarHorario = 1;
                confirmarBorradoHorario = false;
            }
        }
    }

    ComColor(35, 145, 75);
    if (botonEdicion(21U, 167, 228, 145, 36, 26, "GUARDAR"))
    {
        BotonGuardarMedicamento = 1;
    }
    ComColor(205, 75, 65);
    BotonVolverEdicionMedicamento = botonAlSoltar(8U, 324, 228, 145, 36, 26,
                                                   "VOLVER");
    Dibuja();
}

/* Editor de la mascara semanal: permite activar o desactivar cada dia de un horario. */
void dibujarPantallaDiasHorario(void)
{
    const HorarioConfig *horario;
    uint8_t diasSemana;

    BotonAlternarDiaHorario = 0;
    Nueva_pantalla(248, 249, 251);
    ComColor(44, 92, 142);
    ComRect(0, 0, 480, 40, true);
    ComColor(255, 255, 255);
    ComTXT(240, 8, 30, OPT_CENTERX, "DIAS DEL HORARIO");

    horario = obtenerHorarioParaEdicion(IndiceMedicamentoEdicion,
                                         IndiceHorarioEdicion);
    if (horario == NULL)
    {
        ComColor(90, 90, 90);
        ComTXT(240, 105, 26, OPT_CENTERX, "HORARIO NO DISPONIBLE");
    }
    else
    {
        diasSemana = horario->diasSemana & DIA_TODOS;
        ComColor(55, 55, 55);
        ComTXT(240, 55, 26, OPT_CENTERX, "SELECCIONE LOS DIAS");

        if ((diasSemana & DIA_LUNES) != 0U)
        {
            ComColor(35, 145, 75);
        }
        else
        {
            ComColor(150, 150, 150);
        }
        if (botonEdicion(14U, 10, 100, 105, 38, 26, "LUN"))
        {
            DiaHorarioEdicion = DIA_LUNES;
            BotonAlternarDiaHorario = 1;
        }
        if ((diasSemana & DIA_MARTES) != 0U)
        {
            ComColor(35, 145, 75);
        }
        else
        {
            ComColor(150, 150, 150);
        }
        if (botonEdicion(15U, 125, 100, 105, 38, 26, "MAR"))
        {
            DiaHorarioEdicion = DIA_MARTES;
            BotonAlternarDiaHorario = 1;
        }
        if ((diasSemana & DIA_MIERCOLES) != 0U)
        {
            ComColor(35, 145, 75);
        }
        else
        {
            ComColor(150, 150, 150);
        }
        if (botonEdicion(16U, 240, 100, 105, 38, 26, "MIE"))
        {
            DiaHorarioEdicion = DIA_MIERCOLES;
            BotonAlternarDiaHorario = 1;
        }
        if ((diasSemana & DIA_JUEVES) != 0U)
        {
            ComColor(35, 145, 75);
        }
        else
        {
            ComColor(150, 150, 150);
        }
        if (botonEdicion(17U, 355, 100, 105, 38, 26, "JUE"))
        {
            DiaHorarioEdicion = DIA_JUEVES;
            BotonAlternarDiaHorario = 1;
        }
        if ((diasSemana & DIA_VIERNES) != 0U)
        {
            ComColor(35, 145, 75);
        }
        else
        {
            ComColor(150, 150, 150);
        }
        if (botonEdicion(18U, 50, 150, 120, 38, 26, "VIE"))
        {
            DiaHorarioEdicion = DIA_VIERNES;
            BotonAlternarDiaHorario = 1;
        }
        if ((diasSemana & DIA_SABADO) != 0U)
        {
            ComColor(35, 145, 75);
        }
        else
        {
            ComColor(150, 150, 150);
        }
        if (botonEdicion(19U, 180, 150, 120, 38, 26, "SAB"))
        {
            DiaHorarioEdicion = DIA_SABADO;
            BotonAlternarDiaHorario = 1;
        }
        if ((diasSemana & DIA_DOMINGO) != 0U)
        {
            ComColor(35, 145, 75);
        }
        else
        {
            ComColor(150, 150, 150);
        }
        if (botonEdicion(20U, 310, 150, 120, 38, 26, "DOM"))
        {
            DiaHorarioEdicion = DIA_DOMINGO;
            BotonAlternarDiaHorario = 1;
        }
    }

    ComColor(205, 75, 65);
    BotonVolverDiasHorario = botonAlSoltar(10U, 110, 220, 260, 40, 26,
                                            "VOLVER A HORARIOS");
    Dibuja();
}


