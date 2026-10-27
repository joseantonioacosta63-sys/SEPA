
#ifndef MEDICACION_H_
#define MEDICACION_H_

#include <stdint.h>

#define NUM_TOMAS 3

typedef enum
{
    PROGRAMADA,
    PENDIENTE,
    CONFIRMADA,
    REVISAR
} EstadoToma;

typedef struct
{
    const char *nombre;
    uint8_t hora;
    uint8_t minuto;
    EstadoToma estado;

} Toma;

// Funciones publicas
void inicializarMedicacion(void);
void gestionarMedicacion(void);
void confirmarToma(uint8_t indice);
const Toma *obtenerToma(uint8_t indice);
int obtenerProximaMedicacion(void);

#endif
