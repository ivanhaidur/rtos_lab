/*
 * Лабораторна робота №5. Direct to task notification, software timers у FreeRTOS.
 * Гайдур Іван (HAIV_), варіант 2: «Моніторинг псевдо-температури».
 *
 *   SensorTask  — генерує псевдо-температуру (rand()) і передає її в ControlTask
 *                 як value через xTaskNotify( ..., eSetValueWithOverwrite ).
 *   ControlTask — приймає рішення (OK / OVERHEAT) і через notification будить
 *                 DisplayTask зі статусом.
 *   DisplayTask — друкує стан у консоль.
 *
 *   Periodic timer (200 мс) — будить SensorTask через xTaskNotifyGive();
 *                             SensorTask чекає ulTaskNotifyTake() (notification
 *                             як лічильний семафор).
 *   One-shot timer (3 с)    — стартує при перегріві; якщо за 3 с температура не
 *                             нормалізувалась, callback шле в DisplayTask ALARM.
 *
 * Різниця eSetValueWithOverwrite / eSetValueWithoutOverwrite при частому
 * оновленні (див. build.bat):
 *   NOTIFY_OVERWRITE = 1 / 0 — режим xTaskNotify() у SensorTask
 *   CONTROL_WORK_MS          — час "обробки" в ControlTask (10 мс — встигає,
 *                              450 мс — не встигає за оновленнями кожні 200 мс)
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
#include "timers.h"

/* ------------------------------------------------------------------------ */
/* Налаштування                                                             */
/* ------------------------------------------------------------------------ */

#define IDX                 2

#ifndef NOTIFY_OVERWRITE
    #define NOTIFY_OVERWRITE    1       /* варіант: eSetValueWithOverwrite */
#endif
#ifndef CONTROL_WORK_MS
    #define CONTROL_WORK_MS     10
#endif

#define SAMPLE_PERIOD_MS    200         /* periodic timer */
#define ALARM_DELAY_MS      3000        /* one-shot timer */
#define N_SAMPLES           50          /* 50 * 200 мс = 10 с */
#define OVERHEAT_X10        300         /* поріг перегріву 30.0 °C */

#define SENSOR_PRIO         3
#define CONTROL_PRIO        2
#define DISPLAY_PRIO        4       /* вищий за Control: інакше Control може перезаписати
                                     * (eSetValueWithOverwrite) ALARM від таймера,
                                     * доки Display його не прочитав */

/* Формат 32-бітного value:
 *   біти 31..28 — тип (для DisplayTask: 0 = OK, 1 = OVERHEAT, 2 = ALARM)
 *   біти 27..16 — номер вимірювання (seq)
 *   біти 15..0  — температура * 10 (зі знаком)                                       */
#define MSG_OK          0u
#define MSG_OVERHEAT    1u
#define MSG_ALARM       2u

#define PACK( type, seq, t10 )  ( ( ( uint32_t ) ( type ) << 28 ) | ( ( ( uint32_t ) ( seq ) & 0xFFFu ) << 16 ) | ( ( uint32_t ) ( uint16_t ) ( t10 ) ) )
#define TYPE( v )               ( ( v ) >> 28 )
#define SEQ( v )                ( ( ( v ) >> 16 ) & 0xFFFu )
#define T10( v )                ( ( int16_t ) ( ( v ) & 0xFFFFu ) )

/* ------------------------------------------------------------------------ */
/* Дані                                                                     */
/* ------------------------------------------------------------------------ */

static TaskHandle_t  hSensor, hControl, hDisplay;
static TimerHandle_t tSample, tAlarm;

static volatile uint32_t lastSentSeq;       /* останнє вимірювання SensorTask */
static volatile int      overheated;        /* поточний стан, бачить callback таймера */

static struct
{
    uint32_t sent, notify_fail;             /* SensorTask                       */
    uint32_t received, lost, max_lag;       /* ControlTask                      */
    uint32_t episodes, normalized, alarms;  /* перегріви                        */
} st;

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
/* Псевдо-температура: сценарій + шум rand()                                */
/* ------------------------------------------------------------------------ */

/* Базовий профіль (°C * 10) за номером вимірювання n:
 *   0..4   норма 24.0
 *   5..9   нагрів до 31.5   -> короткий перегрів, що минає менше ніж за 3 с
 *   14..16 охолодження до 26.0
 *   20..24 нагрів до 33.0   -> тривалий перегрів, ALARM через 3 с
 *   42..45 охолодження до 25.0                                                 */
static int base_x10( uint32_t n )
{
    if( n < 5 )   return 240;
    if( n < 10 )  return 240 + ( int ) ( n - 4 ) * 15;          /* 255 .. 315 */
    if( n < 14 )  return 315;
    if( n < 17 )  return 315 - ( int ) ( n - 13 ) * 18;         /* 297 .. 261 */
    if( n < 20 )  return 260;
    if( n < 25 )  return 260 + ( int ) ( n - 19 ) * 14;         /* 274 .. 330 */
    if( n < 42 )  return 330;
    if( n < 46 )  return 330 - ( int ) ( n - 41 ) * 20;         /* 310 .. 250 */
    return 250;
}

static int read_temperature_x10( uint32_t n )
{
    return base_x10( n ) + ( rand() % 11 ) - 5;                 /* шум ±0.5 °C */
}

/* ------------------------------------------------------------------------ */
/* Таймери                                                                  */
/* ------------------------------------------------------------------------ */

/* Periodic, 200 мс: будить SensorTask (notification як лічильний семафор) */
static void vSampleTimerCb( TimerHandle_t t )
{
    ( void ) t;
    xTaskNotifyGive( hSensor );
}

/* One-shot, 3 с після початку перегріву: якщо температура не нормалізувалась */
static void vAlarmTimerCb( TimerHandle_t t )
{
    ( void ) t;

    if( overheated )
    {
        st.alarms++;
        xTaskNotify( hDisplay, PACK( MSG_ALARM, lastSentSeq, 0 ), eSetValueWithOverwrite );
    }
}

/* ------------------------------------------------------------------------ */
/* SensorTask                                                               */
/* ------------------------------------------------------------------------ */

static void vSensorTask( void * arg )
{
    ( void ) arg;

    for( uint32_t n = 0; n < N_SAMPLES; n++ )
    {
        /* Чекати "тік" periodic-таймера. pdTRUE — обнулити лічильник. */
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

        int      t10 = read_temperature_x10( n );
        uint32_t seq = n + 1;
        lastSentSeq = seq;

        #if NOTIFY_OVERWRITE
            xTaskNotify( hControl, PACK( 0, seq, t10 ), eSetValueWithOverwrite );   /* завжди pdPASS */
            st.sent++;
        #else
            if( xTaskNotify( hControl, PACK( 0, seq, t10 ), eSetValueWithoutOverwrite ) == pdPASS )
            {
                st.sent++;
            }
            else
            {
                st.notify_fail++;
                log_line( "[Sensor]  #%-2u %4.1f C -> xTaskNotify FAILED (previous value not read yet)",
                          ( unsigned ) seq, t10 / 10.0 );
            }
        #endif
    }

    /* Кінець експерименту: дати ControlTask/DisplayTask дообробити і підсумувати */
    xTimerStop( tSample, 0 );
    vTaskDelay( pdMS_TO_TICKS( 600 ) );

    taskENTER_CRITICAL();
    printf( "\n=========================== SUMMARY ===========================\n" );
    printf( "xTaskNotify mode: %s, ControlTask work: %d ms, sample period: %d ms\n",
            NOTIFY_OVERWRITE ? "eSetValueWithOverwrite" : "eSetValueWithoutOverwrite",
            CONTROL_WORK_MS, SAMPLE_PERIOD_MS );
    printf( "Sensor:  samples=%d  notify OK=%u  notify FAILED=%u\n",
            N_SAMPLES, ( unsigned ) st.sent, ( unsigned ) st.notify_fail );
    printf( "Control: received=%u  skipped samples=%u  max lag=%u samples\n",
            ( unsigned ) st.received, ( unsigned ) st.lost, ( unsigned ) st.max_lag );
    printf( "Overheat episodes=%u  normalized in time=%u  ALARMS=%u\n",
            ( unsigned ) st.episodes, ( unsigned ) st.normalized, ( unsigned ) st.alarms );
    printf( "===============================================================\n" );
    fflush( stdout );
    taskEXIT_CRITICAL();
    exit( 0 );
}

/* ------------------------------------------------------------------------ */
/* ControlTask                                                              */
/* ------------------------------------------------------------------------ */

static void vControlTask( void * arg )
{
    ( void ) arg;
    uint32_t v, prev_seq = 0;

    for( ;; )
    {
        /* Отримати value (температуру) і очистити його */
        xTaskNotifyWait( 0, 0xFFFFFFFFu, &v, portMAX_DELAY );

        uint32_t seq = SEQ( v );
        int      t10 = T10( v );
        uint32_t lag = lastSentSeq - seq;    /* наскільки отримане значення застаріле */

        st.received++;
        if( seq > prev_seq + 1 )
        {
            st.lost += seq - prev_seq - 1;
        }
        if( lag > st.max_lag )
        {
            st.max_lag = lag;
        }
        prev_seq = seq;

        if( !overheated && t10 >= OVERHEAT_X10 )
        {
            overheated = 1;
            st.episodes++;
            xTimerStart( tAlarm, 0 );         /* відлік 3 с до аварії */
            log_line( "[Control] OVERHEAT detected (%.1f C >= %.1f) -> one-shot alarm timer started (3 s)",
                      t10 / 10.0, OVERHEAT_X10 / 10.0 );
        }
        else if( overheated && t10 < OVERHEAT_X10 )
        {
            overheated = 0;
            if( xTimerIsTimerActive( tAlarm ) )
            {
                st.normalized++;
            }
            xTimerStop( tAlarm, 0 );
            log_line( "[Control] temperature normalized (%.1f C) -> alarm timer stopped", t10 / 10.0 );
        }

        uint32_t type = overheated ? MSG_OVERHEAT : MSG_OK;
        xTaskNotify( hDisplay, PACK( type, seq, t10 ), eSetValueWithOverwrite );

        vTaskDelay( pdMS_TO_TICKS( CONTROL_WORK_MS ) );   /* "обробка" */
    }
}

/* ------------------------------------------------------------------------ */
/* DisplayTask                                                              */
/* ------------------------------------------------------------------------ */

static void vDisplayTask( void * arg )
{
    ( void ) arg;
    uint32_t v;

    for( ;; )
    {
        xTaskNotifyWait( 0, 0xFFFFFFFFu, &v, portMAX_DELAY );

        switch( TYPE( v ) )
        {
            case MSG_OK:
                log_line( "[Display] #%-2u %5.1f C   OK       (lag %u)", ( unsigned ) SEQ( v ), T10( v ) / 10.0,
                          ( unsigned ) ( lastSentSeq - SEQ( v ) ) );
                break;

            case MSG_OVERHEAT:
                log_line( "[Display] #%-2u %5.1f C   OVERHEAT (lag %u)", ( unsigned ) SEQ( v ), T10( v ) / 10.0,
                          ( unsigned ) ( lastSentSeq - SEQ( v ) ) );
                break;

            case MSG_ALARM:
                log_line( "[Display] !!! ALARM: overheat lasts more than %d s !!!", ALARM_DELAY_MS / 1000 );
                break;

            default:
                break;
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

    srand( IDX );

    printf( "Lab 5 - task notifications and software timers, variant 2 (Haidur Ivan)\n" );
    printf( "Sensor -> Control: xTaskNotify(%s), Control work %d ms, sampling %d ms, alarm after %d ms\n\n",
            NOTIFY_OVERWRITE ? "eSetValueWithOverwrite" : "eSetValueWithoutOverwrite",
            CONTROL_WORK_MS, SAMPLE_PERIOD_MS, ALARM_DELAY_MS );
    fflush( stdout );

    xTaskCreate( vSensorTask,  "HAIV_Sensor",  1024, NULL, SENSOR_PRIO,  &hSensor );
    xTaskCreate( vControlTask, "HAIV_Control", 1024, NULL, CONTROL_PRIO, &hControl );
    xTaskCreate( vDisplayTask, "HAIV_Display", 1024, NULL, DISPLAY_PRIO, &hDisplay );

    tSample = xTimerCreate( "HAIV_Sample", pdMS_TO_TICKS( SAMPLE_PERIOD_MS ), pdTRUE,  NULL, vSampleTimerCb );
    tAlarm  = xTimerCreate( "HAIV_Alarm",  pdMS_TO_TICKS( ALARM_DELAY_MS ),   pdFALSE, NULL, vAlarmTimerCb );
    configASSERT( tSample && tAlarm );

    xTimerStart( tSample, 0 );
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
