
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

#define FREC 50

int RELOJ;
uint8_t pantallaActual = 0;
float T_act = 0.0f;

// Numero de interrupciones pendientes de procesar
volatile uint32_t ticksPendientes = 0;

// Interrupcion del Timer0A: cada 20 ms
void IntTimer(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    ticksPendientes++;
}

/* Imprime por UART el historial que mantiene el modulo de registro. */
static void mostrarHistorialUART(void);

void confirmarYRegistrar(uint8_t indiceMedicamento, char letraMedicamento)
{
    RegistroToma nuevaToma = {0};

    const HorarioConfig *horario;
    uint8_t hora, minuto, segundo;

    // Consultar el horario antes de confirmar
    horario = obtenerHorarioConfigurado(indiceMedicamento, 0);
    if (horario == NULL)
    {
        return;
    }

    // Solo registrar si se ha confirmado correctamente
    if (confirmarToma(indiceMedicamento, 0))
    {
        // Medicamento
        nuevaToma.medicamento = letraMedicamento;

        // Hora prevista
        nuevaToma.horaProgramada = horario->hora;
        nuevaToma.minutoProgramada = horario->minuto;

        // Hora actual del reloj
        obtenerHora(&hora, &minuto, &segundo);

        nuevaToma.horaConfirmada = hora;
        nuevaToma.minutoConfirmada = minuto;

        // Pendiente: obtener un numero de dia
        nuevaToma.diaToma = 0;

        // Guardar el registro
        agregarRegistro(nuevaToma);
        mostrarHistorialUART();
    }
}


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

            UARTprintf("Confirmada: %02u:%02u\r\n",
                       registro->horaConfirmada,
                       registro->minutoConfirmada);

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

    bool Bme_OK = false;
    uint8_t Sensor_OK = 0;

    s32 g_s32ActualTemp = 0;
    u32 g_u32ActualPress = 0U;
    u32 g_u32ActualHumity = 0U;

    const int32_t REG_CAL[6] = {CAL_DEFAULTS};

    // 1. Configurar el reloj del microcontrolador
    RELOJ = SysCtlClockFreqSet(
        SYSCTL_XTAL_25MHZ |
        SYSCTL_OSC_MAIN |
        SYSCTL_USE_PLL |
        SYSCTL_CFG_VCO_480,
        120000000
    );

    // 2. Inicializar pantalla FT800
    HAL_Init_SPI(1, RELOJ);
    Inicia_pantalla();

    // 3. Configurar la calibracion tactil
    for (i = 0; i < 6; i++)
    {
        Esc_Reg(REG_TOUCH_TRANSFORM_A + 4*i, REG_CAL[i]);
    }

    // 4.Habilitar UART
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTStdioConfig(0, 115200, RELOJ);

    // 5. Inicializar nuestros modulos
    inicializarReloj();
    inicializarMedicacion();
    gestionarMedicacion();
    inicializarRegistro();

    //Historial de la UART
    mostrarHistorialUART();

    // 6. Cargar imagen JPG
    cargarImagenPastillero();

    // 7. Configurar Timer0A a 50 Hz
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (RELOJ/FREC)-1);

    TimerIntRegister(TIMER0_BASE, TIMER_A, IntTimer);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    IntMasterEnable();
    TimerEnable(TIMER0_BASE, TIMER_A);

    // 8. Pantalla principal
    dibujarPantallaInicio();


        //Configuracion del boosterpack
        if (Detecta_BP(1))
         {
             UARTprintf("\nBOOSTERPACK detectado en posicion 1");
             UARTprintf("\nConfigurando puerto I2C0");

             Conf_Boosterpack(1, RELOJ);
         }
         else if (Detecta_BP(2))
         {
             UARTprintf("\nBOOSTERPACK detectado en posicion 2");
             UARTprintf("\nConfigurando puerto I2C2");

             Conf_Boosterpack(2, RELOJ);
         }
         else
         {
             UARTprintf("\nNingun BOOSTERPACK detectado");
             return 0;
         }

        UARTprintf("Inicializando BME280... ");

        Sensor_OK = Test_I2C_Dir(BME280_I2C_ADDRESS2);

        //Comprobar que arranque el BME280
        if (!Sensor_OK)
        {
            UARTprintf("Error en BME280\n");
            Bme_OK = 0;
        }
        else
        {
            bme280_data_readout_template();
            bme280_set_power_mode(BME280_NORMAL_MODE);

            UARTprintf("BME280 inicializado correctamente\n");
            Bme_OK = 1;
        }

    // 9. Bucle principal
    while (1)
    {
        /* Solo se consulta I2C si la inicializacion del sensor fue correcta. */
        if (Bme_OK)
        {
            bme280_read_pressure_temperature_humidity(
                &g_u32ActualPress,
                &g_s32ActualTemp,
                &g_u32ActualHumity
            );
            T_act = (float)g_s32ActualTemp / 100.0f;
        }

        // Recoger las interrupciones acumuladas
        IntMasterDisable();
        pendientes = ticksPendientes;
        ticksPendientes = 0;
        IntMasterEnable();

        for (i = 0; i < pendientes; i++)
        {
            contadorReloj++;
            contadorPantalla++;

            // Cada 50 interrupciones: 1 segundo
            if (contadorReloj >= FREC)
            {
                contadorReloj = 0;

                actualizarReloj();
                gestionarMedicacion();
            }
        }

        // Cada 5 interrupciones: refrescar pantalla
        if (contadorPantalla >= 5)
        {
            contadorPantalla = contadorPantalla % 5;

            if (pantallaActual == 0)
            {
                dibujarPantallaInicio();
            }
            else if (pantallaActual == 1)
            {
                dibujarPantallaCuidador();
            }
        }
        if (pantallaActual == 0 && BotonCuidador != 0)
        {
            pantallaActual = 1;
            BotonCuidador = 0;
            BotonVolver = 0;
        }
        else if (pantallaActual == 1 && BotonVolver != 0)
        {
            pantallaActual = 0;
            BotonCuidador = 0;
            BotonVolver = 0;
        }
        if (pantallaActual == 1 && BotonConfirmarA != 0)
        {
            UARTprintf("BOTON A DETECTADO\r\n");
            confirmarYRegistrar(0, 'A');
            BotonConfirmarA = 0;
        }
        if (pantallaActual == 1 && BotonConfirmarB != 0)
        {
            confirmarYRegistrar(1, 'B');
            BotonConfirmarB = 0;
        }
        if (pantallaActual == 1 && BotonConfirmarC != 0)
        {
            confirmarYRegistrar(2, 'C');
            BotonConfirmarC = 0;
        }

    }
}






