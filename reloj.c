#include "reloj.h"

/* Estado privado del reloj: solo este módulo puede modificar fecha y hora. */
static uint8_t horas;
static uint8_t minutos;
static uint8_t segundos;
static uint8_t diaMes;
static uint8_t mesActual;
static uint16_t anioActual;
static uint8_t diaSemanaActual;
static uint32_t numeroDiaActual;
static bool relojConfigurado;

/* Indica si un anio tiene 29 dias en febrero segun el calendario gregoriano. */
static bool esAnioBisiesto(uint16_t anio)
{
    return ((anio % 4U) == 0U &&
            (((anio % 100U) != 0U) || ((anio % 400U) == 0U)));
}

/* Devuelve cuantos dias tiene el mes indicado del anio indicado. */
static uint8_t obtenerDiasDelMes(uint8_t mes, uint16_t anio)
{
    static const uint8_t diasPorMes[12] =
    {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };

    if (mes == 2U && esAnioBisiesto(anio))
    {
        return 29U;
    }

    return diasPorMes[mes - 1U];
}

/* Calcula el dia semanal como mascara: lunes, martes, ..., domingo. */
static uint8_t calcularDiaSemana(uint8_t dia, uint8_t mes, uint16_t anio)
{
    uint16_t anioAjustado = anio;
    uint8_t mesAjustado = mes;
    uint16_t siglo;
    uint16_t anioSiglo;
    uint8_t resultado;

    /* Algoritmo de Sakamoto: enero y febrero cuentan como meses del anio previo. */
    static const uint8_t desplazamientoMes[12] =
    {
        0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U
    };

    if (mesAjustado < 3U)
    {
        anioAjustado--;
    }

    siglo = anioAjustado / 100U;
    anioSiglo = anioAjustado % 100U;
    resultado = (uint8_t)((anioSiglo + anioSiglo / 4U + siglo / 4U +
                           5U * siglo + desplazamientoMes[mesAjustado - 1U] +
                           dia) % 7U);

    /* El algoritmo devuelve domingo=0; el proyecto usa lunes=bit 0. */
    if (resultado == 0U)
    {
        return DIA_DOMINGO;
    }

    return (uint8_t)(1U << (resultado - 1U));
}

/* Avanza la fecha una vez que el reloj ha llegado a las 00:00:00. */
static void avanzarFecha(void)
{
    diaMes++;

    if (diaMes > obtenerDiasDelMes(mesActual, anioActual))
    {
        diaMes = 1U;
        mesActual++;

        if (mesActual > 12U)
        {
            mesActual = 1U;
            anioActual++;
        }
    }

    numeroDiaActual++;
    diaSemanaActual = calcularDiaSemana(diaMes, mesActual, anioActual);
}

/* Deja el reloj sin configurar hasta que la interfaz reciba una fecha válida. */
void inicializarReloj(void)
{
    horas = 0U;
    minutos = 0U;
    segundos = 0U;
    diaMes = 0U;
    mesActual = 0U;
    anioActual = 0U;
    diaSemanaActual = DIA_LUNES;
    numeroDiaActual = 0U;
    relojConfigurado = false;
}

/* Avanza exactamente un segundo y propaga los cambios de minuto, día, mes y año. */
void actualizarReloj(void)
{
    /* No se contabiliza tiempo hasta que la pantalla inicial acepta una fecha valida. */
    if (!relojConfigurado)
    {
        return;
    }

    segundos++;

    if (segundos == 60)
    {
        segundos = 0;
        minutos++;

        if (minutos == 60)
        {
            minutos = 0;
            horas++;

            if (horas == 24)
            {
                horas = 0;
                avanzarFecha();
            }
        }
    }
}

/* Expone una copia de la hora sin permitir modificar el estado interno directamente. */
void obtenerHora(uint8_t *h, uint8_t *m, uint8_t *s)
{
    *h = horas;
    *m = minutos;
    *s = segundos;
}

/* Devuelve la fecha que se introdujo al inicio y que mantiene el reloj. */
void obtenerFecha(uint8_t *dia, uint8_t *mes, uint16_t *anio)
{
    *dia = diaMes;
    *mes = mesActual;
    *anio = anioActual;
}

/* Comprueba todos los limites antes de aceptar una fecha y hora del usuario. */
bool fechaHoraEsValida(uint8_t dia, uint8_t mes, uint16_t anio,
                       uint8_t hora, uint8_t minuto, uint8_t segundo)
{
    if (anio < 2000U || anio > 2099U || mes < 1U || mes > 12U ||
        hora > 23U || minuto > 59U || segundo > 59U)
    {
        return false;
    }

    return (dia >= 1U && dia <= obtenerDiasDelMes(mes, anio));
}

/* Establece una fecha y hora completas y calcula el dia de la semana asociado. */
bool establecerFechaHora(uint8_t dia, uint8_t mes, uint16_t anio,
                         uint8_t hora, uint8_t minuto, uint8_t segundo)
{
    if (!fechaHoraEsValida(dia, mes, anio, hora, minuto, segundo))
    {
        return false;
    }

    diaMes = dia;
    mesActual = mes;
    anioActual = anio;
    horas = hora;
    minutos = minuto;
    segundos = segundo;
    diaSemanaActual = calcularDiaSemana(dia, mes, anio);
    numeroDiaActual = 0U;
    relojConfigurado = true;
    return true;
}

/* Devuelve la mascara del dia actual: DIA_LUNES, DIA_MARTES, etc. */
uint8_t obtenerDiaSemana(void)
{
    return diaSemanaActual;
}

/* Devuelve los dias completos transcurridos desde la inicializacion del reloj. */
uint32_t obtenerNumeroDia(void)
{
    return numeroDiaActual;
}
