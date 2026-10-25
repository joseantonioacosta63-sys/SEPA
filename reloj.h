#ifndef RELOJ_H_
#define RELOJ_H_

#include <stdint.h>

void inicializarReloj(void);
void actualizarReloj(void);
void obtenerHora(uint8_t *h, uint8_t *m, uint8_t *s);

#endif
