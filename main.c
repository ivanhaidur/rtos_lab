/*
 * Лабораторна робота №1. Вступ у FreeRTOS. Базові функції.
 *
 * Варіант: ID = 2
 *   T1 = 13 + (2 mod 7)       = 15 мс     C1 = 2 + (2 mod 3)       = 4 мс
 *   T2 = 21 + (2 mod 5)       = 23 мс     C2 = 3 + ((2/3) mod 3)   = 3 мс
 *   T3 = 34 + (2 mod 9)       = 36 мс     C3 = 4 + ((2/5) mod 3)   = 4 мс
 *   ID mod 2 == 0  ->  пріоритети τ1 > τ2 > τ3
 *   U = 4/15 + 3/23 + 4/36 ≈ 0.508
 *
 * Три періодичні задачі з негармонічними періодами. Кожна задача у циклі:
 *   1) фіксує тік старту,
 *   2) виконує "роботу" тривалістю Ci (busy-loop, що рахує ВЛАСНІ тіки виконання),
 *   3) фіксує тік фінішу та передає запис у чергу журналу,
 *   4) блокується до наступного періоду через xTaskDelayUntil().
 *
 * Друк виконує лише окрема задача-логер найнижчого пріоритету, тому виклики
 * printf() не спотворюють часову поведінку задач (і не блокують CRT у WIN32-порті).
 *
 * Режими (задаються в Makefile):
 *   LAB_EQUAL_PRIORITIES = 0  — пріоритети за варіантом (τ1 > τ2 > τ3)
 *   LAB_EQUAL_PRIORITIES = 1  — усі три задачі мають однаковий пріоритет
 *                               (для порівняння configUSE_TIME_SLICING = 1 / 0)
 */

#ifdef _WIN32
    #include <windows.h>   /* SetConsoleOutputCP: UTF-8 у консолі Windows */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* ------------------------------------------------------------------------ */
/* Налаштування                                                             */
/* ------------------------------------------------------------------------ */

/* Префікс імен задач: 2 перші літери прізвища + 2 перші літери імені. */
#ifndef LAB_PREFIX
    #define LAB_PREFIX              "HAIV_"
#endif

#define STUDENT_ID                  2

#define T1_MS   ( 13 + ( STUDENT_ID % 7 ) )
#define T2_MS   ( 21 + ( STUDENT_ID % 5 ) )
#define T3_MS   ( 34 + ( STUDENT_ID % 9 ) )
#define C1_MS   ( 2 + ( STUDENT_ID % 3 ) )
#define C2_MS   ( 3 + ( ( STUDENT_ID / 3 ) % 3 ) )
#define C3_MS   ( 4 + ( ( STUDENT_ID / 5 ) % 3 ) )

#ifndef LAB_EQUAL_PRIORITIES
    #define LAB_EQUAL_PRIORITIES    0
#endif

/* Тривалість експерименту: одна гіперперіодa НСК(15, 23, 36) = 4140 мс. */
#ifndef LAB_RUN_MS
    #define LAB_RUN_MS              4140
#endif

/* 1 — друкувати кожну роботу (job), 0 — лише підсумкову таблицю. */
#ifndef LAB_VERBOSE
    #define LAB_VERBOSE             1
#endif

#define NUM_TASKS                   3
#define LOG_QUEUE_LEN               256
#define LOGGER_PRIORITY             ( tskIDLE_PRIORITY )

/* ------------------------------------------------------------------------ */
/* Типи                                                                     */
/* ------------------------------------------------------------------------ */

typedef struct
{
    const char *         name;
    TickType_t           period;     /* Ti, тіки */
    TickType_t           wcet;       /* Ci, тіки */
    UBaseType_t          priority;
    TaskHandle_t         handle;
    volatile uint32_t    execTicks;  /* тіки, протягом яких задача реально виконувалась */

    /* статистика (оновлює лише сама задача) */
    uint32_t             jobs;
    uint32_t             misses;
    uint32_t             startMin, startMax, startSum;   /* затримка старту = start - release */
    uint32_t             respMax, respSum;               /* час відгуку  = finish - release  */
} LabTask_t;

typedef struct
{
    uint8_t     task;
    uint32_t    job;
    TickType_t  release;
    TickType_t  start;
    TickType_t  finish;
} LogRecord_t;

/* ------------------------------------------------------------------------ */
/* Дані                                                                     */
/* ------------------------------------------------------------------------ */

#if ( LAB_EQUAL_PRIORITIES == 1 )
    #define P1  ( tskIDLE_PRIORITY + 2 )
    #define P2  ( tskIDLE_PRIORITY + 2 )
    #define P3  ( tskIDLE_PRIORITY + 2 )
#else
    #define P1  ( tskIDLE_PRIORITY + 3 )   /* τ1 — найвищий */
    #define P2  ( tskIDLE_PRIORITY + 2 )
    #define P3  ( tskIDLE_PRIORITY + 1 )   /* τ3 — найнижчий */
#endif

static LabTask_t xTasks[ NUM_TASKS ] =
{
    { LAB_PREFIX "T1", pdMS_TO_TICKS( T1_MS ), pdMS_TO_TICKS( C1_MS ), P1 },
    { LAB_PREFIX "T2", pdMS_TO_TICKS( T2_MS ), pdMS_TO_TICKS( C2_MS ), P2 },
    { LAB_PREFIX "T3", pdMS_TO_TICKS( T3_MS ), pdMS_TO_TICKS( C3_MS ), P3 },
};

static QueueHandle_t       xLogQueue;
static volatile uint32_t   ulLogDropped = 0;

/* ------------------------------------------------------------------------ */
/* Tick hook: облік часу виконання                                          */
/* ------------------------------------------------------------------------ */

/* Викликається на кожен тік. Тік зараховується тій задачі, яка виконувалась у
 * момент переривання, — так busy-loop імітує саме Ci мс процесорного часу, а не
 * Ci мс "настінного" часу (витіснення не скорочує роботу). */
void vApplicationTickHook( void )
{
    TaskHandle_t xCur = xTaskGetCurrentTaskHandle();

    for( int i = 0; i < NUM_TASKS; i++ )
    {
        if( xTasks[ i ].handle == xCur )
        {
            xTasks[ i ].execTicks++;
            break;
        }
    }
}

/* Імітація роботи тривалістю C тіків власного виконання. */
static void prvDoWork( LabTask_t * t )
{
    const uint32_t ulTarget = t->execTicks + ( uint32_t ) t->wcet;

    while( t->execTicks < ulTarget )
    {
        /* busy-loop */
    }
}

/* ------------------------------------------------------------------------ */
/* Періодична задача                                                        */
/* ------------------------------------------------------------------------ */

static void vPeriodicTask( void * pvParameters )
{
    const uint8_t idx = ( uint8_t ) ( uintptr_t ) pvParameters;
    LabTask_t *   t   = &xTasks[ idx ];

    TickType_t xLastWake = 0;   /* усі задачі звільняються одночасно в t = 0 */

    t->startMin = UINT32_MAX;

    for( ;; )
    {
        const TickType_t xRelease = xLastWake;
        const TickType_t xStart   = xTaskGetTickCount();

        prvDoWork( t );

        const TickType_t xFinish = xTaskGetTickCount();

        if( xRelease < pdMS_TO_TICKS( LAB_RUN_MS ) )
        {
            const uint32_t ulStartDelay = ( uint32_t ) ( xStart - xRelease );
            const uint32_t ulResponse   = ( uint32_t ) ( xFinish - xRelease );

            t->jobs++;
            t->startSum += ulStartDelay;
            t->respSum  += ulResponse;
            if( ulStartDelay < t->startMin ) t->startMin = ulStartDelay;
            if( ulStartDelay > t->startMax ) t->startMax = ulStartDelay;
            if( ulResponse   > t->respMax  ) t->respMax  = ulResponse;
            if( ulResponse   > t->period   ) t->misses++;

            LogRecord_t rec = { idx, t->jobs, xRelease, xStart, xFinish };
            if( xQueueSend( xLogQueue, &rec, 0 ) != pdPASS )
            {
                ulLogDropped++;
            }
        }

        /* Наступний період. Якщо робота запізнилась на цілий період,
         * xTaskDelayUntil повертає pdFALSE без блокування. */
        ( void ) xTaskDelayUntil( &xLastWake, t->period );
    }
}

/* ------------------------------------------------------------------------ */
/* Логер (найнижчий пріоритет) — єдина задача, що викликає printf           */
/* ------------------------------------------------------------------------ */

static void prvPrintSummary( void )
{
    printf( "\n================================ SUMMARY =================================\n" );
    printf( "configUSE_TIME_SLICING = %d, equal priorities = %d, duration = %d ms\n",
            configUSE_TIME_SLICING, LAB_EQUAL_PRIORITIES, LAB_RUN_MS );
    printf( "%-10s %4s %4s %4s %5s | %5s %5s %6s %7s | %6s %6s %6s\n",
            "Task", "T", "C", "Pr", "Jobs",
            "Smin", "Smax", "Savg", "Jitter",
            "Rmax", "Ravg", "Miss" );

    for( int i = 0; i < NUM_TASKS; i++ )
    {
        LabTask_t * t = &xTasks[ i ];
        double savg = t->jobs ? ( double ) t->startSum / t->jobs : 0.0;
        double ravg = t->jobs ? ( double ) t->respSum  / t->jobs : 0.0;

        printf( "%-10s %4u %4u %4u %5u | %5u %5u %6.2f %7u | %6u %6.2f %6u\n",
                t->name,
                ( unsigned ) t->period, ( unsigned ) t->wcet, ( unsigned ) t->priority,
                ( unsigned ) t->jobs,
                ( unsigned ) t->startMin, ( unsigned ) t->startMax, savg,
                ( unsigned ) ( t->startMax - t->startMin ),
                ( unsigned ) t->respMax, ravg, ( unsigned ) t->misses );
    }

    printf( "S = start - release (start delay), Jitter = Smax - Smin,\n"
            "R = finish - release (response time). Units: ticks (1 tick = 1 ms).\n" );
    printf( "Dropped log records: %u\n", ( unsigned ) ulLogDropped );
    printf( "==========================================================================\n" );
    fflush( stdout );
}

static void vLoggerTask( void * pvParameters )
{
    ( void ) pvParameters;
    LogRecord_t rec;

    for( ;; )
    {
        if( xQueueReceive( xLogQueue, &rec, pdMS_TO_TICKS( 50 ) ) == pdPASS )
        {
            #if ( LAB_VERBOSE == 1 )
                printf( "%-9s job=%-4u release=%-5u start=%-5u finish=%-5u  S=%-2u R=%-2u\n",
                        xTasks[ rec.task ].name, ( unsigned ) rec.job,
                        ( unsigned ) rec.release, ( unsigned ) rec.start,
                        ( unsigned ) rec.finish,
                        ( unsigned ) ( rec.start - rec.release ),
                        ( unsigned ) ( rec.finish - rec.release ) );
            #endif
        }
        else if( xTaskGetTickCount() > pdMS_TO_TICKS( LAB_RUN_MS + 100 ) )
        {
            /* Експеримент завершено і черга порожня. */
            prvPrintSummary();
            exit( 0 );
        }
    }
}

/* ------------------------------------------------------------------------ */
/* main                                                                     */
/* ------------------------------------------------------------------------ */

int main( void )
{
    #ifdef _WIN32
        SetConsoleOutputCP( CP_UTF8 );
    #endif

    printf( "Lab 1 - FreeRTOS, ID = %d\n", STUDENT_ID );
    printf( "configUSE_TIME_SLICING = %d, LAB_EQUAL_PRIORITIES = %d\n",
            configUSE_TIME_SLICING, LAB_EQUAL_PRIORITIES );
    for( int i = 0; i < NUM_TASKS; i++ )
    {
        printf( "  %s: T = %u ms, C = %u ms, priority = %u\n", xTasks[ i ].name,
                ( unsigned ) xTasks[ i ].period, ( unsigned ) xTasks[ i ].wcet,
                ( unsigned ) xTasks[ i ].priority );
    }
    printf( "\n" );
    fflush( stdout );

    xLogQueue = xQueueCreate( LOG_QUEUE_LEN, sizeof( LogRecord_t ) );
    configASSERT( xLogQueue != NULL );

    for( int i = 0; i < NUM_TASKS; i++ )
    {
        BaseType_t r = xTaskCreate( vPeriodicTask, xTasks[ i ].name,
                                    configMINIMAL_STACK_SIZE * 2,
                                    ( void * ) ( uintptr_t ) i,
                                    xTasks[ i ].priority, &xTasks[ i ].handle );
        configASSERT( r == pdPASS );
    }

    xTaskCreate( vLoggerTask, LAB_PREFIX "LOG", configMINIMAL_STACK_SIZE * 4,
                 NULL, LOGGER_PRIORITY, NULL );

    vTaskStartScheduler();

    for( ;; ) { }   /* сюди не доходимо */
}

/* ------------------------------------------------------------------------ */
/* Обов'язкові хуки                                                         */
/* ------------------------------------------------------------------------ */

void vApplicationMallocFailedHook( void )
{
    printf( "malloc failed\n" );
    exit( 1 );
}

void vAssertCalled( const char * pcFile, unsigned long ulLine )
{
    printf( "ASSERT: %s:%lu\n", pcFile, ulLine );
    fflush( stdout );
    abort();
}
