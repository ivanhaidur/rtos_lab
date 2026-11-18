/*
 * Лабораторна робота №3. Черги (Queues) у FreeRTOS.
 * Гайдур Іван (HAIV_), варіант 2.
 *
 *   Задачі:          3 — Producer, Consumer, Monitor
 *   Тип у черзі:     struct { float temp; uint32_t id; }
 *   Тайм-аут send:   0 (неблокуюча відправка)
 *   Тайм-аут recv:   500 мс
 *   Monitor:         періодично читає uxQueueMessagesWaiting()
 *
 * Producer відправляє MSG_COUNT повідомлень кожні PRODUCER_PERIOD_MS, а потім
 * зупиняється. Consumer після кожного прийому "обробляє" повідомлення
 * CONSUMER_WORK_MS, тому він повільніший за Producer: черга поступово
 * заповнюється, і неблокуюча відправка починає повертати errQUEUE_FULL.
 * Коли Producer зупиняється, Consumer дочитує чергу, а далі отримує
 * тайм-аути прийому (500 мс). Через RUN_MS Monitor друкує підсумок і
 * завершує програму.
 *
 * Параметри експерименту (задаються з командного рядка, див. build.bat):
 *   QUEUE_LEN  — довжина черги (10 або 1)
 *   PROD_PRIO  — пріоритет Producer
 *   CONS_PRIO  — пріоритет Consumer
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
#include "queue.h"

/* ------------------------------------------------------------------------ */
/* Налаштування                                                             */
/* ------------------------------------------------------------------------ */

#ifndef QUEUE_LEN
    #define QUEUE_LEN           10
#endif
#ifndef PROD_PRIO
    #define PROD_PRIO           2       /* Producer > Consumer, як у прикладі */
#endif
#ifndef CONS_PRIO
    #define CONS_PRIO           1
#endif
#define MON_PRIO                3       /* Monitor найвищий, щоб опитувати чергу вчасно */

#define SEND_TIMEOUT            0                       /* варіант 2: non-blocking */
#define RECV_TIMEOUT            pdMS_TO_TICKS( 500 )    /* варіант 2: 500 мс       */

#define MSG_COUNT               40      /* скільки повідомлень формує Producer */
#define PRODUCER_PERIOD_MS      100
#define CONSUMER_WORK_MS        150
#define MONITOR_PERIOD_MS       500
#define RUN_MS                  7000

/* ------------------------------------------------------------------------ */
/* Дані                                                                     */
/* ------------------------------------------------------------------------ */

typedef struct
{
    float    temp;
    uint32_t id;
} sensor_msg_t;

typedef struct
{
    uint32_t sent;          /* успішно відправлено                       */
    uint32_t full;          /* відмов: черга повна (errQUEUE_FULL)       */
    uint32_t received;      /* прийнято Consumer                         */
    uint32_t timeouts;      /* тайм-аутів прийому                        */
    uint32_t lost_order;    /* пропуски в id (через відкинуті повідомлення) */
    float    temp_sum;      /* сума прийнятих температур                 */
    UBaseType_t max_waiting;/* максимум uxQueueMessagesWaiting() у Monitor */
} stats_t;

static QueueHandle_t qData;
static stats_t       xStats;

/* Друк одного рядка з тіком; критична секція не дає рядкам перемішатися. */
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

/* ------------------------------------------------------------------------ */
/* Producer                                                                 */
/* ------------------------------------------------------------------------ */

static void vProducer( void * arg )
{
    ( void ) arg;
    TickType_t xLastWake = xTaskGetTickCount();

    for( uint32_t id = 1; id <= MSG_COUNT; id++ )
    {
        /* "Датчик": температура коливається в межах 20.0 ... 24.5 °C */
        sensor_msg_t msg = { 20.0f + ( float ) ( ( id * 7 ) % 10 ) * 0.5f, id };

        if( xQueueSend( qData, &msg, SEND_TIMEOUT ) == pdPASS )
        {
            xStats.sent++;
            log_line( "[Producer] sent id=%-3u temp=%.1f", ( unsigned ) msg.id, msg.temp );
        }
        else
        {
            xStats.full++;
            log_line( "[Producer] queue full! id=%u dropped", ( unsigned ) msg.id );
        }

        vTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( PRODUCER_PERIOD_MS ) );
    }

    log_line( "[Producer] done: %u sent, %u dropped", ( unsigned ) xStats.sent, ( unsigned ) xStats.full );
    vTaskDelete( NULL );
}

/* ------------------------------------------------------------------------ */
/* Consumer                                                                 */
/* ------------------------------------------------------------------------ */

static void vConsumer( void * arg )
{
    ( void ) arg;
    sensor_msg_t msg;
    uint32_t     last_id = 0;

    for( ;; )
    {
        if( xQueueReceive( qData, &msg, RECV_TIMEOUT ) == pdPASS )
        {
            xStats.received++;
            xStats.temp_sum += msg.temp;

            if( msg.id != last_id + 1 )
            {
                xStats.lost_order += msg.id - last_id - 1;
            }

            last_id = msg.id;
            log_line( "[Consumer] got  id=%-3u temp=%.1f", ( unsigned ) msg.id, msg.temp );

            vTaskDelay( pdMS_TO_TICKS( CONSUMER_WORK_MS ) );   /* "обробка" */
        }
        else
        {
            xStats.timeouts++;
            log_line( "[Consumer] timeout waiting for data (500 ms)" );
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Monitor                                                                  */
/* ------------------------------------------------------------------------ */

static void vMonitor( void * arg )
{
    ( void ) arg;
    TickType_t xLastWake = xTaskGetTickCount();

    while( xTaskGetTickCount() < pdMS_TO_TICKS( RUN_MS ) )
    {
        vTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( MONITOR_PERIOD_MS ) );

        UBaseType_t waiting = uxQueueMessagesWaiting( qData );
        UBaseType_t free    = uxQueueSpacesAvailable( qData );

        if( waiting > xStats.max_waiting )
        {
            xStats.max_waiting = waiting;
        }

        log_line( "[Monitor]  queue: waiting=%u free=%u %s", ( unsigned ) waiting, ( unsigned ) free,
                  ( free == 0 ) ? "<- FULL" : "" );
    }

    taskENTER_CRITICAL();
    printf( "\n=========================== SUMMARY ===========================\n" );
    printf( "QUEUE_LEN=%d  Producer prio=%d  Consumer prio=%d  Monitor prio=%d\n",
            QUEUE_LEN, PROD_PRIO, CONS_PRIO, MON_PRIO );
    printf( "send timeout=0 (non-blocking), receive timeout=500 ms, run=%d ms\n", RUN_MS );
    printf( "Producer: generated=%d  sent=%u  dropped(full)=%u\n",
            MSG_COUNT, ( unsigned ) xStats.sent, ( unsigned ) xStats.full );
    printf( "Consumer: received=%u  timeouts=%u  avg temp=%.2f\n",
            ( unsigned ) xStats.received, ( unsigned ) xStats.timeouts,
            xStats.received ? xStats.temp_sum / ( float ) xStats.received : 0.0f );
    printf( "Monitor:  max waiting=%u of %d\n", ( unsigned ) xStats.max_waiting, QUEUE_LEN );
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

    printf( "Lab 3 - FreeRTOS queues, variant 2: item = struct { float temp; uint32_t id; } (%u bytes)\n",
            ( unsigned ) sizeof( sensor_msg_t ) );
    printf( "QUEUE_LEN=%d, Producer prio=%d, Consumer prio=%d, Monitor prio=%d\n\n",
            QUEUE_LEN, PROD_PRIO, CONS_PRIO, MON_PRIO );
    fflush( stdout );

    qData = xQueueCreate( QUEUE_LEN, sizeof( sensor_msg_t ) );
    configASSERT( qData );

    xTaskCreate( vProducer, "HAIV_Producer", 1024, NULL, PROD_PRIO, NULL );
    xTaskCreate( vConsumer, "HAIV_Consumer", 1024, NULL, CONS_PRIO, NULL );
    xTaskCreate( vMonitor,  "HAIV_Monitor",  1024, NULL, MON_PRIO,  NULL );

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
