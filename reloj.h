#ifndef RELOJ_H_
#define RELOJ_H_

#include <stdint.h>
#include <stdbool.h>

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

/* Reinicia el reloj hasta que el usuario introduzca una fecha y hora válidas. */
void inicializarReloj(void);
/* Avanza un segundo; se llama desde el programa principal, nunca desde la ISR. */
void actualizarReloj(void);
/* Copia la hora actual en los punteros proporcionados por quien llama. */
void obtenerHora(uint8_t *h, uint8_t *m, uint8_t *s);
/* Copia la fecha actual en los punteros proporcionados por quien llama. */
void obtenerFecha(uint8_t *dia, uint8_t *mes, uint16_t *anio);
/* Comprueba que fecha y hora están dentro de los límites del calendario. */
bool fechaHoraEsValida(uint8_t dia, uint8_t mes, uint16_t anio,
                       uint8_t hora, uint8_t minuto, uint8_t segundo);
/* Aplica una fecha/hora válida y habilita el avance normal del reloj. */
bool establecerFechaHora(uint8_t dia, uint8_t mes, uint16_t anio,
                         uint8_t hora, uint8_t minuto, uint8_t segundo);
/* Devuelve el bit del día semanal actual, compatible con DIA_LUNES, etc. */
uint8_t obtenerDiaSemana(void);
/* Devuelve los días completos transcurridos desde la configuración inicial. */
uint32_t obtenerNumeroDia(void);

#endif
