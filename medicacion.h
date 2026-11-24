

#ifndef MEDICACION_H_
#define MEDICACION_H_

#include <stdint.h>
#include <stdbool.h>
#include "reloj.h"

#define MAX_MEDICAMENTOS 8 /* Numero maximo de medicamentos en memoria. */
#define MAX_HORARIOS_MEDICAMENTOS 3 /* Numero maximo de horarios de cada medicamento por dia. */
#define MAX_NOMBRES_MEDICAMENTOS 24 /* Numero maximo de caracteres para nombres. */

/* El bit 7 no representa un dia: conserva un horario, pero lo desactiva. */
#define HORARIO_DESACTIVADO (1U << 7)


/* Hora y mascara semanal de una administracion configurada. */
typedef struct
{
    uint8_t hora;
    uint8_t minuto;
    uint8_t diasSemana;
} HorarioConfig;

/* Configuracion persistente de un medicamento, guardable en EEPROM. */
typedef struct
{
    char nombre[MAX_NOMBRES_MEDICAMENTOS]; //23 caracteres mas el nulo
    bool activo;
    HorarioConfig horarios[MAX_HORARIOS_MEDICAMENTOS];
    uint8_t horariosUsados;
} MedicConfig;

/* Estado de un horario durante el dia que se esta ejecutando. */
typedef enum
{
    PROGRAMADA,
    PENDIENTE,
    CONFIRMADA,
    EXPIRADA,
} EstadoToma;

/* Estado transitorio en RAM; no forma parte de la configuracion permanente. */
typedef struct
{
    EstadoToma estado;
    bool expiracionPendienteRegistro;
} EstadoDiarioHorario;

/* Carga la configuracion y prepara los estados diarios en RAM. */
void inicializarMedicacion(void);
/* Actualiza estados por hora, dia semanal y vencimiento de cada toma. */
void gestionarMedicacion(void);

/* Guarda o recupera la configuracion permanente de los medicamentos. */
bool cargarConfiguracionMedicacion(void);
bool guardarConfiguracionMedicacion(void);

/* Verifica firma y version antes de confiar en los datos de EEPROM. */
bool hayConfiguracionMedicacion(void);
const MedicConfig *obtenerMedicamentoConfigurado(uint8_t indiceMedicamento);
/* Permite al editor consultar tambien medicamentos desactivados. */
const MedicConfig *obtenerMedicamentoParaEdicion(uint8_t indiceMedicamento);
const HorarioConfig *obtenerHorarioConfigurado(uint8_t indiceMedicamento, uint8_t indiceHorario);
/* El editor puede consultar horarios aunque el medicamento este desactivado. */
const HorarioConfig *obtenerHorarioParaEdicion(uint8_t indiceMedicamento,
                                                uint8_t indiceHorario);
const EstadoDiarioHorario *obtenerEstadoDiarioHorario(uint8_t indiceMedicamento, uint8_t indiceHorario);

/* Opera sobre una toma concreta: medicamento y uno de sus horarios. */
bool puedeConfirmarToma(uint8_t indiceMedicamento, uint8_t indiceHorario);
bool confirmarToma(uint8_t indiceMedicamento, uint8_t indiceHorario);
bool obtenerTomaAnterior(uint8_t *indiceMedicamento, uint8_t *indiceHorario);
bool obtenerTomaFuturaPorOrden(uint8_t orden, uint8_t *indiceMedicamento,
                                uint8_t *indiceHorario);
bool obtenerProximaToma(uint8_t *indiceMedicamento, uint8_t *indiceHorario);
bool obtenerExpiracionPendiente(uint8_t *indiceMedicamento, uint8_t *indiceHorario);
uint8_t contarTomasDelDia(void);
bool obtenerTomaDelDiaPorOrden(uint8_t orden, uint8_t *indiceMedicamento,
                                uint8_t *indiceHorario);

/* Operaciones de edicion en RAM; guardarConfiguracionMedicacion confirma cambios. */
bool crearMedicamento(uint8_t indiceMedicamento, const char *nombre);
bool editarNombreMedicamento(uint8_t indiceMedicamento, const char *nombre);
bool restaurarMedicamento(uint8_t indiceMedicamento, const MedicConfig *respaldo);
bool establecerMedicamentoActivo(uint8_t indiceMedicamento, bool activo);
bool eliminarMedicamento(uint8_t indiceMedicamento);
bool agregarHorarioMedicamento(uint8_t indiceMedicamento, uint8_t hora, uint8_t minuto, uint8_t diasSemana);
bool editarHorarioMedicamento(uint8_t indiceMedicamento, uint8_t indiceHorario, uint8_t hora, uint8_t minuto, uint8_t diasSemana);
bool establecerHorarioActivo(uint8_t indiceMedicamento, uint8_t indiceHorario,
                             bool activo);
bool alternarDiaHorario(uint8_t indiceMedicamento, uint8_t indiceHorario,
                        uint8_t dia);
bool eliminarHorarioMedicamento(uint8_t indiceMedicamento, uint8_t indiceHorario);

#endif
