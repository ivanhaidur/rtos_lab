# Лабораторна робота №6 — Stream & message buffers, event groups у FreeRTOS

Демо «Сенсорні дані + команди конфігурації». Датчик передає потік сирих значень через stream buffer, оператор надсилає текстові команди конфігурації через message buffer, а стан системи (стабільний потік, застосована конфігурація, обрив даних) координує event group. Проєкт працює на симуляторі FreeRTOS для Windows (MSVC або MinGW) і Linux (POSIX).

Автор: Гайдур Іван, варіант 2.

## Варіант 2

| Задача | Пріоритет | Що робить |
|--------|-----------|-----------|
| SensorTask | 3 | Кожні 50 мс генерує значення `uint16_t` (трикутна хвиля 30…60 + шум `rand()`) і пише 2 байти в stream buffer. З 5,0 до 6,5 с імітується обрив лінії |
| FilterTask | 2 | Читає потік блоками по N значень (`xStreamBufferReceive` з trigger level = N · 2 байти), рахує середнє, мінімум, максимум і кількість значень вище порогу |
| ConfigTask | 3 | Отримує команди з message buffer (`xMessageBufferReceive`) і змінює параметри FilterTask |
| OperatorTask | 1 | Емулює оператора: за сценарієм надсилає команди в message buffer |
| SupervisorTask | 4 | Чекає на біти event group і друкує стан системи |

### Stream buffer — потік даних датчика

- Розмір 128 байт (64 значення), один записувач і один читач.
- FilterTask встановлює trigger level, рівний розміру блоку (`xStreamBufferSetTriggerLevel`), тож прокидається лише тоді, коли в буфері є цілий блок.
- Якщо за 500 мс даних немає, `xStreamBufferReceive` повертає 0, і FilterTask виставляє `ERROR_TIMEOUT`.

### Message buffer — команди конфігурації

Кожна команда — окреме повідомлення змінної довжини (рядок без `\0`). Підтримуються:

| Команда | Діапазон | Що змінює |
|---------|----------|-----------|
| `SET_THRESHOLD n` | 0…100 | поріг, вище якого FilterTask рахує значення |
| `SET_BLOCK n` | 2…20 | скільки значень FilterTask читає за раз |

Сценарій оператора:

| Час, мс | Команда | Результат |
|---------|---------|-----------|
| 1500 | `SET_THRESHOLD 50` | застосовано |
| 3000 | `SET_BLOCK 5` | застосовано |
| 4000 | `SET_THRESHOLD abc` | відхилено: не число |
| 7500 | `SET_THRESHOLD 35` | застосовано |
| 8500 | `SET_BLOCK 99` | відхилено: поза діапазоном |

### Event group — стан системи

| Біт | Хто встановлює | Значення |
|-----|----------------|----------|
| `SENSOR_OK` | FilterTask | 3 повні блоки поспіль: потік стабільний (знімається при тайм-ауті) |
| `CONFIG_OK` | ConfigTask | команду застосовано |
| `ERROR_TIMEOUT` | FilterTask | 500 мс без даних |
| `CONFIG_ERROR` | ConfigTask | команду відхилено (додатковий біт) |

SupervisorTask спершу чекає, поки одночасно встановляться `SENSOR_OK` і `CONFIG_OK` (`xWaitForAllBits = pdTRUE`), і друкує `SYSTEM READY`. Далі він чекає на будь-яку з подій `CONFIG_OK`, `CONFIG_ERROR`, `ERROR_TIMEOUT` (`xClearOnExit = pdTRUE`). Після `ERROR_TIMEOUT` він чекає повернення `SENSOR_OK` і друкує, скільки тривав збій.

## Приклад виводу

```
 1501  [Filter]     3 full blocks in a row -> set SENSOR_OK
 1501  [Supervisor] *** SYSTEM READY: SENSOR_OK & CONFIG_OK (wait for ALL bits) ***
 4000  [Config]     "SET_THRESHOLD abc" (17 bytes) -> REJECTED, set CONFIG_ERROR
 5001  [Sensor]     link LOST (simulated), no data
 5751  [Filter]     no data for 500 ms -> set ERROR_TIMEOUT, clear SENSOR_OK
 7201  [Supervisor] SENSOR_OK again -> stream restored, outage handled in 1450 ms
```

## Структура

```
.
├── main.c             — задачі, stream buffer, message buffer, event group
├── FreeRTOSConfig.h   — configUSE_STREAM_BUFFERS = 1, configUSE_EVENT_GROUPS = 1, такт 1 мс
├── Makefile           — збірка під MinGW (Windows) або POSIX (Linux/macOS)
├── build.bat          — збірка під Visual Studio (MSVC) і запуск
├── run.bat            — збірка, запуск, журнал і два скриншоти в results/
├── shot.ps1, show.ps1 — допоміжні: скриншот вікна консолі, вивід журналу
└── README.md
```

Ядро FreeRTOS у репозиторій не входить. Потрібні файли ядра `event_groups.c` і `stream_buffer.c` (їх підключають і `build.bat`, і `Makefile`).

## Як запустити (Windows, Visual Studio)

Потрібна Visual Studio з компонентом «Desktop development with C++».

```bat
git clone https://github.com/FreeRTOS/FreeRTOS.git
cd FreeRTOS\FreeRTOS\Demo
git clone -b lab6 https://github.com/ivanhaidur/rtos_lab Lab6_FreeRTOS
cd Lab6_FreeRTOS
build.bat
```

Або двічі клацніть `run.bat`.

## Як запустити (MinGW або Linux)

```bash
make run
```

Якщо ядро лежить в іншому місці, додайте `FREERTOS_KERNEL=/шлях/до/FreeRTOS/FreeRTOS/Source`.
