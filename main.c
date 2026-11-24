
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "driverlib2.h"
#include "FT800_TIVA.h"
#include "utils/uartstdio.h"
#include "HAL_I2C.h"
#include "sensorlib2.h"

#include "reloj.h"
#include "medicacion.h"
#include "pantalla.h"
#include "registro.h"

/* Timer0A genera 50 interrupciones por segundo: una cada 20 ms. */
#define FREC 50

/* Frecuencia real del sistema, usada al configurar perifericos y temporizadores. */
int RELOJ;
/* Estados de navegacion; la pantalla de fecha bloquea el resto hasta aceptar datos. */
typedef enum
{
    PANTALLA_FECHA_HORA = 0,
    PANTALLA_INICIO,
    PANTALLA_CUIDADOR,
    PANTALLA_CONFIGURACION,
    PANTALLA_EDICION_MEDICAMENTO,
    PANTALLA_DIAS_HORARIO,
    PANTALLA_NOMBRE_MEDICAMENTO
} PantallaActual;

static PantallaActual pantallaActual = PANTALLA_FECHA_HORA;
float T_act = 0.0f;
bool SensorDisponible = false;

/* La ISR solo acumula ticks; el bucle principal realiza el trabajo pesado. */
static volatile uint32_t ticksPendientes = 0;

/* Interrupcion de Timer0A: reconoce el evento y deja un tick pendiente. */
void IntTimer(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    ticksPendientes++;
}

/* Imprime por UART el historial que mantiene el modulo de registro. */
static void mostrarHistorialUART(void);
static void registrarExpiracionesPendientes(void);

/* Sondeo I2C con tiempo maximo para comprobar si existe un BME280 en una direccion. */
static bool detectarDireccionBME280(uint32_t baseI2C, uint8_t direccion)
{
    uint32_t intentos;

    I2CMasterSlaveAddrSet(baseI2C, direccion, true);
    I2CMasterControl(baseI2C, I2C_MASTER_CMD_SINGLE_RECEIVE);
    SysCtlDelay(100U);

    for (intentos = 0U; intentos < 100000U; intentos++)
    {
        if (!I2CMasterBusy(baseI2C))
        {
            break;
        }
    }

    return (intentos < 100000U &&
            I2CMasterErr(baseI2C) == I2C_MASTER_ERR_NONE);
}

/* Confirma una toma y añade al historial RAM unicamente las confirmaciones aceptadas. */
void confirmarYRegistrar(uint8_t indiceMedicamento, uint8_t indiceHorario)
{
    RegistroToma nuevaToma = {0};

    const HorarioConfig *horario;
    uint8_t hora, minuto, segundo;

    /* Consultar el horario antes de confirmar permite registrar su hora prevista. */
    horario = obtenerHorarioConfigurado(indiceMedicamento, indiceHorario);
    if (horario == NULL)
    {
        return;
    }

    /* confirmarToma valida la ventana temporal y evita registros falsos. */
    if (confirmarToma(indiceMedicamento, indiceHorario))
    {
        /* Se conserva una letra para el formato actual del historial UART. */
        nuevaToma.medicamento = (char)('A' + indiceMedicamento);

        // Hora prevista
        nuevaToma.horaProgramada = horario->hora;
        nuevaToma.minutoProgramada = horario->minuto;

        // Hora actual del reloj
        obtenerHora(&hora, &minuto, &segundo);

        nuevaToma.horaConfirmada = hora;
        nuevaToma.minutoConfirmada = minuto;

        nuevaToma.diaToma = obtenerNumeroDia();
        nuevaToma.tipo = REGISTRO_CONFIRMACION;

        // Guardar el registro
        agregarRegistro(nuevaToma);
        mostrarHistorialUART();
    }
}

/* Consume las expiraciones marcadas por medicacion y las vuelca una vez al historial. */
static void registrarExpiracionesPendientes(void)
{
    uint8_t indiceMedicamento;
    uint8_t indiceHorario;
    uint8_t hora;
    uint8_t minuto;
    uint8_t segundo;
    const HorarioConfig *horario;
    RegistroToma nuevaToma;
    bool hayExpiracion = false;

    while (obtenerExpiracionPendiente(&indiceMedicamento, &indiceHorario))
    {
        horario = obtenerHorarioConfigurado(indiceMedicamento, indiceHorario);
        if (horario == NULL)
        {
            continue;
        }

        nuevaToma = (RegistroToma){0};
        nuevaToma.medicamento = (char)('A' + indiceMedicamento);
        nuevaToma.horaProgramada = horario->hora;
        nuevaToma.minutoProgramada = horario->minuto;
        obtenerHora(&hora, &minuto, &segundo);
        nuevaToma.horaConfirmada = hora;
        nuevaToma.minutoConfirmada = minuto;
        nuevaToma.diaToma = obtenerNumeroDia();
        nuevaToma.tipo = REGISTRO_EXPIRACION;
        agregarRegistro(nuevaToma);
        hayExpiracion = true;
    }

    if (hayExpiracion)
    {
        mostrarHistorialUART();
    }
}


/* Muestra el historial temporal para depuracion y supervision desde el ordenador. */
static void mostrarHistorialUART(void)
{
    uint8_t i;
    const RegistroToma *registro;

    UARTprintf("\r\n===== HISTORIAL DE TOMAS =====\r\n");

    for (i = 0; i < obtenerNumeroRegistros(); i++)
    {
        registro = obtenerRegistro(i);

        if (registro != NULL)
        {
            UARTprintf("\r\nMedicacion %c\r\n",
                       registro->medicamento);

            UARTprintf("Programada: %02u:%02u\r\n",
                       registro->horaProgramada,
                       registro->minutoProgramada);

            UARTprintf("Registrada: %02u:%02u\r\n",
                       registro->horaConfirmada,
                       registro->minutoConfirmada);

            if (registro->tipo == REGISTRO_EXPIRACION)
            {
                UARTprintf("Tipo: EXPIRACION\r\n");
            }
            else
            {
                UARTprintf("Tipo: CONFIRMACION\r\n");
            }

            UARTprintf("Dia: %u\r\n",
                       (unsigned int)registro->diaToma);
        }
    }

    UARTprintf("\r\nTotal de registros: %u\r\n",
               obtenerNumeroRegistros());

    UARTprintf("=============================\r\n");
}


int main(void)
{
    uint32_t pendientes;
    uint32_t i;

    uint32_t contadorReloj = 0;
    uint32_t contadorPantalla = 0;

    uint8_t Sensor_OK = 0U;
    uint8_t DevID8 = 0U;
    uint8_t intentoBME;

    s32 g_s32ActualTemp = 0;
    u32 g_u32ActualPress = 0;
    u32 g_u32ActualHumity = 0;
    const MedicConfig *medicamentoEdicion;
    const HorarioConfig *horarioEdicion;
    uint8_t indiceNuevoHorario;
    uint8_t horaNueva;
    uint8_t minutoNuevo;
    bool horarioActivo;
    bool i2cSensorDisponible = false;
    uint32_t i2cSensorBase = 0U;
    bool creandoMedicamento = false;
    MedicConfig respaldoMedicamentoEdicion = {0};
    bool hayRespaldoMedicamento = false;

    const int32_t REG_CAL[6] = {CAL_DEFAULTS};

    /* Inicializacion basica de: reloj del micro, FT800, UART, logica y pantalla inicial. */
    RELOJ = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN |
        SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

    HAL_Init_SPI(1, RELOJ);
    Inicia_pantalla();

    for (i = 0; i < 6; i++)
    {
        Esc_Reg(REG_TOUCH_TRANSFORM_A + 4*i, REG_CAL[i]);
    }

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioConfig(0, 115200, RELOJ);

    inicializarReloj();
    inicializarPantallaFechaHora();
    inicializarMedicacion();
    gestionarMedicacion();
    inicializarRegistro();

    mostrarHistorialUART();

    cargarImagenPastillero();

    /* Timer0A mantiene una base de tiempo de 20 ms; no ejecuta la aplicacion en la ISR. */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (RELOJ/FREC)-1);

    TimerIntRegister(TIMER0_BASE, TIMER_A, IntTimer);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    IntMasterEnable();
    TimerEnable(TIMER0_BASE, TIMER_A);

    dibujarPantallaFechaHora();


        if (Detecta_BP(1))
         {
             UARTprintf("\n--------------------------------------");
             UARTprintf("\n  BOOSTERPACK detectado en posicion 1");
             UARTprintf("\n   Configurando puerto I2C0");
             UARTprintf("\n--------------------------------------");
             Conf_Boosterpack(1, RELOJ);
             i2cSensorDisponible = true;
             i2cSensorBase = I2C0_BASE;
         }
         else if (Detecta_BP(2))
         {
             UARTprintf("\n--------------------------------------");
             UARTprintf("\n  BOOSTERPACK detectado en posicion 2");
             UARTprintf("\n   Configurando puerto I2C2");
             UARTprintf("\n--------------------------------------");
             Conf_Boosterpack(2, RELOJ);
             i2cSensorDisponible = true;
             i2cSensorBase = I2C2_BASE;
         }
         else
         {
             UARTprintf("\n--------------------------------------");
             UARTprintf("\n  Ningun BOOSTERPACK detectado   :-/  ");
             UARTprintf("\n  El asistente seguira sin sensor");
             UARTprintf("\n--------------------------------------");
         }

        if (i2cSensorDisponible)
        {
            UARTprintf("Inicializando BME280... ");
            for (intentoBME = 0U; intentoBME < 3U && !SensorDisponible; intentoBME++)
            {
                Sensor_OK = detectarDireccionBME280(i2cSensorBase,
                                                    BME280_I2C_ADDRESS2);
                if (Sensor_OK)
                {
                    readI2C(BME280_I2C_ADDRESS2, BME280_CHIP_ID_REG,
                            &DevID8, 1U);
                    if (DevID8 == 0x60U)
                    {
                        bme280_data_readout_template();
                        bme280_set_power_mode(BME280_NORMAL_MODE);
                        SensorDisponible = true;
                    }
                }

                if (!SensorDisponible)
                {
                    SysCtlDelay(RELOJ / 150U);
                }
            }

            if (SensorDisponible)
            {
                UARTprintf("Hecho! \nLeyendo DevID... ");
                UARTprintf("DevID= 0X%x \n", DevID8);
            }
            else
            {
                UARTprintf("Error en BME280\n");
            }
        }

    while (1)
    {
        bool refrescarPantalla = false;

        IntMasterDisable();
        pendientes = ticksPendientes;
        ticksPendientes = 0;
        IntMasterEnable();

        for (i = 0; i < pendientes; i++)
        {
            contadorReloj++;
            contadorPantalla++;

            if (contadorReloj >= FREC)
            {
                contadorReloj = 0;

                actualizarReloj();
                gestionarMedicacion();
                registrarExpiracionesPendientes();

                if (SensorDisponible)
                {
                    g_s32ActualTemp = 0;

                    if (bme280_read_pressure_temperature_humidity(
                            &g_u32ActualPress,
                            &g_s32ActualTemp,
                            &g_u32ActualHumity) == 0)
                    {
                        T_act = (float)g_s32ActualTemp / 100.0f;
                    }
                    else
                    {
                        SensorDisponible = false;
                        UARTprintf("Error de lectura BME280\n");
                    }
                }
            }
        }

        if (contadorPantalla >= 5)
        {
            contadorPantalla = contadorPantalla % 5;
            refrescarPantalla = true;
        }

        /* Cada estado reune su representacion y los eventos que puede atender. */
        switch (pantallaActual)
        {
        case PANTALLA_FECHA_HORA:
            if (refrescarPantalla)
            {
                dibujarPantallaFechaHora();
            }
            if (BotonAceptarFechaHora != 0)
            {
                pantallaActual = PANTALLA_INICIO;
                BotonAceptarFechaHora = 0;
                gestionarMedicacion();
                registrarExpiracionesPendientes();
            }
            break;

        case PANTALLA_INICIO:
            if (refrescarPantalla)
            {
                dibujarPantallaInicio();
            }
            if (BotonCuidador != 0)
            {
                pantallaActual = PANTALLA_CUIDADOR;
                BotonCuidador = 0;
                BotonVolver = 0;
            }
            break;

        case PANTALLA_CUIDADOR:
            if (refrescarPantalla)
            {
                dibujarPantallaCuidador();
            }
            if (BotonVolver != 0)
            {
                pantallaActual = PANTALLA_INICIO;
                BotonCuidador = 0;
                BotonVolver = 0;
            }
            else if (BotonConfiguracion != 0)
            {
                pantallaActual = PANTALLA_CONFIGURACION;
                BotonConfiguracion = 0;
                BotonVolverConfiguracion = 0;
            }
            else if (BotonConfirmarToma != 0)
            {
                confirmarYRegistrar(IndiceMedicamentoConfirmar, IndiceHorarioConfirmar);
                BotonConfirmarToma = 0;
            }
            break;

        case PANTALLA_CONFIGURACION:
            if (refrescarPantalla)
            {
                dibujarPantallaConfiguracion();
            }
            if (BotonVolverConfiguracion != 0)
            {
                pantallaActual = PANTALLA_CUIDADOR;
                BotonVolverConfiguracion = 0;
            }
            else if (BotonEditarMedicamento != 0)
            {
                /* El editor comienza siempre seleccionando el primer horario. */
                medicamentoEdicion = obtenerMedicamentoParaEdicion(IndiceMedicamentoEdicion);
                if (medicamentoEdicion != NULL)
                {
                    respaldoMedicamentoEdicion = *medicamentoEdicion;
                    hayRespaldoMedicamento = true;
                    pantallaActual = PANTALLA_EDICION_MEDICAMENTO;
                    IndiceHorarioEdicion = 0U;
                }
                BotonEditarMedicamento = 0;
                BotonVolverEdicionMedicamento = 0;
            }
            else if (BotonCrearMedicamento != 0)
            {
                creandoMedicamento = true;
                respaldoMedicamentoEdicion = (MedicConfig){0};
                hayRespaldoMedicamento = true;
                iniciarEdicionNombreMedicamento(NULL);
                pantallaActual = PANTALLA_NOMBRE_MEDICAMENTO;
                BotonCrearMedicamento = 0;
            }
            else if (BotonCambiarActividadMedicamento != 0)
            {
                medicamentoEdicion = obtenerMedicamentoParaEdicion(IndiceMedicamentoEdicion);
                if (medicamentoEdicion != NULL &&
                    establecerMedicamentoActivo(IndiceMedicamentoEdicion,
                                                !medicamentoEdicion->activo))
                {
                    gestionarMedicacion();
                }
                BotonCambiarActividadMedicamento = 0;
            }
            else if (BotonEliminarMedicamento != 0)
            {
                if (eliminarMedicamento(IndiceMedicamentoEdicion))
                {
                    guardarConfiguracionMedicacion();
                    gestionarMedicacion();
                }
                BotonEliminarMedicamento = 0;
            }
            break;

        case PANTALLA_EDICION_MEDICAMENTO:
            if (refrescarPantalla)
            {
                dibujarPantallaEdicionMedicamento();
            }
            if (BotonEditarNombreMedicamento != 0)
            {
                medicamentoEdicion = obtenerMedicamentoParaEdicion(IndiceMedicamentoEdicion);
                if (medicamentoEdicion != NULL)
                {
                    creandoMedicamento = false;
                    iniciarEdicionNombreMedicamento(medicamentoEdicion->nombre);
                    pantallaActual = PANTALLA_NOMBRE_MEDICAMENTO;
                }
                BotonEditarNombreMedicamento = 0;
            }
            else if (BotonVolverEdicionMedicamento != 0)
            {
                if (hayRespaldoMedicamento)
                {
                    restaurarMedicamento(IndiceMedicamentoEdicion,
                                         &respaldoMedicamentoEdicion);
                    gestionarMedicacion();
                    hayRespaldoMedicamento = false;
                }
                pantallaActual = PANTALLA_CONFIGURACION;
                BotonVolverEdicionMedicamento = 0;
            }
            else if (BotonEditarDiasHorario != 0)
            {
                pantallaActual = PANTALLA_DIAS_HORARIO;
                BotonEditarDiasHorario = 0;
                BotonVolverDiasHorario = 0;
            }
            else
            {
                if (BotonAgregarHorario != 0)
                {
                    medicamentoEdicion = obtenerMedicamentoParaEdicion(IndiceMedicamentoEdicion);
                    if (medicamentoEdicion == NULL)
                    {
                        indiceNuevoHorario = 0U;
                    }
                    else
                    {
                        indiceNuevoHorario = medicamentoEdicion->horariosUsados;
                    }
                    if (medicamentoEdicion != NULL &&
                        indiceNuevoHorario < MAX_HORARIOS_MEDICAMENTOS &&
                        agregarHorarioMedicamento(IndiceMedicamentoEdicion, 8U, 0U,
                                                  DIA_TODOS))
                    {
                        /* El nuevo horario queda seleccionado para editarlo inmediatamente. */
                        IndiceHorarioEdicion = indiceNuevoHorario;
                        gestionarMedicacion();
                    }
                    BotonAgregarHorario = 0;
                }
                else if (BotonCambiarActividadHorario != 0)
                {
                    horarioEdicion = obtenerHorarioParaEdicion(IndiceMedicamentoEdicion,
                                                               IndiceHorarioEdicion);
                    horarioActivo = (horarioEdicion != NULL &&
                        (horarioEdicion->diasSemana & HORARIO_DESACTIVADO) == 0U);
                    if (horarioEdicion != NULL &&
                        establecerHorarioActivo(IndiceMedicamentoEdicion,
                                                IndiceHorarioEdicion, !horarioActivo))
                    {
                        gestionarMedicacion();
                    }
                    BotonCambiarActividadHorario = 0;
                }
                else if (BotonEliminarHorario != 0)
                {
                    if (eliminarHorarioMedicamento(IndiceMedicamentoEdicion,
                                                   IndiceHorarioEdicion))
                    {
                        gestionarMedicacion();
                    }
                    BotonEliminarHorario = 0;
                }
                else if (BotonHoraMenos != 0 || BotonHoraMas != 0 ||
                         BotonMinutoMenos != 0 || BotonMinutoMas != 0)
                {
                    horarioEdicion = obtenerHorarioParaEdicion(IndiceMedicamentoEdicion,
                                                               IndiceHorarioEdicion);
                    if (horarioEdicion != NULL)
                    {
                        horaNueva = horarioEdicion->hora;
                        minutoNuevo = horarioEdicion->minuto;

                        /* Las horas y minutos son circulares para no salir de sus limites. */
                        if (BotonHoraMenos != 0)
                        {
                            if (horaNueva == 0U)
                            {
                                horaNueva = 23U;
                            }
                            else
                            {
                                horaNueva = (uint8_t)(horaNueva - 1U);
                            }
                        }
                        else if (BotonHoraMas != 0)
                        {
                            if (horaNueva == 23U)
                            {
                                horaNueva = 0U;
                            }
                            else
                            {
                                horaNueva = (uint8_t)(horaNueva + 1U);
                            }
                        }
                        else if (BotonMinutoMenos != 0)
                        {
                            if (minutoNuevo == 0U)
                            {
                                minutoNuevo = 59U;
                            }
                            else
                            {
                                minutoNuevo = (uint8_t)(minutoNuevo - 1U);
                            }
                        }
                        else
                        {
                            if (minutoNuevo == 59U)
                            {
                                minutoNuevo = 0U;
                            }
                            else
                            {
                                minutoNuevo = (uint8_t)(minutoNuevo + 1U);
                            }
                        }

                        if (editarHorarioMedicamento(IndiceMedicamentoEdicion,
                                                     IndiceHorarioEdicion, horaNueva,
                                                     minutoNuevo,
                                                     horarioEdicion->diasSemana))
                        {
                            gestionarMedicacion();
                        }
                    }
                    BotonHoraMenos = 0;
                    BotonHoraMas = 0;
                    BotonMinutoMenos = 0;
                    BotonMinutoMas = 0;
                }

                /* GUARDAR permanece separado: puede ejecutarse tras una edicion valida. */
                if (BotonGuardarMedicamento != 0)
                {
                    if (guardarConfiguracionMedicacion())
                    {
                        medicamentoEdicion =
                            obtenerMedicamentoParaEdicion(IndiceMedicamentoEdicion);
                        if (medicamentoEdicion != NULL)
                        {
                            respaldoMedicamentoEdicion = *medicamentoEdicion;
                            hayRespaldoMedicamento = true;
                        }
                    }
                    BotonGuardarMedicamento = 0;
                }
            }
            break;

        case PANTALLA_DIAS_HORARIO:
            if (refrescarPantalla)
            {
                dibujarPantallaDiasHorario();
            }
            if (BotonVolverDiasHorario != 0)
            {
                pantallaActual = PANTALLA_EDICION_MEDICAMENTO;
                BotonVolverDiasHorario = 0;
            }
            else if (BotonAlternarDiaHorario != 0)
            {
                if (alternarDiaHorario(IndiceMedicamentoEdicion,
                                       IndiceHorarioEdicion, DiaHorarioEdicion))
                {
                    gestionarMedicacion();
                }
                BotonAlternarDiaHorario = 0;
            }
            break;

        case PANTALLA_NOMBRE_MEDICAMENTO:
            if (refrescarPantalla)
            {
                dibujarPantallaNombreMedicamento();
            }
            if (BotonCancelarNombreMedicamento != 0)
            {
                if (creandoMedicamento)
                {
                    pantallaActual = PANTALLA_CONFIGURACION;
                }
                else
                {
                    pantallaActual = PANTALLA_EDICION_MEDICAMENTO;
                }
                creandoMedicamento = false;
                if (pantallaActual == PANTALLA_CONFIGURACION)
                {
                    hayRespaldoMedicamento = false;
                }
                BotonCancelarNombreMedicamento = 0;
            }
            else if (BotonAceptarNombreMedicamento != 0)
            {
                if ((creandoMedicamento &&
                     crearMedicamento(IndiceMedicamentoEdicion,
                                       obtenerNombreEdicionMedicamento())) ||
                    (!creandoMedicamento &&
                     editarNombreMedicamento(IndiceMedicamentoEdicion,
                                             obtenerNombreEdicionMedicamento())))
                {
                    gestionarMedicacion();
                    if (creandoMedicamento)
                    {
                        IndiceHorarioEdicion = 0U;
                    }
                    pantallaActual = PANTALLA_EDICION_MEDICAMENTO;
                    creandoMedicamento = false;
                }
                BotonAceptarNombreMedicamento = 0;
            }
            break;

        default:
            /* Un estado no reconocido no se procesa ni modifica los datos existentes. */
            break;
        }

    }
}






