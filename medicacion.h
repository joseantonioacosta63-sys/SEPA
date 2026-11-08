

#ifndef MEDICACION_H_
#define MEDICACION_H_

#include <stdint.h>
#include <stdbool.h>
#include "reloj.h"
#define MAX_MEDICAMENTOS 8 //numero maximo de medicamentos que se podran almacenar
#define MAX_HORARIOS_MEDICAMENTOS 3 //numero maximo de horarios por medicamento por cada dia
#define MAX_NOMBRES_MEDICAMENTOS 24 //numero maximo de caracteres para los medicamentos

typedef struct //estructura que define los horarios distintos programables con su hora, minuto y dias de la semana
{
    uint8_t hora;
    uint8_t minuto;
    uint8_t diasSemana;
} HorarioConfig;

typedef struct //estructura que engloba  HorarioConfig para varios horarios de una misma medicacion, su actividad y su nombre
{
    char nombre[MAX_NOMBRES_MEDICAMENTOS]; //23 caracteres mas el nulo
    bool activo;
    HorarioConfig horarios[MAX_HORARIOS_MEDICAMENTOS];
    uint8_t horariosUsados;
} MedicConfig;

typedef enum
{
    PROGRAMADA,
    PENDIENTE,
    CONFIRMADA,
    REVISAR
} EstadoToma;

/* Estado de ejecucion de un horario y su unica incidencia pendiente de revisar. */
typedef struct
{
    EstadoToma estado;
    uint32_t numeroDiaProgramado;
    bool revisionPendiente;
} EstadoDiarioHorario;

// Funciones publicas
void inicializarMedicacion(void);
void gestionarMedicacion(void);

/* Guarda o recupera la configuracion permanente de los medicamentos. */
bool cargarConfiguracionMedicacion(void);
bool guardarConfiguracionMedicacion(void);

bool hayConfiguracionMedicacion(void);
const MedicConfig *obtenerMedicamentoConfigurado(uint8_t indiceMedicamento);
const HorarioConfig *obtenerHorarioConfigurado(uint8_t indiceMedicamento, uint8_t indiceHorario);
const EstadoDiarioHorario *obtenerEstadoDiarioHorario(uint8_t indiceMedicamento, uint8_t indiceHorario);
bool tieneRevisionPendiente(uint8_t indiceMedicamento, uint8_t indiceHorario);

/* Opera sobre una toma concreta: medicamento y uno de sus horarios. */
bool confirmarToma(uint8_t indiceMedicamento, uint8_t indiceHorario);
bool obtenerProximaToma(uint8_t *indiceMedicamento, uint8_t *indiceHorario);

bool crearMedicamento(uint8_t indiceMedicamento, const char *nombre);
bool editarNombreMedicamento(uint8_t indiceMedicamento, const char *nombre);
bool establecerMedicamentoActivo(uint8_t indiceMedicamento, bool activo);
bool eliminarMedicamento(uint8_t indiceMedicamento);
bool agregarHorarioMedicamento(uint8_t indiceMedicamento, uint8_t hora, uint8_t minuto, uint8_t diasSemana);
bool editarHorarioMedicamento(uint8_t indiceMedicamento, uint8_t indiceHorario, uint8_t hora, uint8_t minuto, uint8_t diasSemana);
bool eliminarHorarioMedicamento(uint8_t indiceMedicamento, uint8_t indiceHorario);

#endif
