/*
===============================================================================
 Name        : main.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include <cr_section_macros.h>

// TODO: insert other include files here
#include "FreeRTOS.h"
#include "task.h"
#include "DigitalIoPin.h"

#include "semphr.h"
// TODO: insert other definitions and declarations here

/* Sets up system hardware */
static void prvSetupHardware(void)
{
	SystemCoreClockUpdate();
	Board_Init();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}




class Rack { /* class Rack strictly needs FreeRTOS */
	DigitalIoPin switchLIM1;
	DigitalIoPin switchLIM2;
	/* comment on what stepper's DIR and STEP pins are */
	DigitalIoPin DIR;
	DigitalIoPin STEP;

	bool bDIR;
	TickType_t xPulseXms;

static const bool dirCLw = false;
static const bool dirCCLw = true;
static const TickType_t x1ms = configTICK_RATE_HZ / 1000;

public:

	bool lim1();
	bool lim2();

	bool dir();

	void step();
	void dirTOGGLE();
	void dirMATCH();
	void dirWRITE(bool dirVal);

public:

	Rack(int portLS1, int pinLS1,
			int portLS2, int pinLS2,
			int portDIR, int pinDIR,
			int portSTEP, int pinSTEP,
			bool dirINIT = dirCLw,
			TickType_t xPulseXms = x1ms)
			:	switchLIM1( portLS1, pinLS1, DigitalIoPin::pullup, true ),
				switchLIM2( portLS2, pinLS2, DigitalIoPin::pullup, true ),
				DIR(  portDIR , pinDIR , DigitalIoPin::output, false ),
				STEP( portSTEP, pinSTEP, DigitalIoPin::output, false ),
				bDIR(       dirINIT  ),
				xPulseXms( xPulseXms )
	{}

public:

	void indicateLS( int indicatorPORT1, int indicatorPIN1,
			int indicatorPORT2, int indicatorPIN2 );
	void buttonCOMMAND( int portBUTTON1, int pinBUTTON1,
			int portBUTTON2, int pinBUTTON2 );
};

void Rack::step()
{
	STEP.write( false );
	vTaskDelay( xPulseXms );
	STEP.write( true );
	vTaskDelay( xPulseXms );
}

bool Rack::lim1()
{
	return switchLIM1.read();
}

bool Rack::lim2()
{
	return switchLIM2.read();
}

bool Rack::dir()
{
	return bDIR;
}

void Rack::dirTOGGLE()
{
	bDIR = !bDIR;
	DIR.write( bDIR );
}

void Rack::dirMATCH()
{
	DIR.write( bDIR );
}

void Rack::dirWRITE(bool dirVal)
{
	DIR.write( bDIR = dirVal );
}

void Rack::indicateLS( int indicatorPORT1, int indicatorPIN1,
			int indicatorPORT2, int indicatorPIN2 )
{
	DigitalIoPin led1(indicatorPORT1, indicatorPIN1, DigitalIoPin::output, false);
	DigitalIoPin led2(indicatorPORT2, indicatorPIN2, DigitalIoPin::output, false);

	bool lim1, lim2;
	for( ;; )
	{
		lim1 = switchLIM1.read();
		lim2 = switchLIM2.read();

		if( lim1 ) led1.write( false );
		else       led1.write( true );

		if( lim2 ) led2.write( false );
		else       led2.write( true );

		if( !(lim1 || lim2) ) vTaskDelay( x1ms );
	}
}

void Rack::buttonCOMMAND( int portBUTTON1, int pinBUTTON1,
		int portBUTTON2, int pinBUTTON2 )
{
	DigitalIoPin switchCCLw(portBUTTON1, pinBUTTON1, DigitalIoPin::pullup, true);
	DigitalIoPin switchCLw( portBUTTON2, pinBUTTON2, DigitalIoPin::pullup, true);

	bool limCCLw, limCLw;
	bool swCCL, swCL;
	for( ;; )
	{
		limCCLw = switchLIM1.read();
		limCLw = switchLIM2.read();

		swCCL = switchCCLw.read();
		swCL = switchCLw.read();

		if( swCL && swCCL )
		{}
		else if( swCL && !limCLw )
		{
			dirWRITE( dirCLw );
			step();
		}
		else if( swCCL && !limCCLw )
		{
			dirWRITE( dirCCLw );
			step();
		}
		else /* no button pressed */
		{
			vTaskDelay( x1ms );
		}
	}
}

static Rack *prack1;
static void vLimit(void *pvParameters)
{
	prack1->indicateLS(0, 25, 0, 3);
}
static void vButton(void *pvParameters)
{
	prack1->buttonCOMMAND(0, 17, 1, 9);
}

/* the following is required if runtime statistics are to be collected */
extern "C" {

void vConfigureTimerForRunTimeStats( void ) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}

}
/* end runtime statictics collection */

/**
 * @brief	main routine for FreeRTOS blinky example
 * @return	Nothing, function should not exit
 */
int main(void)
{
	prvSetupHardware();

	prack1 = new Rack(0, 27, 0, 28,
			1, 0, 0, 24);

	xTaskCreate(vLimit, "indicate limit switch",
			configMINIMAL_STACK_SIZE + 128, NULL,
			(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	xTaskCreate(vButton, "drive",
				configMINIMAL_STACK_SIZE + 128, NULL,
				(tskIDLE_PRIORITY + 1UL), (TaskHandle_t *) NULL);

	vTaskStartScheduler();

	return 1;
}
