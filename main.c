/*
 * Лабораторна робота №6. Stream & message buffers, event groups у FreeRTOS.
 * Гайдур Іван (HAIV_), варіант 2: «Сенсорні дані + команди конфігурації».
 *
 *   SensorTask     — кожні 50 мс генерує значення uint16 і пише його (2 байти)
 *                    у stream buffer. З 5.0 до 6.5 с імітується обрив лінії.
 *   FilterTask     — читає зі stream buffer блоками по N значень (trigger level),
 *                    рахує середнє/мін/макс і кількість значень вище порогу.
 *   ConfigTask     — отримує текстові команди ("SET_THRESHOLD 50") через
 *                    message buffer і змінює параметри FilterTask.
 *   OperatorTask   — емулює оператора: за сценарієм надсилає команди в message buffer.
 *   SupervisorTask — чекає на біти event group і друкує стан системи.
 *
 * Event group:
 *   SENSOR_OK     — потік даних іде стабільно (3 повні блоки поспіль)
 *   CONFIG_OK     — конфігурацію успішно застосовано
 *   ERROR_TIMEOUT — FilterTask довго (500 мс) не бачить даних
 *   CONFIG_ERROR  — команду відхилено (додатковий біт)
 */

#ifdef _WIN32
    #include <windows.h>   /* SetConsoleOutputCP: UTF-8 у консолі Windows */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "message_buffer.h"
#include "event_groups.h"

/* ------------------------------------------------------------------------ */
/* Налаштування                                                             */
/* ------------------------------------------------------------------------ */

#define IDX                 2
#define SAMPLE_PERIOD_MS    50
#define STREAM_SIZE         128         /* байт у stream buffer (64 значення) */
#define MSG_BUF_SIZE        128         /* байт у message buffer */
#define FILTER_TIMEOUT_MS   500
#define STABLE_BLOCKS       3           /* скільки повних блоків поспіль = SENSOR_OK */
#define LINK_LOST_FROM_MS   5000
#define LINK_LOST_TO_MS     6500
#define RUN_MS              10000

#define BIT_SENSOR_OK       ( 1u << 0 )
#define BIT_CONFIG_OK       ( 1u << 1 )
#define BIT_ERROR_TIMEOUT   ( 1u << 2 )
#define BIT_CONFIG_ERROR    ( 1u << 3 )

/* ------------------------------------------------------------------------ */
/* Дані                                                                     */
/* ------------------------------------------------------------------------ */

static StreamBufferHandle_t  xSensorStream;
static MessageBufferHandle_t xCmdBuffer;
static EventGroupHandle_t    xEvents;

/* Параметри FilterTask, які змінює ConfigTask */
static struct
{
    uint16_t threshold;
    uint16_t block;             /* значень в одному блоці */
} xFilterCfg = { 55, 10 };

static struct
{
    uint32_t samples, bytes, overflow;
    uint32_t blocks, partial, timeouts;
    uint32_t cmd_ok, cmd_err;
} st;

static void log_line( const char * fmt, ... )
{
    char    buf[ 140 ];
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( buf, sizeof( buf ), fmt, ap );
    va_end( ap );

    taskENTER_CRITICAL();
    printf( "%5u  %s\n", ( unsigned ) xTaskGetTickCount(), buf );
    fflush( stdout );
    taskEXIT_CRITICAL();
}

/* ------------------------------------------------------------------------ */
/* SensorTask: потік сирих значень uint16 -> stream buffer                   */
/* ------------------------------------------------------------------------ */

static uint16_t sensor_value( uint32_t n )
{
    uint32_t phase = n % 40;                                   /* трикутна хвиля 30..60 */
    int      tri   = ( phase < 20 ) ? ( int ) phase : ( int ) ( 40 - phase );
    return ( uint16_t ) ( 30 + tri * 3 / 2 + ( rand() % 7 ) - 3 );   /* + шум ±3 */
}

static void vSensorTask( void * arg )
{
    ( void ) arg;
    TickType_t xLastWake = xTaskGetTickCount();
    int        link_up   = 1;

    for( uint32_t n = 0; ; n++ )
    {
        vTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( SAMPLE_PERIOD_MS ) );

        TickType_t now  = xTaskGetTickCount();
        int        lost = ( now >= pdMS_TO_TICKS( LINK_LOST_FROM_MS ) ) && ( now < pdMS_TO_TICKS( LINK_LOST_TO_MS ) );

        if( lost != !link_up )
        {
            link_up = !lost;
            log_line( "[Sensor]     %s", link_up ? "link restored, stream resumes" : "link LOST (simulated), no data" );
        }

        if( lost )
        {
            continue;
        }

        uint16_t v    = sensor_value( n );
        size_t   sent = xStreamBufferSend( xSensorStream, &v, sizeof( v ), 0 );

        st.samples++;
        st.bytes += ( uint32_t ) sent;
        if( sent < sizeof( v ) )
        {
            st.overflow++;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* FilterTask: читає потік блоками                                           */
/* ------------------------------------------------------------------------ */

static void vFilterTask( void * arg )
{
    ( void ) arg;
    uint16_t buf[ 32 ];
    int      good_blocks = 0, in_timeout = 0;
    uint16_t cur_block   = 0;

    for( ;; )
    {
        uint16_t thr, blk;
        taskENTER_CRITICAL();
        thr = xFilterCfg.threshold;
        blk = xFilterCfg.block;
        taskEXIT_CRITICAL();

        if( blk != cur_block )
        {
            /* Розблокуватися лише коли в потоці є цілий блок */
            xStreamBufferSetTriggerLevel( xSensorStream, blk * sizeof( uint16_t ) );
            cur_block = blk;
        }

        size_t got = xStreamBufferReceive( xSensorStream, buf, blk * sizeof( uint16_t ),
                                           pdMS_TO_TICKS( FILTER_TIMEOUT_MS ) );
        size_t n = got / sizeof( uint16_t );

        if( n == 0 )
        {
            st.timeouts++;
            good_blocks = 0;
            if( !in_timeout )
            {
                in_timeout = 1;
                log_line( "[Filter]     no data for %d ms -> set ERROR_TIMEOUT, clear SENSOR_OK", FILTER_TIMEOUT_MS );
                xEventGroupClearBits( xEvents, BIT_SENSOR_OK );
                xEventGroupSetBits( xEvents, BIT_ERROR_TIMEOUT );
            }
            continue;
        }

        uint32_t sum = 0;
        uint16_t mn = 0xFFFF, mx = 0, over = 0;
        for( size_t i = 0; i < n; i++ )
        {
            sum += buf[ i ];
            if( buf[ i ] < mn ) mn = buf[ i ];
            if( buf[ i ] > mx ) mx = buf[ i ];
            if( buf[ i ] > thr ) over++;
        }

        st.blocks++;
        if( n < blk )
        {
            st.partial++;
        }

        log_line( "[Filter]     block %2u x %-2u: avg=%5.1f min=%2u max=%2u  over %u: %u%s",
                  ( unsigned ) st.blocks, ( unsigned ) n, ( double ) sum / ( double ) n, mn, mx, thr, over,
                  ( n < blk ) ? "  (partial)" : "" );

        if( n == blk )
        {
            good_blocks++;
        }
        else
        {
            good_blocks = 0;
        }

        if( good_blocks == STABLE_BLOCKS )
        {
            in_timeout = 0;
            log_line( "[Filter]     %d full blocks in a row -> set SENSOR_OK", STABLE_BLOCKS );
            xEventGroupSetBits( xEvents, BIT_SENSOR_OK );
        }
    }
}

/* ------------------------------------------------------------------------ */
/* ConfigTask: текстові команди з message buffer                             */
/* ------------------------------------------------------------------------ */

static void vConfigTask( void * arg )
{
    ( void ) arg;
    char cmd[ 64 ];

    for( ;; )
    {
        size_t len = xMessageBufferReceive( xCmdBuffer, cmd, sizeof( cmd ) - 1, portMAX_DELAY );
        cmd[ len ] = '\0';

        int  value;
        char extra;
        int  ok = 0;

        if( ( sscanf( cmd, "SET_THRESHOLD %d %c", &value, &extra ) == 1 ) && ( value >= 0 ) && ( value <= 100 ) )
        {
            taskENTER_CRITICAL();
            xFilterCfg.threshold = ( uint16_t ) value;
            taskEXIT_CRITICAL();
            ok = 1;
        }
        else if( ( sscanf( cmd, "SET_BLOCK %d %c", &value, &extra ) == 1 ) && ( value >= 2 ) && ( value <= 20 ) )
        {
            taskENTER_CRITICAL();
            xFilterCfg.block = ( uint16_t ) value;
            taskEXIT_CRITICAL();
            ok = 1;
        }

        if( ok )
        {
            st.cmd_ok++;
            log_line( "[Config]     \"%s\" (%u bytes) -> applied, set CONFIG_OK", cmd, ( unsigned ) len );
            xEventGroupSetBits( xEvents, BIT_CONFIG_OK );
        }
        else
        {
            st.cmd_err++;
            log_line( "[Config]     \"%s\" (%u bytes) -> REJECTED, set CONFIG_ERROR", cmd, ( unsigned ) len );
            xEventGroupSetBits( xEvents, BIT_CONFIG_ERROR );
        }
    }
}

/* ------------------------------------------------------------------------ */
/* OperatorTask: сценарій команд                                             */
/* ------------------------------------------------------------------------ */

typedef struct
{
    uint32_t     at_ms;
    const char * text;
} op_cmd_t;

static const op_cmd_t xScript[] =
{
    { 1500, "SET_THRESHOLD 50" },
    { 3000, "SET_BLOCK 5" },
    { 4000, "SET_THRESHOLD abc" },     /* некоректна команда */
    { 7500, "SET_THRESHOLD 35" },
    { 8500, "SET_BLOCK 99" },          /* значення поза межами */
};

static void vOperatorTask( void * arg )
{
    ( void ) arg;

    for( size_t i = 0; i < sizeof( xScript ) / sizeof( xScript[ 0 ] ); i++ )
    {
        TickType_t now = xTaskGetTickCount();
        if( pdMS_TO_TICKS( xScript[ i ].at_ms ) > now )
        {
            vTaskDelay( pdMS_TO_TICKS( xScript[ i ].at_ms ) - now );
        }

        size_t len = strlen( xScript[ i ].text );
        size_t put = xMessageBufferSend( xCmdBuffer, xScript[ i ].text, len, pdMS_TO_TICKS( 100 ) );
        log_line( "[Operator]   send \"%s\" -> message buffer (%s)", xScript[ i ].text, put == len ? "ok" : "FULL" );
    }

    vTaskDelete( NULL );
}

/* ------------------------------------------------------------------------ */
/* SupervisorTask: event group                                               */
/* ------------------------------------------------------------------------ */

static void print_status( const char * prefix )
{
    EventBits_t b = xEventGroupGetBits( xEvents );
    log_line( "[Supervisor] %s SENSOR_OK=%d  ERROR_TIMEOUT=%d  threshold=%u  block=%u  stream=%u bytes",
              prefix, ( b & BIT_SENSOR_OK ) ? 1 : 0, ( b & BIT_ERROR_TIMEOUT ) ? 1 : 0,
              xFilterCfg.threshold, xFilterCfg.block, ( unsigned ) xStreamBufferBytesAvailable( xSensorStream ) );
}

static void vSupervisorTask( void * arg )
{
    ( void ) arg;

    /* 1) Чекати, доки ОДНОЧАСНО є стабільний потік і застосована конфігурація */
    EventBits_t b = xEventGroupWaitBits( xEvents, BIT_SENSOR_OK | BIT_CONFIG_OK,
                                         pdFALSE, pdTRUE, pdMS_TO_TICKS( 5000 ) );
    if( ( b & ( BIT_SENSOR_OK | BIT_CONFIG_OK ) ) == ( BIT_SENSOR_OK | BIT_CONFIG_OK ) )
    {
        log_line( "[Supervisor] *** SYSTEM READY: SENSOR_OK & CONFIG_OK (wait for ALL bits) ***" );
    }
    else
    {
        log_line( "[Supervisor] system NOT ready in 5 s" );
    }

    /* 2) Далі реагувати на будь-яку з подій (wait for ANY, clear on exit) */
    while( xTaskGetTickCount() < pdMS_TO_TICKS( RUN_MS ) )
    {
        b = xEventGroupWaitBits( xEvents, BIT_CONFIG_OK | BIT_CONFIG_ERROR | BIT_ERROR_TIMEOUT,
                                 pdTRUE, pdFALSE, pdMS_TO_TICKS( 1000 ) );

        if( b & BIT_CONFIG_OK )
        {
            print_status( "CONFIG_OK    ->" );
        }
        if( b & BIT_CONFIG_ERROR )
        {
            log_line( "[Supervisor] CONFIG_ERROR -> command rejected, configuration unchanged" );
        }
        if( b & BIT_ERROR_TIMEOUT )
        {
            TickType_t t0 = xTaskGetTickCount();
            log_line( "[Supervisor] ERROR_TIMEOUT -> sensor stream lost, waiting for SENSOR_OK..." );
            xEventGroupWaitBits( xEvents, BIT_SENSOR_OK, pdFALSE, pdTRUE, portMAX_DELAY );
            log_line( "[Supervisor] SENSOR_OK again -> stream restored, outage handled in %u ms",
                      ( unsigned ) ( xTaskGetTickCount() - t0 ) );
        }
        if( ( b & ( BIT_CONFIG_OK | BIT_CONFIG_ERROR | BIT_ERROR_TIMEOUT ) ) == 0 )
        {
            print_status( "status:" );
        }
    }

    taskENTER_CRITICAL();
    printf( "\n=========================== SUMMARY ===========================\n" );
    printf( "Sensor:  samples=%u  bytes to stream=%u  stream overflows=%u\n",
            ( unsigned ) st.samples, ( unsigned ) st.bytes, ( unsigned ) st.overflow );
    printf( "Filter:  blocks=%u  partial=%u  timeouts (500 ms)=%u\n",
            ( unsigned ) st.blocks, ( unsigned ) st.partial, ( unsigned ) st.timeouts );
    printf( "Config:  commands applied=%u  rejected=%u  final threshold=%u block=%u\n",
            ( unsigned ) st.cmd_ok, ( unsigned ) st.cmd_err, xFilterCfg.threshold, xFilterCfg.block );
    printf( "===============================================================\n" );
    fflush( stdout );
    taskEXIT_CRITICAL();
    exit( 0 );
}

/* ------------------------------------------------------------------------ */
/* main                                                                     */
/* ------------------------------------------------------------------------ */

int main( void )
{
    #ifdef _WIN32
        SetConsoleOutputCP( CP_UTF8 );
    #endif

    srand( IDX );

    xSensorStream = xStreamBufferCreate( STREAM_SIZE, xFilterCfg.block * sizeof( uint16_t ) );
    xCmdBuffer    = xMessageBufferCreate( MSG_BUF_SIZE );
    xEvents       = xEventGroupCreate();
    configASSERT( xSensorStream && xCmdBuffer && xEvents );

    printf( "Lab 6 - stream/message buffers and event groups, variant 2 (Haidur Ivan)\n" );
    printf( "Sensor every %d ms -> stream buffer (%d B); commands -> message buffer (%d B); link lost %d..%d ms\n\n",
            SAMPLE_PERIOD_MS, STREAM_SIZE, MSG_BUF_SIZE, LINK_LOST_FROM_MS, LINK_LOST_TO_MS );
    fflush( stdout );

    xTaskCreate( vSupervisorTask, "HAIV_Super",    1024, NULL, 4, NULL );
    xTaskCreate( vConfigTask,     "HAIV_Config",   1024, NULL, 3, NULL );
    xTaskCreate( vSensorTask,     "HAIV_Sensor",   1024, NULL, 3, NULL );
    xTaskCreate( vFilterTask,     "HAIV_Filter",   1024, NULL, 2, NULL );
    xTaskCreate( vOperatorTask,   "HAIV_Operator", 1024, NULL, 1, NULL );

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
