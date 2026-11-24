/*
 * pantalla.h
 *
 *  Created on: 21 oct. 2026
 *      Author: josea
 */

#ifndef PANTALLA_H_
#define PANTALLA_H_

#include <stdint.h>

/* Funciones de dibujo de las distintas pantallas del FT800. */
void cargarImagenPastillero(void);
void dibujarImagenPastillero(int x, int y);
void inicializarPantallaFechaHora(void);
void dibujarPantallaFechaHora(void);
void dibujarPantallaInicio(void);
void dibujarPantallaCuidador(void);
void dibujarPantallaConfiguracion(void);
void dibujarPantallaEdicionMedicamento(void);
void dibujarPantallaDiasHorario(void);
void iniciarEdicionNombreMedicamento(const char *nombreInicial);
const char *obtenerNombreEdicionMedicamento(void);
void dibujarPantallaNombreMedicamento(void);
/* Etiquetas tactiles publicadas por pantalla.c para que main.c procese acciones. */
extern int BotonAceptarFechaHora;
extern int BotonCuidador;
extern int BotonVolver;
extern int BotonConfiguracion;
extern int BotonVolverConfiguracion;
extern int BotonCrearMedicamento;
extern int BotonEditarNombreMedicamento;
extern int BotonAceptarNombreMedicamento;
extern int BotonCancelarNombreMedicamento;
extern int BotonCambiarActividadMedicamento;
extern int BotonEditarMedicamento;
extern int BotonEliminarMedicamento;
extern int BotonVolverEdicionMedicamento;
extern int BotonAgregarHorario;
extern int BotonCambiarActividadHorario;
extern int BotonEditarDiasHorario;
extern int BotonVolverDiasHorario;
extern int BotonAlternarDiaHorario;
extern int BotonEliminarHorario;
extern int BotonGuardarMedicamento;
extern int BotonHoraMenos;
extern int BotonHoraMas;
extern int BotonMinutoMenos;
extern int BotonMinutoMas;
/* Seleccion actual dentro del editor de medicamento y horarios. */
extern uint8_t IndiceMedicamentoEdicion;
extern uint8_t IndiceHorarioEdicion;
extern uint8_t DiaHorarioEdicion;
/* Toma seleccionada desde la lista del cuidador para su confirmacion. */
extern int BotonConfirmarToma;
extern uint8_t IndiceMedicamentoConfirmar;
extern uint8_t IndiceHorarioConfirmar;
#endif /* PANTALLA_H_ */
