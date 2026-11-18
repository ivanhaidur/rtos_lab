/*
 * Лабораторна робота №2. FreeRTOS Thread Local Storage Pointers (TLS).
 * Гайдур Іван (HAIV_), варіант IDX = 2.
 *
 *   Ntasks = 5, Mode = delay, Burst B = 2, TLS index V1/V2 = 0/1, Base = 110 ms
 *   Ni     = 10 * Ntasks + IDX = 52 ітерації на задачу
 *   seed   = IDX = 2
 *   delay  = vTaskDelay( pdMS_TO_TICKS( 110 + 7 * i ) ), i — номер задачі
 *
 * Кожна задача тримає свій стан (task_context_t) і профіль (profile_t) у
 * TLS-слотах V1/V2. Модуль журналу (burst_log / burst_flush) і модуль
 * контрольної суми не мають глобальних змінних: дані поточної задачі вони
 * беруть з її TLS через xTaskGetThreadLocalStoragePointer( NULL, ... ).
 */

#ifdef _WIN32
    #include <windows.h>   /* SetConsoleOutputCP: UTF-8 у консолі Windows */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

/* ------------------------------------------------------------------------ */
/* Варіант                                                                  */
/* ------------------------------------------------------------------------ */

#define IDX             2
#define NTASKS          5
#define MODE            MODE_DELAY
#define BURST_B         2           /* скільки рядків накопичувати до друку (1..10) */
#define TLS_V1          0           /* слот для task_context_t */
#define TLS_V2          1           /* слот для profile_t      */
#define BASE_DELAY_MS   110
#define BASE_CYCLES     0           /* для режиму busy (у цьому варіанті не використовується) */
#define STEP_DELAY_MS   7
#define STEP_CYCLES     800

#define NI              ( 10 * NTASKS + IDX )   /* 52 */
#define BURST_N         5

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS <= TLS_V1 ) || ( configNUM_THREAD_LOCAL_STORAGE_POINTERS <= TLS_V2 )
    #error "configNUM_THREAD_LOCAL_STORAGE_POINTERS замалий для TLS_V1 / TLS_V2"
#endif

/* ------------------------------------------------------------------------ */
/* Структури даних                                                          */
/* ------------------------------------------------------------------------ */

typedef struct
{
    char     name[ 12 ];              /* ім'я задачі ("T02_0" тощо)                 */
    uint32_t iter;                    /* номер ітерації (0..Ni)                     */
    uint32_t checksum;                /* CRC8 SAE J1850 від iter і seed             */
    uint32_t delayTicksOrCycles;      /* затримка (тіки) або тривалість навантаження */
    uint32_t seed;                    /* початкове "зерно" = IDX                    */
    char     line1[ BURST_N ][ 64 ];  /* буфер рядків для burst-логів (рядки 0..4) */
    char     line2[ BURST_N ][ 64 ];  /*                               (рядки 5..9) */
    uint8_t  bcnt;                    /* скільки рядків накопичено                  */
} task_context_t;

typedef enum { MODE_DELAY = 0, MODE_BUSY = 1 } run_mode_t;

typedef struct
{
    run_mode_t mode;        /* delay або busy                              */
    uint8_t    burstMode;   /* 0 — звичайний лог; 1 — burst                */
    uint8_t    burstSize;   /* B: скільки рядків друкувати одним пакетом   */
    uint32_t   baseDelayMs; /* базова затримка для delay                   */
    uint32_t   baseCycles;  /* базові цикли для busy                       */
    uint16_t   step;        /* крок між задачами (7 мс або 800 циклів)     */
} profile_t;

/* Доступ до TLS поточної задачі */
static task_context_t * ctx_get( void )
{
    return ( task_context_t * ) pvTaskGetThreadLocalStoragePointer( NULL, TLS_V1 );
}

static profile_t * prof_get( void )
{
    return ( profile_t * ) pvTaskGetThreadLocalStoragePointer( NULL, TLS_V2 );
}

/* ------------------------------------------------------------------------ */
/* Модуль контрольної суми: CRC-8/SAE-J1850                                 */
/* poly 0x1D, init 0xFF, без відзеркалення, xorout 0xFF                     */
/* ------------------------------------------------------------------------ */

static uint8_t crc8_sae_j1850( const uint8_t * data, size_t len )
{
    uint8_t crc = 0xFF;

    for( size_t n = 0; n < len; n++ )
    {
        crc ^= data[ n ];

        for( int b = 0; b < 8; b++ )
        {
            crc = ( crc & 0x80 ) ? ( uint8_t ) ( ( crc << 1 ) ^ 0x1D ) : ( uint8_t ) ( crc << 1 );
        }
    }

    return crc ^ 0xFF;
}

/* Оновлює checksum поточної задачі: CRC по байтах iter і seed (little-endian). */
static void checksum_update( void )
{
    task_context_t * ctx = ctx_get();
    uint8_t buf[ 8 ];

    for( int k = 0; k < 4; k++ )
    {
        buf[ k ]     = ( uint8_t ) ( ctx->iter >> ( 8 * k ) );
        buf[ 4 + k ] = ( uint8_t ) ( ctx->seed >> ( 8 * k ) );
    }

    ctx->checksum = crc8_sae_j1850( buf, sizeof( buf ) );
}

/* ------------------------------------------------------------------------ */
/* Модуль журналу (burst)                                                   */
/* ------------------------------------------------------------------------ */

static char * burst_slot( task_context_t * ctx, uint8_t k )
{
    return ( k < BURST_N ) ? ctx->line1[ k ] : ctx->line2[ k - BURST_N ];
}

/* Друкує всі накопичені рядки одним пакетом. Критична секція гарантує, що
 * пакет не переплететься з виводом інших задач. */
static void burst_flush( void )
{
    task_context_t * ctx = ctx_get();

    if( ctx->bcnt == 0 )
    {
        return;
    }

    taskENTER_CRITICAL();
    {
        for( uint8_t k = 0; k < ctx->bcnt; k++ )
        {
            printf( "%s\n", burst_slot( ctx, k ) );
        }

        fflush( stdout );
    }
    taskEXIT_CRITICAL();

    ctx->bcnt = 0;
}

/* Додає рядок про поточну ітерацію; у режимі burst друкує, коли накопичено B рядків. */
static void burst_log( void )
{
    task_context_t * ctx  = ctx_get();
    profile_t *      prof = prof_get();
    char             line[ 64 ];

    snprintf( line, sizeof( line ), "[Task %s] Tick:%u Iter:%u Sum:0x%08X",
              ctx->name, ( unsigned ) xTaskGetTickCount(),
              ( unsigned ) ctx->iter, ( unsigned ) ctx->checksum );

    if( prof->burstMode == 0 )
    {
        taskENTER_CRITICAL();
        printf( "%s\n", line );
        fflush( stdout );
        taskEXIT_CRITICAL();
        return;
    }

    memcpy( burst_slot( ctx, ctx->bcnt ), line, sizeof( line ) );
    ctx->bcnt++;

    if( ctx->bcnt >= prof->burstSize )
    {
        burst_flush();
    }
}

/* ------------------------------------------------------------------------ */
/* Навантаження                                                             */
/* ------------------------------------------------------------------------ */

static void busy_loop( uint32_t cycles )
{
    for( volatile uint32_t n = 0; n < cycles; n++ )
    {
    }
}

static void do_wait( void )
{
    task_context_t * ctx  = ctx_get();
    profile_t *      prof = prof_get();

    if( prof->mode == MODE_DELAY )
    {
        vTaskDelay( ( TickType_t ) ctx->delayTicksOrCycles );
    }
    else
    {
        busy_loop( ctx->delayTicksOrCycles );
    }
}

/* ------------------------------------------------------------------------ */
/* Задача                                                                   */
/* ------------------------------------------------------------------------ */

static void vWorker( void * arg )
{
    const int i = ( int ) ( uintptr_t ) arg;

    /* 1. Створити TaskContext і Profile та покласти їх у TLS */
    task_context_t * ctx  = pvPortMalloc( sizeof( task_context_t ) );
    profile_t *      prof = pvPortMalloc( sizeof( profile_t ) );
    configASSERT( ctx != NULL && prof != NULL );

    memset( ctx, 0, sizeof( *ctx ) );
    snprintf( ctx->name, sizeof( ctx->name ), "T%02d_%d", IDX, i );
    ctx->seed = IDX;

    prof->mode        = MODE;
    prof->burstMode   = ( BURST_B > 1 ) ? 1 : 0;
    prof->burstSize   = BURST_B;
    prof->baseDelayMs = BASE_DELAY_MS;
    prof->baseCycles  = BASE_CYCLES;
    prof->step        = ( MODE == MODE_DELAY ) ? STEP_DELAY_MS : STEP_CYCLES;

    ctx->delayTicksOrCycles = ( prof->mode == MODE_DELAY )
                              ? pdMS_TO_TICKS( prof->baseDelayMs + prof->step * i )
                              : prof->baseCycles + prof->step * i;

    vTaskSetThreadLocalStoragePointer( NULL, TLS_V1, ctx );
    vTaskSetThreadLocalStoragePointer( NULL, TLS_V2, prof );

    taskENTER_CRITICAL();
    printf( "[Task %s] start: TLS[%d]=ctx %p, TLS[%d]=prof %p, delay=%u ms, Ni=%d, B=%d\n",
            ctx->name, TLS_V1, ( void * ) ctx, TLS_V2, ( void * ) prof,
            ( unsigned ) ctx->delayTicksOrCycles, NI, BURST_B );
    fflush( stdout );
    taskEXIT_CRITICAL();

    /* 2. Цикл роботи: Ni ітерацій. Далі задача працює лише через TLS. */
    for( int n = 0; n < NI; n++ )
    {
        ctx_get()->iter++;
        checksum_update();
        burst_log();
        do_wait();
    }

    /* 3. Завершення */
    burst_flush();

    taskENTER_CRITICAL();
    printf( "Task finished %s (#%d)\n", ctx_get()->name, i );
    fflush( stdout );
    taskEXIT_CRITICAL();

    vTaskSetThreadLocalStoragePointer( NULL, TLS_V1, NULL );
    vTaskSetThreadLocalStoragePointer( NULL, TLS_V2, NULL );
    vPortFree( ctx );
    vPortFree( prof );

    vTaskDelete( NULL );
}

/* ------------------------------------------------------------------------ */
/* Спостерігач: чекає, доки всі робочі задачі завершаться, і закриває програму */
/* ------------------------------------------------------------------------ */

static void vMonitor( void * arg )
{
    ( void ) arg;
    const UBaseType_t uxStart   = uxTaskGetNumberOfTasks();   /* робочі + монітор + idle */
    const size_t      heapStart = xPortGetFreeHeapSize();

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 200 ) );

        /* Видалені задачі зникають із лічильника, коли idle звільнить їхню пам'ять. */
        if( uxTaskGetNumberOfTasks() <= uxStart - NTASKS )
        {
            taskENTER_CRITICAL();
            printf( "\nAll %d tasks finished at tick %u. Free heap: %u -> %u bytes\n",
                    NTASKS, ( unsigned ) xTaskGetTickCount(),
                    ( unsigned ) heapStart, ( unsigned ) xPortGetFreeHeapSize() );
            fflush( stdout );
            taskEXIT_CRITICAL();
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

    printf( "Lab 2 - FreeRTOS TLS, IDX = %d: Ntasks=%d, mode=%s, B=%d, V1/V2=%d/%d, base=%d ms, Ni=%d\n\n",
            IDX, NTASKS, ( MODE == MODE_DELAY ) ? "delay" : "busy", BURST_B,
            TLS_V1, TLS_V2, BASE_DELAY_MS, NI );
    fflush( stdout );

    for( int i = 0; i < NTASKS; ++i )
    {
        char name[ configMAX_TASK_NAME_LEN ];
        snprintf( name, sizeof( name ), "HAIV_T%02d_%d", IDX, i );
        BaseType_t r = xTaskCreate( vWorker, name, 1024, ( void * ) ( uintptr_t ) i, 1, NULL );
        configASSERT( r == pdPASS );
    }

    xTaskCreate( vMonitor, "HAIV_MON", configMINIMAL_STACK_SIZE * 2, NULL, 1, NULL );

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
