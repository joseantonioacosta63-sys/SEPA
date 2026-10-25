#include "medicacion.h"
#include "reloj.h"
#include "driverlib2.h"
#include <stddef.h>

static Toma tomas[NUM_TOMAS];

void inicializarMedicacion(void)
{
    tomas[0] = (Toma){"Medicacion A", 9, 0, PROGRAMADA};
    tomas[1] = (Toma){"Medicacion B", 14, 0, PROGRAMADA};
    tomas[2] = (Toma){"Medicacion C", 21, 0, PROGRAMADA};
}


void gestionarMedicacion(void)
{
    uint8_t h, m, s;
    uint8_t i;

    obtenerHora(&h, &m, &s);

    uint16_t minutosActuales = h * 60 + m;

    for (i = 0; i < NUM_TOMAS; i++)
    {
        uint16_t minutosToma =
            tomas[i].hora * 60 + tomas[i].minuto;

        if (tomas[i].estado == PROGRAMADA &&
                minutosActuales >= minutosToma)
        {
            tomas[i].estado = PENDIENTE;
        }
    }
}

void confirmarToma(uint8_t indice)
{
    if (indice < NUM_TOMAS)
    {
        if (tomas[indice].estado == PENDIENTE ||
            tomas[indice].estado == REVISAR)
        {
            tomas[indice].estado = CONFIRMADA;
        }
    }
}


const Toma *obtenerToma(uint8_t indice)
{
    if (indice < NUM_TOMAS)
    {
        return &tomas[indice];
    }
    else
    {
        return NULL;
    }
}









