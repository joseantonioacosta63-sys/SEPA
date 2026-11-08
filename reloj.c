#include "reloj.h"

static uint8_t horas;
static uint8_t minutos;
static uint8_t segundos;
static uint8_t diaSemanaActual;
static uint32_t numeroDiaActual;

void inicializarReloj(void)
{
    horas = 8;
    minutos = 59;
    segundos = 50;
    diaSemanaActual = DIA_LUNES;
    numeroDiaActual = 0U;
}

void actualizarReloj(void)
{
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
                numeroDiaActual++;

                /* Tras domingo se vuelve a lunes; los demas dias avanzan un bit. */
                if (diaSemanaActual == DIA_DOMINGO)
                {
                    diaSemanaActual = DIA_LUNES;
                }
                else
                {
                    diaSemanaActual <<= 1;
                }
            }
        }
    }
}

void obtenerHora(uint8_t *h, uint8_t *m, uint8_t *s)
{
    *h = horas;
    *m = minutos;
    *s = segundos;
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
