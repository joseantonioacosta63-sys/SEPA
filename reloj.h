#ifndef RELOJ_H_
#define RELOJ_H_

#include <stdint.h>

/* Cada dia ocupa un bit para poder compararlo con diasSemana de un horario. */
#define DIA_LUNES     (1U << 0)
#define DIA_MARTES    (1U << 1)
#define DIA_MIERCOLES (1U << 2)
#define DIA_JUEVES    (1U << 3)
#define DIA_VIERNES   (1U << 4)
#define DIA_SABADO    (1U << 5)
#define DIA_DOMINGO   (1U << 6)
#define DIA_TODOS     (DIA_LUNES | DIA_MARTES | DIA_MIERCOLES | DIA_JUEVES | \
                       DIA_VIERNES | DIA_SABADO | DIA_DOMINGO)

void inicializarReloj(void);
void actualizarReloj(void);
void obtenerHora(uint8_t *h, uint8_t *m, uint8_t *s);
uint8_t obtenerDiaSemana(void);
uint32_t obtenerNumeroDia(void);

#endif
