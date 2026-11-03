
#ifndef MEDICACION_H_
#define MEDICACION_H_

#include <stdint.h>
#include <stdbool.h>
#define MAX_MEDICAMENTOS 8 //numero maximo de medicamentos que se podran almacenar
#define MAX_HORARIOS_MEDICAMENTOS 3 //numero maximo de horarios por medicamento por cada dia
#define MAX_NOMBRES_MEDICAMENTOS 24 //numero maximo de caracteres para los medicamentos

//definiciones de los bits asociados a cada dia en diasSemana de la estructura HorarioConfig
#define DIA_LUNES (1U << 0)
#define DIA_MARTES (1U << 1)
#define DIA_MIERCOLES (1U << 2)
#define DIA_JUEVES (1U << 3)
#define DIA_VIERNES (1U << 4)
#define DIA_SABADO (1U << 5)
#define DIA_DOMINGO (1U << 6)
#define DIA_TODOS (DIA_LUNES | DIA_MARTES | DIA_MIERCOLES | DIA_JUEVES | DIA_VIERNES | DIA_SABADO | DIA_DOMINGO)


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

typedef struct //estructura que define el estado de cada toma de cada medicacion
{
    EstadoToma estado;
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
