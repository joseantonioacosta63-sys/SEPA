#include "reloj.h"

static uint8_t horas;
static uint8_t minutos;
static uint8_t segundos;

void inicializarReloj(void)
{
    horas = 8;
    minutos = 59;
    segundos = 50;
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
