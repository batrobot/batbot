/*
* File: main.c
*
* C/C++ source code generated on  : Thu Jun 4 11:12:24 2015
* C/C++ source code generated by  : Alireza Ramezani, Champaign-Urbana-IL
* Target selection: stm32F4xx.tlc
* Embedded hardware selection: STMicroelectronics->STM32F4xx 32-bit Cortex-M4
* Code generation objectives:
*    1. Execution efficiency
*    2. ROM efficiency
*    3. RAM efficiency
*    4. Traceability
*    5. Safety precaution
*    6. Debugging
*    7. MISRA-C:2004 guidelines
*    8. Polyspace Model Link for Simulink
* Validation result: Passed (14), Warnings (2), Error (1)
*
*
*
* ******************************************************************************
* * attention
* *
* * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
* * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
* * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
* *
* ******************************************************************************
*/

#include <stdio.h>
#include "VN_lib.h"
#include "VN_user.h"
#include "stddef.h"
#include "stm32f4xx.h"
#include "daq.h"
#include "controller.h"
#include "rtwtypes.h"
#include "debug.h"
#include "as5048b.h"
#include "interface_board.h"
#include "stm32f4_sdio_sd.h"
#include "sdio_debug.h"
#include "diskio.h"
#include "ff.h"

/* DAQ and controller loop at this rate */
#define SAMPLE_TIME 0.002

/* Uncomment for debug msg over USART port */
#define DEBUG_MAIN

/* this variable is used to create a time reference incremented by 1 mili-second */
//#define SYSTEMTICK_PERIOD_MS 0.1

/* this variables are used to allocate a timer for the main application */
#define MAIN_TIM TIM7
#define MAIN_RCC_APBPeriph RCC_APB1Periph_TIM7

/* RCC_Configuration defined in RCC_Configuration.c file */
/* Clock configuration function provided as example */
extern void RCC_Configuration(void);

/* Real-time model */
extern RT_MODEL_daq *const daq_M;

/* Set which subrates need to run this base step (base rate always runs).*/
/* Defined in daq.c file */
extern void daq_SetEventsForThisBaseStep(boolean_T*);

/* Flags for taskOverrun */
static boolean_T OverrunFlags[1];

/* Number of auto reload timer rotation computed */
static uint32_t autoReloadTimerLoopVal_S = 1;

/* Remaining number of auto reload timer rotation to do */
static uint32_t remainAutoReloadTimerLoopVal_S = 1;

/* These vars are used for calibration purposes */
extern boolean_T calibrate_encs;
extern boolean_T save_param_on_SD;
extern double motor_cmd[4];

/* PWM duty cicle to PA3, PB1, PE5, PE6 */
int16_T _pwm_pa3;
int16_T _pwm_pb1;
int16_T _pwm_pe5;
int16_T _pwm_pe6;

/* Work area (file system object) for logical drive */
SD_Error SD_err;
FATFS fs;
FIL	data;							// File object 
FIL	param;
FRESULT	fresult;					// FatFs return code 
FILINFO info;
SD_Error SDstatus;
char_T buffstr[1000];			// Line buffer 
char_T filename[16];			// File name buffer
char_T line[1000];				// Line buffer 
UINT bw, strlength;

/* Some vars to read encoders */
uint16_t angleReg[4] = {0, 0, 0, 0};
uint8_t autoGain[4] = {0, 0, 0, 0};
uint8_t diag[4] = {0, 0, 0, 0};
uint16_t magnitude[4] = {0, 0, 0, 0};
double angle[4] = {0, 0, 0, 0};

/* Some vars for vector nav measurments */
VN100_SPI_Packet *packet;
float yaw, pitch, roll;

/* Private functions */
void NVIC_Configuration(void);
void main_delay_us(unsigned long us);
void main_delay_ms(unsigned long ms);
void main_initialize_us_timer(void);

/****************************************************
SysTick_Handler function
This handler is called every tick and schedules tasks
*****************************************************/
void SysTick_Handler(void)
{
	/* Manage nb of loop before interrupt has to be processed */
	remainAutoReloadTimerLoopVal_S--;
	if (remainAutoReloadTimerLoopVal_S) {
		return;
	}

	remainAutoReloadTimerLoopVal_S = autoReloadTimerLoopVal_S;

	/* Check base rate for overrun */
	if (OverrunFlags[0]) {
		rtmSetErrorStatus(daq_M, "Overrun");
		return;
	}

	OverrunFlags[0] = true;
	
	/* read encoders */
	AS5048B_readBodyAngles(angleReg, autoGain, diag, magnitude, angle);
	
	/* vn100 read yaw, pitch, and roll */
	packet = VN100_SPI_GetYPR(0, &yaw, &pitch, &roll);
	
	/* inpute to controller here */
	controller_U.pid_gian[0]; // forelimb Kp 
	controller_U.pid_gian[1]; // leg Kp
	controller_U.pid_gian[2]; // forelimb Kd 
	controller_U.pid_gian[3]; // leg Kd
	controller_U.pid_gian[4]; // forelimb Ki 
	controller_U.pid_gian[5]; // leg Ki 
	controller_U.actuator_ctrl_params[0] = 100; // PID_SATURATION_THRESHOLD [pwm] (control sat, used in pid func)
	controller_U.actuator_ctrl_params[1] = 20; // MAX_ANGLE_DIFFERENCE [deg\sec] (used in anti-roll-over func)
	controller_U.actuator_ctrl_params[2] = 360; // ANTI_ROLLOVER_CORRECTION [deg] (used in anti-roll-over func)
	controller_U.actuator_ctrl_params[3] = 275; // MAX_RP_ANGLE_RIGHT [deg] 
	controller_U.actuator_ctrl_params[4] = 205; // MAX_DV_ANGLE_RIGHT [deg]
	controller_U.actuator_ctrl_params[5] = 225; // MIN_RP_ANGLE_RIGHT [deg]
	controller_U.actuator_ctrl_params[6] = 145; // MIN_DV_ANGLE_RIGHT [deg]
	controller_U.actuator_ctrl_params[7] = 110; // MAX_RP_ANGLE_LEFT [deg]
	controller_U.actuator_ctrl_params[8] = 80; // MAX_DV_ANGLE_LEFT [deg]
	controller_U.actuator_ctrl_params[9] = 60; // MIN_RP_ANGLE_LEFT [deg]
	controller_U.actuator_ctrl_params[10] = 0; // MIN_DV_ANGLE_LEFT [deg]
	controller_U.actuator_ctrl_params[11] = SAMPLE_TIME; // Sample interval
	controller_U.actuator_ctrl_params[12];		// PID_TRACKING_PRECISION_THRESHOLD
	controller_U.actuator_ctrl_params[13];		// ANTI_WINDUP_THRESHOLD

	controller_U.flight_ctrl_params[0]; // ROLL_SENSITIVITY [-] -0.01
	controller_U.flight_ctrl_params[1];  // PITCH_SENSITIVITY [-] 0.001
	controller_U.flight_ctrl_params[2] = 0.0;  // MAX_FORELIMB_ANGLE [deg] (n.a.)
	controller_U.flight_ctrl_params[3] = 0.0;  // MIN_FORELIMB_ANGLE [deg] (n.a.)
	controller_U.flight_ctrl_params[4] = 0.0;  // MAX_LEG_ANGLE [deg] (n.a.)
	controller_U.flight_ctrl_params[5] = 0.0;  // MIN_LEG_ANGLE [deg] (n.a.)
	
	controller_U.flight_ctrl_params[6];  // R_foreq  [deg]
	controller_U.flight_ctrl_params[7];  // L_foreq [deg]
	controller_U.flight_ctrl_params[8];  // R_leq [deg]
	controller_U.flight_ctrl_params[9];  // L_leq [deg]
	
	// IMU
	controller_U.roll = roll;
	controller_U.pitch = pitch;
	controller_U.yaw = yaw;
	
	// Encoders
	controller_U.angle[0] = angle[0]; // Right forelimb
	controller_U.angle[1] = angle[2]; // Left forelimb
	controller_U.angle[2] = 0; // Right tail
	controller_U.angle[3] = angle[1]; // Left tail 
	
	/* Step the controller for base rate */
	controller_step();
		
	/* drv8835 commands */
	if (!calibrate_encs)
	{
		daq_U.pwm_pa6 = controller_Y.M0A; // Right forelimb 
		daq_U.pwm_pa7 = controller_Y.M0B;
		daq_U.pwm_pb0  = controller_Y.M1A; // Left forelimb
		_pwm_pb1  = controller_Y.M1B;
		daq_U.pwm_pa1; // Right leg
		daq_U.pwm_pa0;
		daq_U.pwm_pa2 = controller_Y.M3B; // Left leg
		_pwm_pa3 = controller_Y.M3A;
	}
	else
	{						// Calibrate sensors by driving the spindle...
		daq_U.pwm_pa6 = 0;
		daq_U.pwm_pa7 = 0;
		daq_U.pwm_pb0  = 0;
		_pwm_pb1  = 0;
		daq_U.pwm_pa1 = 0; 
		daq_U.pwm_pa0 = 0;
		daq_U.pwm_pa2 = 0; 
		_pwm_pa3 = 0;
		
		if (motor_cmd[0] > 0) // M1
		{
			daq_U.pwm_pa6 = motor_cmd[0]; 
		}
		else
		{
			daq_U.pwm_pa7 = -motor_cmd[0];
		} 
		
		if (motor_cmd[1] > 0) // M2
		{
			daq_U.pwm_pb0 = motor_cmd[1]; 
		}
		else
		{
			_pwm_pb1 = -motor_cmd[1];
		} 
		
		if (motor_cmd[2] > 0) // M3
		{
			daq_U.pwm_pa1 = motor_cmd[2]; 
		}
		else
		{
			daq_U.pwm_pa0 = -motor_cmd[2];
		} 
		
		if (motor_cmd[3] > 0) // M4
		{
			daq_U.pwm_pa2 = motor_cmd[3]; 
		}
		else
		{
			_pwm_pa3 = -motor_cmd[3];
		} 	
	}
	
	
	/* TMS320 commands*/
	_pwm_pe5 = controller_Y.ubldc[0];	// right armwing
	_pwm_pe6 = controller_Y.ubldc[1];	// left armwing
	
	
	/* Step the model for base rate */
	daq_step();
	INTERFACE_BOARD_pwmGenerator();
	
#define _REC_SD_CARD
#ifdef _REC_SD_CARD
	if (SD_err == SD_OK)
	{
		/////////////////////////////// buffer data on SD card  ///////////////////////////////////
		/* Data structure is as following */
		// 1st col: time	2nd col: roll	3rd col: pitch	4th col: yaw	5th col: enc1	6th col: enc2	7th col: enc3	8th col:enc4	9th col: u1	10th col: u2	11th col: u3	12th col: u4
	
		/* find the length of the datalog file */
		fresult = f_stat("data.txt", &info);  
 
		sprintf(filename, "data.txt"); 
		fresult = f_open(&data, filename, FA_OPEN_ALWAYS | FA_WRITE);
		
		/* If the file existed seek to the end */
		if (fresult == FR_OK) f_lseek(&data, info.fsize);
		
		sprintf(buffstr,
			"%f, %f, %f, %f, %f, %f, %f, %f \r\n",
			controller_Y.time,
			controller_Y.q[0],
			controller_Y.q[1], 
			controller_Y.q[2],
			angle[0],
			angle[1],
			angle[2],
			angle[3]);
		fresult = f_write(&data, buffstr, strlen(buffstr), &bw);
		
		f_close(&data); 
		
		if (save_param_on_SD)
		{
			/* Save params as param.txt file on SD card. */ 
			// forelimb Kp 
			// leg Kp
			// forelimb Kd 
			// leg Kd
			// forelimb Ki 
			// leg Ki 
			// PID_SATURATION_THRESHOLD [pwm] (control sat, used in pid func)
			// MAX_ANGLE_DIFFERENCE [deg\sec] (used in anti-roll-over func)
			// ANTI_ROLLOVER_CORRECTION [deg] (used in anti-roll-over func)
			// MAX_RP_ANGLE_RIGHT [deg] 
			// MAX_DV_ANGLE_RIGHT [deg]
			// MIN_RP_ANGLE_RIGHT [deg]
			// MIN_DV_ANGLE_RIGHT [deg]
			// MAX_RP_ANGLE_LEFT [deg]
			// MAX_DV_ANGLE_LEFT [deg]
			// MIN_RP_ANGLE_LEFT [deg]
			// MIN_DV_ANGLE_LEFT [deg]
			// Sample interval
			// PID_TRACKING_PRECISION_THRESHOLD
			// ANTI_WINDUP_THRESHOLD
			// ROLL_SENSITIVITY [-] -0.01
			// PITCH_SENSITIVITY [-] 0.001
			// MAX_FORELIMB_ANGLE [deg] (n.a.)
			// MIN_FORELIMB_ANGLE [deg] (n.a.)
			// MAX_LEG_ANGLE [deg] (n.a.)
			// MIN_LEG_ANGLE [deg] (n.a.)
			// R_foreq  [deg]
			// L_foreq [deg]
			// R_leq [deg]
			// L_leq [deg]
			
			sprintf(filename, "param.txt"); 
			/* open file on SD card */
			while (f_open(&param, filename, FA_OPEN_ALWAYS | FA_WRITE) != FR_OK)
				;
	//
			///* write the header to file */
			//char *header = "I wrote params on SD for you :). \r\n";
			//while (f_write(&param, header, strlen(header), &bw) != FR_OK)	// Write data to the file 
				//;	
			
			/* close file */
			f_close(&param);
			
			/* Flight params are saved on SD card per request... */
			// 1st col: .....
	
			/* find the length of the datalog file */
			fresult = f_stat("param.txt", &info);  
		
			fresult = f_open(&param, filename, FA_OPEN_ALWAYS | FA_WRITE);
		
			/* If the file existed seek to the end */
			//if (fresult == FR_OK) f_lseek(&param, info.fsize);
			sprintf(buffstr,
				"%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f,\r\n", 
				controller_U.pid_gian[0],
				controller_U.pid_gian[1],
				controller_U.pid_gian[2],
				controller_U.pid_gian[3],
				controller_U.pid_gian[4],
				controller_U.pid_gian[5],
				controller_U.actuator_ctrl_params[0],
				controller_U.actuator_ctrl_params[1],
				controller_U.actuator_ctrl_params[2],
				controller_U.actuator_ctrl_params[3],
				controller_U.actuator_ctrl_params[4],
				controller_U.actuator_ctrl_params[5],
				controller_U.actuator_ctrl_params[6],
				controller_U.actuator_ctrl_params[7],
				controller_U.actuator_ctrl_params[8],
				controller_U.actuator_ctrl_params[9],
				controller_U.actuator_ctrl_params[10],
				controller_U.actuator_ctrl_params[11],
				controller_U.actuator_ctrl_params[12],
				controller_U.actuator_ctrl_params[13],
				controller_U.flight_ctrl_params[0],
				controller_U.flight_ctrl_params[1],
				controller_U.flight_ctrl_params[2],
				controller_U.flight_ctrl_params[3],
				controller_U.flight_ctrl_params[4],
				controller_U.flight_ctrl_params[5],
				controller_U.flight_ctrl_params[6],
				controller_U.flight_ctrl_params[7],
				controller_U.flight_ctrl_params[8],
				controller_U.flight_ctrl_params[9]);
			fresult = f_write(&param, buffstr, strlen(buffstr), &bw);
			f_close(&param); 
			
			// update 
			save_param_on_SD = false;
		}
		
		/////////////////////////////// buffer data on SD card  ///////////////////////////////////	
	}
	
#endif

#define _DEBUG
#ifdef _DEBUG
	/////////////////////////////// DEBUG  ///////////////////////////////////
	
	debug_bat_robot();
	
	/////////////////////////////// DEBUG  ///////////////////////////////////
#endif 
	
	/* Indicate task for base rate complete */
	OverrunFlags[0] = false;
}

/****************************************************
main function
Example of main :
- Clock configuration
- call Initialize
- Wait for systick (infinite loop)
*****************************************************/
int main (void)
{

	/* Data initialization */
	int_T i;
	for (i=0;i<1;i++) {
		OverrunFlags[i] = 0;
	}

	/* Clock has to be configured first*/
	RCC_Configuration();
	
	/* initialize interupt vector */
	NVIC_Configuration();
	
	/* Initilaize the main application timer */
	main_initialize_us_timer();

	/* Model initialization call */
	daq_initialize();
	INTERFACE_BOARD_initialize();
	
	/* vn100 spi initialization */
	SPI_initialize();	
	
	/* tare imu */
	VN100_SPI_Tare(0);
	
	/* controller initialization call */
	controller_initialize();
	
	/* initialize encoders */
	AS5048B_initialize();
	
#ifdef _REC_SD_CARD
	
	/* initialize SD card */
	
	SD_err = SD_Init(); 
	if (SD_err == SD_OK)
	{
		/////////////////////////////// Make a quick header for SD log file ///////////////////////////////////
	
		/* Register work area to the default drive */
		f_mount(0, &fs);
	
		/* Save as data.txt file on SD card. */ 
		sprintf(filename, "data.txt"); 
	
		/* open file on SD card */
		while (f_open(&data, filename, FA_OPEN_ALWAYS | FA_WRITE) != FR_OK)
			;
	
		/* write the header to file */
		char *header = "Hello World! This is B2 talking! I am saving data on SD card. \r\n";
		while (f_write(&data, header, strlen(header), &bw) != FR_OK)	// Write data to the file 
			;	
			
			/* close file */
		f_close(&data);
	
		/* open file on SD card */
		char *pch;
		float var, varVector[30];
		uint8_T index = 0;
		
		/* Read param.txt file on SD card. */ 
		sprintf(filename, "param.txt"); 
		if (f_open(&param, filename, FA_READ) == FR_OK)
		{
			/* Read param line */
			f_gets(line, sizeof line, &param);
			pch = strtok(line, " ,");
			while (pch != NULL)
			{
				sscanf(pch, "%f", &var);
				pch = strtok(NULL, " ,");
				varVector[index] = var;
				index++;
			}
		
			/* Write servo params */
			for (index = 0;index < 6;index++)
			{
				controller_U.pid_gian[index] = varVector[index];
			}
			for (index = 0;index < 14;index++)
			{
				controller_U.actuator_ctrl_params[index] = varVector[index + 6];
			}
			/* Write fligth control params */
			for (index = 0;index < 10;index++)
			{
				controller_U.flight_ctrl_params[index] = varVector[index + 20];
			}
		}
		else
		{
			/* Write servo params */
			for (index = 0;index < 6;index++)
			{
				controller_U.pid_gian[index] = 0;
			}
			for (index = 0;index < 14;index++)
			{
				controller_U.actuator_ctrl_params[index] = 0;
			}
			/* Write fligth control params */
			for (index = 0;index < 10;index++)
			{
				controller_U.flight_ctrl_params[index] = 0;
			}
		}
		
		
		
		/* Close the file */
		f_close(&param);
		
		/////////////////////////////// Make a quick header for SD log file ///////////////////////////////////	
	}
	
	
#endif
	
	/* Systick configuration and enable SysTickHandler interrupt */
	float dt = SAMPLE_TIME;
	if (SysTick_Config((uint32_t)(SystemCoreClock * dt))) {
		autoReloadTimerLoopVal_S = 1;
		do {
			autoReloadTimerLoopVal_S++;
		} while ((uint32_t)(SystemCoreClock * dt)/autoReloadTimerLoopVal_S >
			SysTick_LOAD_RELOAD_Msk);

		SysTick_Config((uint32_t)(SystemCoreClock * dt)/autoReloadTimerLoopVal_S);
	}

	remainAutoReloadTimerLoopVal_S = autoReloadTimerLoopVal_S;//Set nb of loop to do

	
	/* Infinite loop */
	/* Real time from systickHandler */
	while (1) 
	{	
		//
	}	
}


/*******************************************************************************
* Function Name  : NVIC_Configuration
* Input          : 
* Output         : None
* Return         : 
* Description    : 
*******************************************************************************/
void NVIC_Configuration(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	  /* Configure the NVIC Preemption Priority Bits */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

	NVIC_InitStructure.NVIC_IRQChannel = SDIO_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	// DMA2 STREAMx Interrupt ENABLE
	NVIC_InitStructure.NVIC_IRQChannel = SD_SDIO_DMA_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_Init(&NVIC_InitStructure);

	//NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	//NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	//NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	//NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	//NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	//NVIC_Init(&NVIC_InitStructure);
}

/*******************************************************************************
* Function Name  : main_delay_us
* Input          : 
* Output         : None
* Return         : 
* Description    : 
*******************************************************************************/

void main_delay_us(unsigned long us)
{
	uint16_t t0 = MAIN_TIM->CNT;
	for (;;) {
		int t = MAIN_TIM->CNT;
		if (t < t0){
			t += 0x10000;
		}

		if (us < t - t0)
			return;

		us -= t - t0;
		t0 = t;
	}
}


/*******************************************************************************
* Function Name  : main_delay_ms
* Input          : 
* Output         : None
* Return         : 
* Description    : 
*******************************************************************************/

void main_delay_ms(unsigned long ms)
{
	main_delay_us(ms * 1000);
}


/*******************************************************************************
* Function Name  : main_initialize_us_timer
* Input          : 
* Output         : None
* Return         : 
* Description    : 
*******************************************************************************/

void main_initialize_us_timer()
{
	RCC_ClocksTypeDef RCC_Clocks;
	RCC_GetClocksFreq(&RCC_Clocks);

	RCC->APB1ENR |= MAIN_RCC_APBPeriph;
	MAIN_TIM->PSC = (RCC_Clocks.PCLK1_Frequency / 1000000) - 1;
	MAIN_TIM->ARR = 0xFFFF;
	MAIN_TIM->CR1 = TIM_CR1_CEN;
}



/* File trailer for Real-Time Workshop generated code.
*
* [EOF] main.c
*/
