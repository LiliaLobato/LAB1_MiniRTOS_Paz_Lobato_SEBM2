/**
 * @file rtos.c
 * @author ITESO
 * @date Feb 2018
 * @brief Implementation of rtos API
 *
 * This is the implementation of the rtos module for the
 * embedded systems II course at ITESO
 */

#include "rtos.h"
#include "rtos_config.h"
#include "clock_config.h"

#ifdef RTOS_ENABLE_IS_ALIVE
#include "fsl_gpio.h"
#include "fsl_port.h"
#endif
/**********************************************************************************/
// Module defines
/**********************************************************************************/

#define FORCE_INLINE 	__attribute__((always_inline)) inline

#define STACK_FRAME_SIZE			8
#define STACK_LR_OFFSET				2
#define STACK_PSR_OFFSET			1
#define STACK_PSR_DEFAULT			0x01000000

/**********************************************************************************/
// IS ALIVE definitions
/**********************************************************************************/

#ifdef RTOS_ENABLE_IS_ALIVE
#define CAT_STRING(x,y)  		x##y
#define alive_GPIO(x)			CAT_STRING(GPIO,x)
#define alive_PORT(x)			CAT_STRING(PORT,x)
#define alive_CLOCK(x)			CAT_STRING(kCLOCK_Port,x)
static void init_is_alive(void);
static void refresh_is_alive(void);
#endif

/**********************************************************************************/
// Type definitions
/**********************************************************************************/

typedef enum
{
	S_READY = 0, S_RUNNING, S_WAITING, S_SUSPENDED
} task_state_e;
typedef enum
{
	kFromISR = 0, kFromNormalExec
} task_switch_type_e;

typedef struct
{
	uint8_t priority;
	task_state_e state;
	uint32_t *sp;
	void (*task_body)();
	rtos_tick_t local_tick;
	uint32_t reserved[10];
	uint32_t stack[RTOS_STACK_SIZE];
} rtos_tcb_t;

/**********************************************************************************/
// Global (static) task list
/**********************************************************************************/

struct
{
	uint8_t nTasks;
	rtos_task_handle_t current_task;
	rtos_task_handle_t next_task;
	rtos_tcb_t tasks[RTOS_MAX_NUMBER_OF_TASKS + 1];
	rtos_tick_t global_tick;
} task_list =
{ 0 };

/**********************************************************************************/
// Local methods prototypes
/**********************************************************************************/

static void reload_systick(void);
static void dispatcher(task_switch_type_e type);
static void activate_waiting_tasks();
FORCE_INLINE static void context_switch(task_switch_type_e type);
static void idle_task(void);

/**********************************************************************************/
// API implementation
/**********************************************************************************/

void rtos_start_scheduler(void)
{
#ifdef RTOS_ENABLE_IS_ALIVE
	init_is_alive();
#endif
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk
	        | SysTick_CTRL_ENABLE_Msk;
	reload_systick();

	task_list.global_tick=0;	//Poner el reloj global en 0
	//rtos_create_task(idle_task(),0,kAutoStart);	//Llamar rtos create task(idles task, 0, auto start)

	for (;;)
		;
}

rtos_task_handle_t rtos_create_task(void (*task_body)(), uint8_t priority,
		rtos_autostart_e autostart)
{

}

rtos_tick_t rtos_get_clock(void)
{
	return task_list.global_tick; //return valor del reloj del sistema
}

void rtos_delay(rtos_tick_t ticks)
{
	task_list.tasks[task_list.current_task].state=S_WAITING;	//Pone la tarea que llamó esta función en estado de espera
	task_list.tasks[task_list.current_task].local_tick++;	//Asigna ticks a el reloj local de la tarea
	dispatcher(kFromNormalExec);	//Llama dispatcher(desde la tarea)

}

void rtos_suspend_task(void)
{
	task_list.tasks[task_list.current_task].state=S_SUSPENDED;	//Pone la tarea que llamó esta función en estado suspendido
	dispatcher(kFromNormalExec);	//Llama dispatcher(desde la tarea)
}

void rtos_activate_task(rtos_task_handle_t task)
{
	task_list.tasks[task_list.current_task].state=S_READY;	//Pone la tarea que llamó esta función en estado listo
	dispatcher(kFromNormalExec);	//Llama dispatcher(desde la tarea)
}

/**********************************************************************************/
// Local methods implementation
/**********************************************************************************/

static void reload_systick(void)
{
	SysTick->LOAD = USEC_TO_COUNT(RTOS_TIC_PERIOD_IN_US,
	        CLOCK_GetCoreSysClkFreq());
	SysTick->VAL = 0;
}

static void dispatcher(task_switch_type_e type)
{
	//task_list.tasks[task_list.next_task].task_body = idle_task(); //siguiente tarea = tarea idle TODO
	uint8_t prioridad_mas_alta = 0;
	for(uint8_t i=0;i<task_list.nTasks;i++){//for cada tarea en lista de tareas do
		//if prioridad de tarea ¿prioridad mas alta y el estado de tarea es listo o corriendo then
		if(task_list.tasks[i].priority > prioridad_mas_alta & (task_list.tasks[i].state == S_READY | task_list.tasks[i].state == S_RUNNING)){
			prioridad_mas_alta = task_list.tasks[i].priority;	//prioridad_mas_alta = prioridad_de_tarea
			task_list.tasks[task_list.next_task] = task_list.tasks[i];//siguiente_tarea = tarea
		}//end if
	}//end for
	//if siguiente tarea diferente de tarea actual then
	if(task_list.next_task  != task_list.current_task){
		context_switch(kFromNormalExec);	//context switch (desde la tarea)
	}//end if
}

FORCE_INLINE static void context_switch(task_switch_type_e type)
{

}

static void activate_waiting_tasks()
{
	for(uint8_t i=0;i<task_list.nTasks;i++){	//for cada tarea en lista de tareas do
		//if tarea en estado de espera then
		if(task_list.tasks[i].state == S_WAITING ){
			task_list.tasks[i].local_tick--;	//Disminuye en 1 el reloj local de la tarea
			//if reloj local de la tarea en 0 then
			if(0 == task_list.tasks[i].local_tick){
				task_list.tasks[i].state = S_READY; //pone la tarea en estado ’listo’
			}//end if
		}//end if
	}//end for
}

/**********************************************************************************/
// IDLE TASK
/**********************************************************************************/

static void idle_task(void)
{
	for (;;)
	{

	}
}

/****************************************************/
// ISR implementation
/****************************************************/

void SysTick_Handler(void)
{
#ifdef RTOS_ENABLE_IS_ALIVE
	refresh_is_alive();
#endif
	task_list.global_tick++; //incrementa el reloj global en 1
	activate_waiting_tasks();	//activate waiting tasks()
	reload_systick();
	dispatcher(kFromISR);	//dispatcher(desde interrupción);
}

void PendSV_Handler(void)
{
	//Carga el stack pointer del procesador con el stack pointer de la tarea actual
}

/**********************************************************************************/
// IS ALIVE SIGNAL IMPLEMENTATION
/**********************************************************************************/

#ifdef RTOS_ENABLE_IS_ALIVE
static void init_is_alive(void)
{
	gpio_pin_config_t gpio_config =
	{ kGPIO_DigitalOutput, 1, };

	port_pin_config_t port_config =
	{ kPORT_PullDisable, kPORT_FastSlewRate, kPORT_PassiveFilterDisable,
	        kPORT_OpenDrainDisable, kPORT_LowDriveStrength, kPORT_MuxAsGpio,
	        kPORT_UnlockRegister, };
	CLOCK_EnableClock(alive_CLOCK(RTOS_IS_ALIVE_PORT));
	PORT_SetPinConfig(alive_PORT(RTOS_IS_ALIVE_PORT), RTOS_IS_ALIVE_PIN,
	        &port_config);
	GPIO_PinInit(alive_GPIO(RTOS_IS_ALIVE_PORT), RTOS_IS_ALIVE_PIN,
	        &gpio_config);
}

static void refresh_is_alive(void)
{
	static uint8_t state = 0;
	static uint32_t count = 0;
	SysTick->LOAD = USEC_TO_COUNT(RTOS_TIC_PERIOD_IN_US,
	        CLOCK_GetCoreSysClkFreq());
	SysTick->VAL = 0;
	if (RTOS_IS_ALIVE_PERIOD_IN_US / RTOS_TIC_PERIOD_IN_US - 1 == count)
	{
		GPIO_PinWrite(alive_GPIO(RTOS_IS_ALIVE_PORT), RTOS_IS_ALIVE_PIN,
		        state);
		state = state == 0 ? 1 : 0;
		count = 0;
	} else //
	{
		count++;
	}
}
#endif
///
