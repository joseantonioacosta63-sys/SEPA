
#include <stdint.h>
#include <stdbool.h>

#include "driverlib2.h"
#include "FT800_TIVA.h"

#include "reloj.h"
#include "medicacion.h"
#include "pantalla.h"

#define FREC 50

int RELOJ;

// Numero de interrupciones pendientes de procesar
volatile uint32_t ticksPendientes = 0;

// Interrupcion del Timer0A: cada 20 ms
void IntTimer(void)
{
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
    ticksPendientes++;
}

int main(void)
{
    uint32_t pendientes;
    uint32_t i;

    uint32_t contadorReloj = 0;
    uint32_t contadorPantalla = 0;

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

    // 4. Inicializar nuestros modulos
    inicializarReloj();
    inicializarMedicacion();
    gestionarMedicacion();

    // 5. Cargar imagen JPG
    cargarImagenPastillero();

    // 6. Configurar Timer0A a 50 Hz
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);

    TimerClockSourceSet(TIMER0_BASE, TIMER_CLOCK_SYSTEM);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER0_BASE, TIMER_A, (RELOJ/FREC)-1);

    TimerIntRegister(TIMER0_BASE, TIMER_A, IntTimer);
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    IntMasterEnable();
    TimerEnable(TIMER0_BASE, TIMER_A);

    // 7. Dibujar inicialmente la pantalla principal
    dibujarPantallaInicio();

    // 8. Bucle principal
    while (1)
    {
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

            dibujarPantallaInicio();
        }
    }
}






