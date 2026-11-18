/*
 * FreeRTOSConfig.h — Лабораторна робота №1 (FreeRTOS, симулятор WIN32-MingW / POSIX)
 *
 * Змінено відносно демо: такт 1 мс, увімкнено tick hook (облік часу виконання
 * задач), configUSE_TIME_SLICING можна перемикати з командного рядка:
 *     make TIME_SLICING=0
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ---- Планувальник ------------------------------------------------------ */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )   /* 1 tick = 1 мс */
#define configMAX_PRIORITIES                    ( 7 )
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 256 )
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1

/* Режим квантування часу між задачами однакового пріоритету.
 * Значення за замовчуванням 1; перевизначається через -DconfigUSE_TIME_SLICING=0 */
#ifndef configUSE_TIME_SLICING
    #define configUSE_TIME_SLICING              1
#endif

/* ---- Пам'ять ----------------------------------------------------------- */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 256 * 1024 ) )

/* ---- Хуки -------------------------------------------------------------- */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     1   /* потрібен для обліку Ci */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          0

/* ---- Інші можливості --------------------------------------------------- */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_TRACE_FACILITY                1
#define configUSE_CO_ROUTINES                   0
#define configUSE_TIMERS                        0
#define configGENERATE_RUN_TIME_STATS           0

/* ---- Доступні функції API ---------------------------------------------- */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_xTaskGetSchedulerState          1

/* ---- Асерти ------------------------------------------------------------ */
void vAssertCalled( const char * pcFile, unsigned long ulLine );
#define configASSERT( x )    if( ( x ) == 0 ) vAssertCalled( __FILE__, __LINE__ )

#endif /* FREERTOS_CONFIG_H */
