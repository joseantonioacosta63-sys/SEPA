/*
 * pantalla.h
 *
 *  Created on: 21 oct. 2026
 *      Author: josea
 */

#ifndef PANTALLA_H_
#define PANTALLA_H_

#include <stdint.h>

void cargarImagenPastillero(void);
void dibujarImagenPastillero(int x, int y);
void inicializarPantallaFechaHora(void);
void dibujarPantallaFechaHora(void);
void dibujarPantallaInicio(void);
void dibujarPantallaCuidador(void);
void dibujarPantallaConfiguracion(void);
void dibujarPantallaEdicionMedicamento(void);
extern int BotonAceptarFechaHora;
extern int BotonCuidador;
extern int BotonVolver;
extern int BotonConfiguracion;
extern int BotonVolverConfiguracion;
extern int BotonCrearMedicamento;
extern int BotonCambiarActividadMedicamento;
extern int BotonEditarMedicamento;
extern int BotonVolverEdicionMedicamento;
extern int BotonAgregarHorario;
extern int BotonCambiarActividadHorario;
extern int BotonHoraMenos;
extern int BotonHoraMas;
extern int BotonMinutoMenos;
extern int BotonMinutoMas;
extern uint8_t IndiceMedicamentoEdicion;
extern uint8_t IndiceHorarioEdicion;
extern int BotonConfirmarToma;
extern uint8_t IndiceMedicamentoConfirmar;
extern uint8_t IndiceHorarioConfirmar;
#endif /* PANTALLA_H_ */
