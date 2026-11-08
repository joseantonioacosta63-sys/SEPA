#include "medicacion.h"
#include "reloj.h"
#include "driverlib2.h"
#include <stddef.h>

#define CONFIGURACION_FIRMA 0x53455041UL
#define CONFIGURACION_VERSION 1UL

/*
 * Formato que se almacenara completo en EEPROM. Se mantiene privado porque
 * solo el modulo de medicacion debe conocer su organizacion interna.
 */
typedef struct
{
    uint32_t firma;
    uint32_t version;
    MedicConfig medicamentos[MAX_MEDICAMENTOS];
} ConfiguracionPersistente;

/*
 * La union garantiza un buffer de palabras de 32 bits para EEPROMRead y
 * EEPROMProgram, aunque el formato se amplie mas adelante.
 */
typedef union
{
    ConfiguracionPersistente datos;
    uint32_t palabras[(sizeof(ConfiguracionPersistente) + sizeof(uint32_t) - 1U) /
                      sizeof(uint32_t)];
} AlmacenamientoConfiguracion;

/* Copia de trabajo en RAM de la configuracion que se cargara desde EEPROM. */
static AlmacenamientoConfiguracion almacenamientoConfiguracion;
#define configuracion (almacenamientoConfiguracion.datos)

/* El estado diario no forma parte de la configuracion persistente. */
static EstadoDiarioHorario estadosDiarios[MAX_MEDICAMENTOS][MAX_HORARIOS_MEDICAMENTOS];

/* Marca la copia RAM como compatible con el formato actual antes de guardarla. */
static void actualizarCabeceraConfiguracion(void)
{
    configuracion.firma = CONFIGURACION_FIRMA;
    configuracion.version = CONFIGURACION_VERSION;
}

/* Comprueba que una hora y una mascara semanal son utilizables por un horario. */
static bool horarioEsValido(uint8_t hora, uint8_t minuto, uint8_t diasSemana)
{
    return (hora < 24U && minuto < 60U &&
            (diasSemana & DIA_TODOS) != 0U &&
            (diasSemana & (uint8_t)(~DIA_TODOS)) == 0U);
}

/* Comprueba que un nombre leido de EEPROM tiene terminador dentro de su array. */
static bool nombreEsValido(const char nombre[MAX_NOMBRES_MEDICAMENTOS])
{
    uint8_t posicion;

    for (posicion = 0; posicion < MAX_NOMBRES_MEDICAMENTOS; posicion++)
    {
        if (nombre[posicion] == '\0')
        {
            return (posicion != 0U);
        }
    }

    return false;
}

/* Copia un nombre corto y terminado en nulo; rechaza textos vacios o demasiado largos. */
static bool copiarNombreMedicamento(char destino[MAX_NOMBRES_MEDICAMENTOS], const char *origen)
{
    uint8_t posicion = 0;

    if (origen == NULL || origen[0] == '\0')
    {
        return false;
    }

    while (origen[posicion] != '\0' && posicion < (MAX_NOMBRES_MEDICAMENTOS - 1U))
    {
        destino[posicion] = origen[posicion];
        posicion++;
    }

    if (origen[posicion] != '\0')
    {
        return false;
    }

    destino[posicion] = '\0';
    return true;
}

/* Distingue un medicamento creado de un hueco vacio, aunque este desactivado. */
static bool medicamentoExiste(uint8_t indiceMedicamento)
{
    return (indiceMedicamento < MAX_MEDICAMENTOS &&
            configuracion.medicamentos[indiceMedicamento].nombre[0] != '\0');
}

/* Reinicia los estados de todos los horarios de un medicamento concreto. */
static void reiniciarEstadosMedicamento(uint8_t indiceMedicamento)
{
    uint8_t horario;

    for (horario = 0; horario < MAX_HORARIOS_MEDICAMENTOS; horario++)
    {
        estadosDiarios[indiceMedicamento][horario].estado = PROGRAMADA;
    }
}

/* Valida los limites de los datos recibidos antes de aceptarlos desde EEPROM. */
static bool configuracionEsValida(void)
{
    uint8_t indiceMedicamento;
    uint8_t indiceHorario;
    const MedicConfig *medicamento;

    if (!hayConfiguracionMedicacion())
    {
        return false;
    }

    for (indiceMedicamento = 0; indiceMedicamento < MAX_MEDICAMENTOS;
         indiceMedicamento++)
    {
        medicamento = &configuracion.medicamentos[indiceMedicamento];

        if (medicamento->nombre[0] == '\0')
        {
            if (medicamento->activo || medicamento->horariosUsados != 0U)
            {
                return false;
            }

            continue;
        }

        if (!nombreEsValido(medicamento->nombre) ||
            medicamento->horariosUsados > MAX_HORARIOS_MEDICAMENTOS)
        {
            return false;
        }

        for (indiceHorario = 0;
             indiceHorario < medicamento->horariosUsados;
             indiceHorario++)
        {
            if (!horarioEsValido(medicamento->horarios[indiceHorario].hora,
                                 medicamento->horarios[indiceHorario].minuto,
                                 medicamento->horarios[indiceHorario].diasSemana))
            {
                return false;
            }
        }
    }

    return true;
}

/* Inicializa el periferico EEPROM y comprueba que cabe nuestro unico registro. */
static bool prepararEEPROM(void)
{
    uint32_t intentos;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0);

    for (intentos = 0; intentos < 100000U; intentos++)
    {
        if (SysCtlPeripheralReady(SYSCTL_PERIPH_EEPROM0))
        {
            break;
        }
    }

    if (intentos == 100000U || EEPROMInit() != EEPROM_INIT_OK)
    {
        return false;
    }

    return (EEPROMSizeGet() >= sizeof(almacenamientoConfiguracion.palabras));
}

/* Indica si la configuracion en RAM tiene el formato esperado por este firmware. */
bool hayConfiguracionMedicacion(void)
{
    return (configuracion.firma == CONFIGURACION_FIRMA &&
            configuracion.version == CONFIGURACION_VERSION);
}

/* Lee la configuracion persistente; ante error deja una configuracion RAM vacia. */
bool cargarConfiguracionMedicacion(void)
{
    if (!prepararEEPROM())
    {
        almacenamientoConfiguracion = (AlmacenamientoConfiguracion){0};
        return false;
    }

    EEPROMRead(almacenamientoConfiguracion.palabras, 0U,
               sizeof(almacenamientoConfiguracion.palabras));

    if (!configuracionEsValida())
    {
        almacenamientoConfiguracion = (AlmacenamientoConfiguracion){0};
        return false;
    }

    return true;
}

/* Guarda la configuracion RAM solo cuando tiene firma, version y limites validos. */
bool guardarConfiguracionMedicacion(void)
{
    uint32_t resultado;

    if (!configuracionEsValida() || !prepararEEPROM())
    {
        return false;
    }

    resultado = EEPROMProgram(almacenamientoConfiguracion.palabras, 0U,
                               sizeof(almacenamientoConfiguracion.palabras));
    return (resultado == 0U);
}

/* Devuelve la configuracion de un medicamento o NULL si su indice no existe. */
const MedicConfig *obtenerMedicamentoConfigurado(uint8_t indiceMedicamento)
{
    if (!hayConfiguracionMedicacion() ||
        indiceMedicamento >= MAX_MEDICAMENTOS ||
        !configuracion.medicamentos[indiceMedicamento].activo)
    {
        return NULL;
    }

    return &configuracion.medicamentos[indiceMedicamento];
}

/* Devuelve un horario configurado, evitando leer huecos que aun no se usan. */
const HorarioConfig *obtenerHorarioConfigurado(uint8_t indiceMedicamento, uint8_t indiceHorario)
{
    const MedicConfig *medicamento;

    medicamento = obtenerMedicamentoConfigurado(indiceMedicamento);

    if (medicamento == NULL ||
        indiceHorario >= MAX_HORARIOS_MEDICAMENTOS ||
        indiceHorario >= medicamento->horariosUsados)
    {
        return NULL;
    }

    return &medicamento->horarios[indiceHorario];
}

/* Devuelve el estado diario asociado a un horario valido y configurado. */
const EstadoDiarioHorario *obtenerEstadoDiarioHorario(uint8_t indiceMedicamento, uint8_t indiceHorario)
{
    if (obtenerHorarioConfigurado(indiceMedicamento, indiceHorario) == NULL)
    {
        return NULL;
    }

    return &estadosDiarios[indiceMedicamento][indiceHorario];
}

/* Crea un medicamento en un hueco libre y deja sus horarios sin configurar. */
bool crearMedicamento(uint8_t indiceMedicamento, const char *nombre)
{
    MedicConfig nuevoMedicamento = (MedicConfig){0};

    if (indiceMedicamento >= MAX_MEDICAMENTOS ||
        medicamentoExiste(indiceMedicamento) ||
        !copiarNombreMedicamento(nuevoMedicamento.nombre, nombre))
    {
        return false;
    }

    nuevoMedicamento.activo = true;
    configuracion.medicamentos[indiceMedicamento] = nuevoMedicamento;
    reiniciarEstadosMedicamento(indiceMedicamento);
    actualizarCabeceraConfiguracion();
    return true;
}

/* Cambia solo el nombre y conserva los horarios del medicamento existente. */
bool editarNombreMedicamento(uint8_t indiceMedicamento, const char *nombre)
{
    if (indiceMedicamento >= MAX_MEDICAMENTOS ||
        !medicamentoExiste(indiceMedicamento) ||
        !copiarNombreMedicamento(configuracion.medicamentos[indiceMedicamento].nombre, nombre))
    {
        return false;
    }

    actualizarCabeceraConfiguracion();
    return true;
}

/* Activa o desactiva un medicamento sin borrar su configuracion ni horarios. */
bool establecerMedicamentoActivo(uint8_t indiceMedicamento, bool activo)
{
    if (indiceMedicamento >= MAX_MEDICAMENTOS ||
        !medicamentoExiste(indiceMedicamento))
    {
        return false;
    }

    configuracion.medicamentos[indiceMedicamento].activo = activo;
    actualizarCabeceraConfiguracion();
    return true;
}

/* Elimina la configuracion de un medicamento y sus estados diarios asociados. */
bool eliminarMedicamento(uint8_t indiceMedicamento)
{
    if (indiceMedicamento >= MAX_MEDICAMENTOS ||
        !medicamentoExiste(indiceMedicamento))
    {
        return false;
    }

    configuracion.medicamentos[indiceMedicamento] = (MedicConfig){0};
    reiniciarEstadosMedicamento(indiceMedicamento);
    actualizarCabeceraConfiguracion();
    return true;
}

/* Anade un horario al siguiente hueco libre de un medicamento existente. */
bool agregarHorarioMedicamento(uint8_t indiceMedicamento, uint8_t hora, uint8_t minuto, uint8_t diasSemana)
{
    MedicConfig *medicamento;
    uint8_t indiceHorario;

    if (indiceMedicamento >= MAX_MEDICAMENTOS ||
        !medicamentoExiste(indiceMedicamento) ||
        !horarioEsValido(hora, minuto, diasSemana))
    {
        return false;
    }

    medicamento = &configuracion.medicamentos[indiceMedicamento];

    if (medicamento->horariosUsados >= MAX_HORARIOS_MEDICAMENTOS)
    {
        return false;
    }

    indiceHorario = medicamento->horariosUsados;
    medicamento->horarios[indiceHorario] = (HorarioConfig){hora, minuto, diasSemana};
    medicamento->horariosUsados++;
    estadosDiarios[indiceMedicamento][indiceHorario].estado = PROGRAMADA;
    actualizarCabeceraConfiguracion();
    return true;
}

/* Edita un horario existente y reinicia su estado porque ha cambiado la toma. */
bool editarHorarioMedicamento(uint8_t indiceMedicamento, uint8_t indiceHorario, uint8_t hora, uint8_t minuto, uint8_t diasSemana)
{
    MedicConfig *medicamento;

    if (indiceMedicamento >= MAX_MEDICAMENTOS ||
        !medicamentoExiste(indiceMedicamento) ||
        !horarioEsValido(hora, minuto, diasSemana))
    {
        return false;
    }

    medicamento = &configuracion.medicamentos[indiceMedicamento];

    if (indiceHorario >= medicamento->horariosUsados)
    {
        return false;
    }

    medicamento->horarios[indiceHorario] = (HorarioConfig){hora, minuto, diasSemana};
    estadosDiarios[indiceMedicamento][indiceHorario].estado = PROGRAMADA;
    actualizarCabeceraConfiguracion();
    return true;
}

/* Elimina un horario y desplaza los posteriores para mantenerlos consecutivos. */
bool eliminarHorarioMedicamento(uint8_t indiceMedicamento, uint8_t indiceHorario)
{
    MedicConfig *medicamento;
    uint8_t horario;

    if (indiceMedicamento >= MAX_MEDICAMENTOS ||
        !medicamentoExiste(indiceMedicamento))
    {
        return false;
    }

    medicamento = &configuracion.medicamentos[indiceMedicamento];

    if (indiceHorario >= medicamento->horariosUsados)
    {
        return false;
    }

    for (horario = indiceHorario; horario + 1U < medicamento->horariosUsados; horario++)
    {
        medicamento->horarios[horario] = medicamento->horarios[horario + 1U];
        estadosDiarios[indiceMedicamento][horario] = estadosDiarios[indiceMedicamento][horario + 1U];
    }

    medicamento->horariosUsados--;
    medicamento->horarios[medicamento->horariosUsados] = (HorarioConfig){0};
    estadosDiarios[indiceMedicamento][medicamento->horariosUsados].estado = PROGRAMADA;
    actualizarCabeceraConfiguracion();
    return true;
}

static void reiniciarEstadosDiarios(void)
{
    uint8_t medicamento;
    uint8_t horario;

    for (medicamento = 0; medicamento < MAX_MEDICAMENTOS; medicamento++)
    {
        for (horario = 0; horario < MAX_HORARIOS_MEDICAMENTOS; horario++)
        {
            estadosDiarios[medicamento][horario].estado = PROGRAMADA;
        }
    }
}

void inicializarMedicacion(void)
{
    reiniciarEstadosDiarios();

    if (!cargarConfiguracionMedicacion())
    {
        // Crear los tres medicamentos por defecto
        crearMedicamento(0, "Medicacion A");
        crearMedicamento(1, "Medicacion B");
        crearMedicamento(2, "Medicacion C");

        // Asignar un horario diario a cada medicamento
        agregarHorarioMedicamento(0, 9, 0, DIA_TODOS);
        agregarHorarioMedicamento(1, 14, 0, DIA_TODOS);
        agregarHorarioMedicamento(2, 21, 0, DIA_TODOS);

        // Intentar guardar la configuracion en EEPROM
        guardarConfiguracionMedicacion();
    }
}

void gestionarMedicacion(void)
{
    uint8_t horaActual;
    uint8_t minutoActual;
    uint8_t segundoActual;
    uint8_t indiceMedicamento;
    uint8_t indiceHorario;
    uint32_t segundosActuales;
    uint32_t segundosProgramados;
    const MedicConfig *medicamento;
    const HorarioConfig *horario;

    if (!hayConfiguracionMedicacion())
    {
        return;
    }

    obtenerHora(&horaActual, &minutoActual, &segundoActual);
    segundosActuales = (uint32_t)horaActual * 3600U +
                        (uint32_t)minutoActual * 60U + segundoActual;

    for (indiceMedicamento = 0; indiceMedicamento < MAX_MEDICAMENTOS;
         indiceMedicamento++)
    {
        medicamento = obtenerMedicamentoConfigurado(indiceMedicamento);

        if (medicamento == NULL)
        {
            continue;
        }

        for (indiceHorario = 0; indiceHorario < medicamento->horariosUsados;
             indiceHorario++)
        {
            horario = &medicamento->horarios[indiceHorario];

            /* Hasta disponer de fecha, solo los horarios diarios son ejecutables. */
            if (horario->diasSemana != DIA_TODOS ||
                estadosDiarios[indiceMedicamento][indiceHorario].estado == CONFIRMADA)
            {
                continue;
            }

            segundosProgramados = (uint32_t)horario->hora * 3600U +
                                  (uint32_t)horario->minuto * 60U;

            if (segundosActuales >= segundosProgramados + 1800U)
            {
                estadosDiarios[indiceMedicamento][indiceHorario].estado = REVISAR;
            }
            else if (segundosActuales >= segundosProgramados)
            {
                estadosDiarios[indiceMedicamento][indiceHorario].estado = PENDIENTE;
            }
            else
            {
                estadosDiarios[indiceMedicamento][indiceHorario].estado = PROGRAMADA;
            }
        }
    }
}

/* Confirma solo durante la ventana [10 min antes, 30 min despues) de la toma. */
bool confirmarToma(uint8_t indiceMedicamento, uint8_t indiceHorario)
{
    uint8_t horaActual;
    uint8_t minutoActual;
    uint8_t segundoActual;
    uint32_t segundosActuales;
    uint32_t segundosProgramados;
    const HorarioConfig *horario;
    EstadoDiarioHorario *estado;

    horario = obtenerHorarioConfigurado(indiceMedicamento, indiceHorario);

    if (horario == NULL || horario->diasSemana != DIA_TODOS)
    {
        return false;
    }

    estado = &estadosDiarios[indiceMedicamento][indiceHorario];

    if (estado->estado == CONFIRMADA || estado->estado == REVISAR)
    {
        return false;
    }

    obtenerHora(&horaActual, &minutoActual, &segundoActual);
    segundosActuales = (uint32_t)horaActual * 3600U +
                        (uint32_t)minutoActual * 60U + segundoActual;
    segundosProgramados = (uint32_t)horario->hora * 3600U +
                          (uint32_t)horario->minuto * 60U;

    /* Una toma a las 00:00 no puede adelantar su ventana al dia anterior aun. */
    if ((segundosProgramados >= 600U &&
         segundosActuales < segundosProgramados - 600U) ||
        segundosActuales >= segundosProgramados + 1800U)
    {
        return false;
    }

    estado->estado = CONFIRMADA;
    return true;
}

/* Busca la toma diaria futura mas cercana y devuelve sus dos indices. */
bool obtenerProximaToma(uint8_t *indiceMedicamentoResultado,
                        uint8_t *indiceHorarioResultado)
{
    uint8_t i, j;
    uint32_t minutosToma;
    uint32_t menorMinuto = 1440U;
    bool encontrada = false;

    const MedicConfig *medicamento;
    const HorarioConfig *horario;

    if (indiceMedicamentoResultado == NULL ||
        indiceHorarioResultado == NULL ||
        !hayConfiguracionMedicacion())
    {
        return false;
    }

    for (i = 0; i < MAX_MEDICAMENTOS; i++)
    {
        medicamento = obtenerMedicamentoConfigurado(i);

        if (medicamento == NULL)
        {
            continue;
        }

        for (j = 0; j < medicamento->horariosUsados; j++)
        {
            horario = &medicamento->horarios[j];

            if (horario->diasSemana != DIA_TODOS)
            {
                continue;
            }

            if (estadosDiarios[i][j].estado == CONFIRMADA)
            {
                continue;
            }

            minutosToma = horario->hora * 60U + horario->minuto;

            if (minutosToma < menorMinuto)
            {
                menorMinuto = minutosToma;
                *indiceMedicamentoResultado = i;
                *indiceHorarioResultado = j;
                encontrada = true;
            }
        }
    }

    return encontrada;
}







