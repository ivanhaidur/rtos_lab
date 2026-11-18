/*
 * Лабораторна робота №4. Counting Semaphore, Binary Semaphore, Mutex у FreeRTOS.
 * Гайдур Іван (HAIV_), варіант 2: «Два сенсори – один обробник».
 *
 * Один проєкт, п'ять демонстрацій (макрос DEMO, див. build.bat):
 *
 *   DEMO 1  Binary Semaphore, варіант 2: SensorA (500 мс) і SensorB (900 мс)
 *           дають сигнал в один бінарний семафор, Handler (найвищий пріоритет)
 *           ловить усі події й показує, від кого сигнал (через ID).
 *   DEMO 2  Те саме, але Handler має нижчий пріоритет за сенсори: коли обидва
 *           сенсори спрацьовують в один тік, другий xSemaphoreGive() повертає
 *           pdFAIL — бінарний семафор не рахує події. Handler усе одно не
 *           втрачає подій, бо джерело записане у лічильник pending[] кожного сенсора.
 *   DEMO 3  Counting Semaphore: пул із 2 ресурсів на 4 задачі.
 *   DEMO 4  Спільна консоль БЕЗ м'ютекса: повідомлення з 3 частин перемішуються.
 *   DEMO 5  Спільна консоль З м'ютексом: кожне повідомлення друкується блоком.
 */

#ifdef _WIN32
    #include <windows.h>   /* SetConsoleOutputCP: UTF-8 у консолі Windows */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#ifndef DEMO
    #define DEMO    1
#endif

/* ------------------------------------------------------------------------ */
/* Друк одного рядка з тіком. Критична секція робить атомарним лише ОДИН     */
/* рядок; цілісність повідомлення з кількох рядків забезпечує м'ютекс (DEMO 5) */
/* ------------------------------------------------------------------------ */

static void log_line( const char * fmt, ... )
{
    char    buf[ 128 ];
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );

    taskENTER_CRITICAL();
    printf( "%5u  %s\n", ( unsigned ) xTaskGetTickCount(), buf );
    fflush( stdout );
    taskEXIT_CRITICAL();
}

static void finish( const char * fmt, ... )
{
    char    buf[ 512 ];
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );

    taskENTER_CRITICAL();
    printf( "\n=========================== SUMMARY ===========================\n%s"
            "===============================================================\n", buf );
    fflush( stdout );
    taskEXIT_CRITICAL();
    exit( 0 );
}

/* ======================================================================== */
/* DEMO 1, 2 — Binary Semaphore: два сенсори, один обробник (варіант 2)     */
/* ======================================================================== */
#if ( DEMO == 1 ) || ( DEMO == 2 )

#define SENSOR_A_ID         0
#define SENSOR_B_ID         1
#define SENSOR_A_PERIOD_MS  500
#define SENSOR_B_PERIOD_MS  900
#define BIN_RUN_MS          9100        /* 0..9100 мс: збіги періодів на 4500 і 9000 мс */

#define SENSOR_PRIO         2
#if ( DEMO == 1 )
    #define HANDLER_PRIO    3           /* Handler вищий за сенсори — реагує миттєво */
#else
    #define HANDLER_PRIO    1           /* Handler нижчий — сенсори встигають дати сигнал двічі */
#endif

typedef struct
{
    const char * name;
    uint8_t      id;
    uint32_t     period_ms;
} sensor_cfg_t;

static const sensor_cfg_t xSensors[ 2 ] =
{
    { "SensorA", SENSOR_A_ID, SENSOR_A_PERIOD_MS },
    { "SensorB", SENSOR_B_ID, SENSOR_B_PERIOD_MS },
};

static SemaphoreHandle_t xEventSem;
static volatile uint32_t pending[ 2 ];      /* необроблені події від кожного сенсора */
static uint32_t          given[ 2 ], give_fail[ 2 ], handled[ 2 ], takes, multi_takes;

static void vSensor( void * arg )
{
    const sensor_cfg_t * s = ( const sensor_cfg_t * ) arg;
    TickType_t xLastWake = 0;
    uint32_t   n = 0;

    for( ;; )
    {
        vTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( s->period_ms ) );
        n++;

        /* 1) записати, ВІД КОГО подія (ID сенсора); 2) дати сигнал */
        taskENTER_CRITICAL();
        pending[ s->id ]++;
        taskEXIT_CRITICAL();

        log_line( "[%s] signal #%u -> Give", s->name, ( unsigned ) n );

        if( xSemaphoreGive( xEventSem ) == pdPASS )
        {
            given[ s->id ]++;
        }
        else
        {
            give_fail[ s->id ]++;
            log_line( "[%s] Give FAILED: semaphore already given (Handler has not taken it yet)", s->name );
        }
    }
}

static void vHandler( void * arg )
{
    ( void ) arg;

    for( ;; )
    {
        xSemaphoreTake( xEventSem, portMAX_DELAY );
        takes++;

        /* Забрати всі накопичені події: семафор міг "склеїти" кілька сигналів */
        uint32_t ev[ 2 ];
        taskENTER_CRITICAL();
        ev[ 0 ] = pending[ 0 ]; pending[ 0 ] = 0;
        ev[ 1 ] = pending[ 1 ]; pending[ 1 ] = 0;
        taskEXIT_CRITICAL();

        if( ev[ 0 ] + ev[ 1 ] > 1 )
        {
            multi_takes++;
        }

        for( int id = 0; id < 2; id++ )
        {
            for( uint32_t k = 0; k < ev[ id ]; k++ )
            {
                handled[ id ]++;
                log_line( "    [Handler] EVENT from %s (ID=%d)%s", xSensors[ id ].name, id,
                          ( ev[ 0 ] + ev[ 1 ] > 1 ) ? "  <- 2 events in one Take" : "" );
            }
        }
    }
}

static void vStopper( void * arg )
{
    ( void ) arg;
    vTaskDelay( pdMS_TO_TICKS( BIN_RUN_MS ) );

    finish( "DEMO %d: Binary Semaphore, SensorA %d ms, SensorB %d ms, Handler prio=%d, sensors prio=%d\n"
            "SensorA: signals=%u  Give OK=%u  Give FAILED=%u  handled=%u\n"
            "SensorB: signals=%u  Give OK=%u  Give FAILED=%u  handled=%u\n"
            "Handler: Take=%u  of them with 2 events=%u  lost events=%u\n",
            DEMO, SENSOR_A_PERIOD_MS, SENSOR_B_PERIOD_MS, HANDLER_PRIO, SENSOR_PRIO,
            ( unsigned ) ( given[ 0 ] + give_fail[ 0 ] ), ( unsigned ) given[ 0 ], ( unsigned ) give_fail[ 0 ], ( unsigned ) handled[ 0 ],
            ( unsigned ) ( given[ 1 ] + give_fail[ 1 ] ), ( unsigned ) given[ 1 ], ( unsigned ) give_fail[ 1 ], ( unsigned ) handled[ 1 ],
            ( unsigned ) takes, ( unsigned ) multi_takes,
            ( unsigned ) ( given[ 0 ] + give_fail[ 0 ] + given[ 1 ] + give_fail[ 1 ] - handled[ 0 ] - handled[ 1 ] ) );
}

static void demo_start( void )
{
    xEventSem = xSemaphoreCreateBinary();
    configASSERT( xEventSem );

    xTaskCreate( vHandler, "HAIV_Handler", 1024, NULL, HANDLER_PRIO, NULL );
    xTaskCreate( vSensor,  "HAIV_SensorA", 1024, ( void * ) &xSensors[ 0 ], SENSOR_PRIO, NULL );
    xTaskCreate( vSensor,  "HAIV_SensorB", 1024, ( void * ) &xSensors[ 1 ], SENSOR_PRIO, NULL );
    xTaskCreate( vStopper, "HAIV_Stop",    1024, NULL, 4, NULL );

    printf( "DEMO %d - Binary Semaphore (variant 2): SensorA %d ms + SensorB %d ms -> one Handler\n"
            "Handler prio=%d, sensors prio=%d, run %d ms\n\n",
            DEMO, SENSOR_A_PERIOD_MS, SENSOR_B_PERIOD_MS, HANDLER_PRIO, SENSOR_PRIO, BIN_RUN_MS );
}

/* ======================================================================== */
/* DEMO 3 — Counting Semaphore: пул ресурсів                                */
/* ======================================================================== */
#elif ( DEMO == 3 )

#define POOL_SIZE       2
#define N_WORKERS       4
#define ROUNDS          2

static SemaphoreHandle_t xPool;
static volatile int      finished;

static void vWorker( void * arg )
{
    const int  i       = ( int ) ( uintptr_t ) arg;
    const char name[ 2 ] = { ( char ) ( 'A' + i ), 0 };
    const uint32_t work_ms = 800 + 200 * i;

    vTaskDelay( pdMS_TO_TICKS( 10 * i ) );   /* задачі приходять по черзі */

    for( int r = 1; r <= ROUNDS; r++ )
    {
        if( uxSemaphoreGetCount( xPool ) == 0 )
        {
            log_line( "[Worker%s] waiting... (all %d resources busy)", name, POOL_SIZE );
        }

        TickType_t t0 = xTaskGetTickCount();
        xSemaphoreTake( xPool, portMAX_DELAY );
        log_line( "[Worker%s] TAKE  resource (waited %u ms, free now %u)", name,
                  ( unsigned ) ( xTaskGetTickCount() - t0 ), ( unsigned ) uxSemaphoreGetCount( xPool ) );

        vTaskDelay( pdMS_TO_TICKS( work_ms ) );    /* користується ресурсом */

        xSemaphoreGive( xPool );
        log_line( "[Worker%s] GIVE  resource after %u ms (free now %u)", name,
                  ( unsigned ) work_ms, ( unsigned ) uxSemaphoreGetCount( xPool ) );

        vTaskDelay( pdMS_TO_TICKS( 100 ) );
    }

    taskENTER_CRITICAL();
    finished++;
    taskEXIT_CRITICAL();

    if( finished == N_WORKERS )
    {
        finish( "DEMO 3: Counting Semaphore, pool=%d, workers=%d, rounds=%d\n"
                "All workers done at tick %u, free resources=%u of %d\n",
                POOL_SIZE, N_WORKERS, ROUNDS, ( unsigned ) xTaskGetTickCount(),
                ( unsigned ) uxSemaphoreGetCount( xPool ), POOL_SIZE );
    }

    vTaskDelete( NULL );
}

static void demo_start( void )
{
    xPool = xSemaphoreCreateCounting( POOL_SIZE, POOL_SIZE );
    configASSERT( xPool );

    for( int i = 0; i < N_WORKERS; i++ )
    {
        char name[ 16 ];
        snprintf( name, sizeof( name ), "HAIV_Worker%c", 'A' + i );
        xTaskCreate( vWorker, name, 1024, ( void * ) ( uintptr_t ) i, 2, NULL );
    }

    printf( "DEMO 3 - Counting Semaphore: %d workers share a pool of %d resources, %d rounds each\n\n",
            N_WORKERS, POOL_SIZE, ROUNDS );
}

/* ======================================================================== */
/* DEMO 4, 5 — Mutex: спільна консоль                                       */
/* ======================================================================== */
#elif ( DEMO == 4 ) || ( DEMO == 5 )

#define USE_MUTEX       ( DEMO == 5 )
#define N_USERS         3
#define MESSAGES        3
#define PARTS           3

#if USE_MUTEX
    static SemaphoreHandle_t xConsoleMutex;
#endif
static volatile int      finished;

static void vUser( void * arg )
{
    const int i = ( int ) ( uintptr_t ) arg;

    for( int m = 1; m <= MESSAGES; m++ )
    {
        #if USE_MUTEX
            xSemaphoreTake( xConsoleMutex, portMAX_DELAY );
        #endif

        /* Повідомлення з кількох рядків; між рядками задача віддає процесор
         * (як при повільному виводі в UART/консоль). */
        for( int p = 1; p <= PARTS; p++ )
        {
            log_line( "[User%d] message %d, line %d/%d", i + 1, m, p, PARTS );
            vTaskDelay( pdMS_TO_TICKS( 20 ) );
        }

        #if USE_MUTEX
            xSemaphoreGive( xConsoleMutex );
        #endif

        vTaskDelay( pdMS_TO_TICKS( 50 + 30 * i ) );
    }

    taskENTER_CRITICAL();
    finished++;
    taskEXIT_CRITICAL();

    if( finished == N_USERS )
    {
        finish( "DEMO %d: shared console %s mutex, %d users x %d messages x %d lines\n",
                DEMO, USE_MUTEX ? "WITH" : "WITHOUT", N_USERS, MESSAGES, PARTS );
    }

    vTaskDelete( NULL );
}

static void demo_start( void )
{
    #if USE_MUTEX
        xConsoleMutex = xSemaphoreCreateMutex();
        configASSERT( xConsoleMutex );
    #endif

    for( int i = 0; i < N_USERS; i++ )
    {
        char name[ 16 ];
        snprintf( name, sizeof( name ), "HAIV_User%d", i + 1 );
        xTaskCreate( vUser, name, 1024, ( void * ) ( uintptr_t ) i, 2, NULL );
    }

    printf( "DEMO %d - Mutex, shared console %s mutex: %d users print %d-line messages\n\n",
            DEMO, USE_MUTEX ? "WITH" : "WITHOUT", N_USERS, PARTS );
}

#else
    #error "DEMO must be 1..5"
#endif

/* ------------------------------------------------------------------------ */
/* main                                                                     */
/* ------------------------------------------------------------------------ */

int main( void )
{
    #ifdef _WIN32
        SetConsoleOutputCP( CP_UTF8 );
    #endif

    printf( "Lab 4 - FreeRTOS semaphores and mutex, variant 2 (Haidur Ivan)\n" );
    demo_start();
    fflush( stdout );

    vTaskStartScheduler();

    for( ;; ) { }
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
