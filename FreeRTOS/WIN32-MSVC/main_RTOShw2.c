/*
 * FreeRTOS Kernel V10.2.1
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/******************************************************************************
 * NOTE: Windows will not be running the FreeRTOS demo threads continuously, so
 * do not expect to get real time behaviour from the FreeRTOS Windows port, or
 * this demo application.  Also, the timing information in the FreeRTOS+Trace
 * logs have no meaningful units.  See the documentation page for the Windows
 * port for further information:
 * http://www.freertos.org/FreeRTOS-Windows-Simulator-Emulator-for-Visual-Studio-and-Eclipse-MingW.html
 *
 * NOTE 2:  This project provides two demo applications.  A simple blinky style
 * project, and a more comprehensive test and demo application.  The
 * mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting in main.c is used to select
 * between the two.  See the notes on using mainCREATE_SIMPLE_BLINKY_DEMO_ONLY
 * in main.c.  This file implements the simply blinky version.  Console output
 * is used in place of the normal LED toggling.
 *
 * NOTE 3:  This file only contains the source code that is specific to the
 * basic demo.  Generic functions, such FreeRTOS hook functions, are defined
 * in main.c.
 ******************************************************************************
 *
 * main_blinky() creates one queue, one software timer, and two tasks.  It then
 * starts the scheduler.
 *
 * The Queue Send Task:
 * The queue send task is implemented by the prvQueueSendTask() function in
 * this file.  It uses vTaskDelayUntil() to create a periodic task that sends
 * the value 100 to the queue every 200 milliseconds (please read the notes
 * above regarding the accuracy of timing under Windows).
 *
 * The Queue Send Software Timer:
 * The timer is a one-shot timer that is reset by a key press.  The timer's
 * period is set to two seconds - if the timer expires then its callback
 * function writes the value 200 to the queue.  The callback function is
 * implemented by prvQueueSendTimerCallback() within this file.
 *
 * The Queue Receive Task:
 * The queue receive task is implemented by the prvQueueReceiveTask() function
 * in this file.  prvQueueReceiveTask() waits for data to arrive on the queue.
 * When data is received, the task checks the value of the data, then outputs a
 * message to indicate if the data came from the queue send task or the queue
 * send software timer.
 *
 * Expected Behaviour:
 * - The queue send task writes to the queue every 200ms, so every 200ms the
 *   queue receive task will output a message indicating that data was received
 *   on the queue from the queue send task.
 * - The queue send software timer has a period of two seconds, and is reset
 *   each time a key is pressed.  So if two seconds expire without a key being
 *   pressed then the queue receive task will output a message indicating that
 *   data was received on the queue from the queue send software timer.
 *
 * NOTE:  Console input and output relies on Windows system calls, which can
 * interfere with the execution of the FreeRTOS Windows port.  This demo only
 * uses Windows system call occasionally.  Heavier use of Windows system calls
 * can crash the port.
 */

/* Standard includes. */
#include <stdio.h>
#include <conio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Priorities at which the tasks are created. */
#define mainQUEUE_RECEIVE_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )
#define	mainQUEUE_SEND_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
#define mainTEST_TASK_PRIORITY				( tskIDLE_PRIORITY + 1 )


/* The rate at which data is sent to the queue.  The times are converted from
milliseconds to ticks using the pdMS_TO_TICKS() macro. */
#define mainTASK_SEND_FREQUENCY_MS			pdMS_TO_TICKS( 200UL )
#define mainTIMER_SEND_FREQUENCY_MS			pdMS_TO_TICKS( 2000UL )

/* The number of items the queue can hold at once. */
#define mainQUEUE_LENGTH					( 2 )

/* The values sent to the queue receive task from the queue send task and the
queue send software timer respectively. */
#define mainVALUE_SENT_FROM_TASK			( 100UL )
#define mainVALUE_SENT_FROM_TIMER			( 200UL )

/*-----------------------------------------------------------*/

/*
 * The tasks as described in the comments at the top of this file.
 */
static void prvQueueReceiveTask( void *pvParameters );
static void prvQueueSendTask( void *pvParameters );

/*
EDF TEST tasks

*/
static void TaskA( void *pvParameters );
static void TaskB( void *pvParameters );
static void TaskC(void *pvParameters);

/*
 * The callback function executed when the software timer expires.
 */
static void prvQueueSendTimerCallback( TimerHandle_t xTimerHandle );

/*-----------------------------------------------------------*/

/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

/* A software timer that is started from the tick hook. */
static TimerHandle_t xTimer = NULL;

/*-----------------------------------------------------------*/

/*EDF parameters*/
/*deadlines*/
#define A_DDL 10
#define B_DDL 10
#define C_DDL 1000

/* arriving periods */
#define mainTASK_A_FREQUENCY_MS			pdMS_TO_TICKS( 500UL )
#define mainTASK_B_FREQUENCY_MS			pdMS_TO_TICKS( 5000UL )
#define mainTASK_C_FREQUENCY_MS			pdMS_TO_TICKS( 500UL )


/*EDF datastruct for current state 
 *!!!!!warning!!!!!
 *must be declared
*/
char ulTaskname[20];
uint32_t ulTaskRunTimeLast=0;
unsigned long ulTaskNumber[configEXPECTED_EDF_TASKS];
uint32_t ulTaskBeginTime[configEXPECTED_EDF_TASKS];
uint32_t ulTaskRunTime[configEXPECTED_EDF_TASKS];
TickType_t ulTaskDDL;

/*** SEE THE COMMENTS AT THE TOP OF THIS FILE ***/
void main_hw2( void )
{

	if (1==1);
	{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskDeadlineCreate( TaskA,			/* The function that implements the task. */
					"TA", 							/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 		/* The size of the stack to allocate to the task. */
					NULL, 							/* The parameter passed to the task - not used in this simple case. */
					mainTEST_TASK_PRIORITY,/* The priority assigned to the task. */
					NULL ,							/* The task handle is not required, so NULL is passed. */
					A_DDL);

		xTaskDeadlineCreate( TaskB, "TB", configMINIMAL_STACK_SIZE, NULL, mainTEST_TASK_PRIORITY, NULL ,B_DDL);

		xTaskDeadlineCreate( TaskC, "TC", configMINIMAL_STACK_SIZE, NULL, mainTEST_TASK_PRIORITY, NULL, C_DDL);

		/* Start the tasks and timer running. */
		vTaskStartScheduler();
	}

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	for( ;; );
}
/*-----------------------------------------------------------*/
static void TaskA(void *pvParameters) {
	TickType_t xNextWakeTime;
	const TickType_t xBlockTime = mainTASK_A_FREQUENCY_MS;

	/* Prevent the compiler warning about the unused parameter. */
	(void)pvParameters;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();

	double workload = 2.0;

	for (;; )
	{
		/* Place this task in the blocked state until it is time to run again.
		The block time is specified in ticks, pdMS_TO_TICKS() was used to
		convert a time specified in milliseconds into a time specified in ticks.
		While in the Blocked state this task will not consume any CPU time. */
		vTaskDelayUntil(&xNextWakeTime, xBlockTime);

		
		for (int i = 0;i < 99999;i += 1) {
			workload += 1.0;
			workload /= i;
			workload += 2.0;
			workload /= i;
			workload += 3.0;
			workload /= i;
		}

		/*working block*/
		printf("current task status:%ld,%ld,%ld\r\n", ulTaskNumber[1], ulTaskNumber[2],ulTaskNumber[3]);
		//printf("running ticktimes:%u,%u,%u,%u\r\n", ulTaskRunTime[0], ulTaskRunTime[1], ulTaskRunTime[2], ulTaskRunTime[3]);
		printf("current running task:%6s\r\n", ulTaskname);
		printf("current task begintime:%u\r\n", ulTaskBeginTime[1]);
		printf("current task ddl:%u\r\n", ulTaskDDL);

	}
}

static void TaskB(void *pvParameters) {
	TickType_t xNextWakeTime;
	const TickType_t xBlockTime = mainTASK_B_FREQUENCY_MS;

	/* Prevent the compiler warning about the unused parameter. */
	(void)pvParameters;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();

	for (;; )
	{
		/* Place this task in the blocked state until it is time to run again.
		The block time is specified in ticks, pdMS_TO_TICKS() was used to
		convert a time specified in milliseconds into a time specified in ticks.
		While in the Blocked state this task will not consume any CPU time. */
		vTaskDelayUntil(&xNextWakeTime, xBlockTime);

		/*working block*/
		//printf("current task status:%ld,%ld\r\n", ulTaskNumber[1], ulTaskNumber[2]);
		//printf("running ticktimes:%u,%u,%u,%u\r\n", ulTaskRunTime[0], ulTaskRunTime[1], ulTaskRunTime[2], ulTaskRunTime[3] );
		printf("current running task:%6s\r\n", ulTaskname);
		printf("current task begintime:%u\r\n", ulTaskBeginTime[2]);
		printf("current task ddl:%u\r\n", ulTaskDDL);
	}
}


static void TaskC(void *pvParameters) {
	TickType_t xNextWakeTime;
	const TickType_t xBlockTime = mainTASK_A_FREQUENCY_MS;

	/* Prevent the compiler warning about the unused parameter. */
	(void)pvParameters;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();

	/* first time*/
	vTaskDelayUntil(&xNextWakeTime, 1000);


	for (;; )
	{
		/* Place this task in the blocked state until it is time to run again.
		The block time is specified in ticks, pdMS_TO_TICKS() was used to
		convert a time specified in milliseconds into a time specified in ticks.
		While in the Blocked state this task will not consume any CPU time. */
		vTaskDelayUntil(&xNextWakeTime, xBlockTime);

		/*working block*/
		//printf("current task status:%ld,%ld\r\n", ulTaskNumber[1], ulTaskNumber[2]);
		//printf("running ticktimes:%u,%u,%u,%u\r\n", ulTaskRunTime[0], ulTaskRunTime[1], ulTaskRunTime[2], ulTaskRunTime[3]);
		printf("current running task:%6s\r\n", ulTaskname);
		printf("current task begintime:%u\r\n", ulTaskBeginTime[3]);
		printf("current task ddl:%u\r\n", ulTaskDDL);
	}
}


/*-----------------------------------------------------------*/


