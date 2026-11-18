# Лабораторна робота №4 — Counting Semaphore, Binary Semaphore, Mutex у FreeRTOS

Проєкт показує три механізми синхронізації FreeRTOS: бінарний семафор для сигналізації між задачами, лічильний семафор для пулу ресурсів і м'ютекс для спільної консолі. Індивідуальне завдання — варіант 2 «Два сенсори, один обробник». Проєкт працює на симуляторі FreeRTOS для Windows (MSVC або MinGW) і Linux (POSIX).

Автор: Гайдур Іван, варіант 2.

## Варіант 2: «Два сенсори – один обробник»

| Параметр | Значення |
|----------|----------|
| Тип синхронізації | Binary Semaphore |
| Сенсори | SensorA — раз на 500 мс, SensorB — раз на 900 мс |
| Обробник | Handler ловить усі події й показує, від кого сигнал (через ID) |

Бінарний семафор не передає даних і не рахує сигнали, тому сенсор спершу збільшує свій лічильник `pending[id]` (у критичній секції), а потім викликає `xSemaphoreGive()`. Після кожного `xSemaphoreTake()` Handler забирає всі накопичені події з обох лічильників і друкує ID джерела. Навіть якщо два сигнали «злилися» в один стан семафора, жодна подія не губиться.

Періоди 500 і 900 мс збігаються кожні 4500 мс (НСК), тож на тіках 4500 і 9000 обидва сенсори дають сигнал одночасно.

## Демонстрації

Один `main.c`, режим задається макросом `DEMO`:

| DEMO | Скрипт | Що показує |
|------|--------|------------|
| 1 | `run_1_binary.bat` | Варіант 2. Handler (пріоритет 3) вищий за сенсори (2) і реагує на кожен `Give` миттєво |
| 2 | `run_2_binary_lowprio.bat` | Варіант 2, але Handler (1) нижчий за сенсори. На тіках 4500 і 9000 другий `Give` повертає `pdFAIL`, а Handler отримує 2 події за один `Take` |
| 3 | `run_3_counting.bat` | Counting Semaphore: 4 задачі ділять пул із 2 ресурсів; видно, хто чекає і скільки |
| 4 | `run_4_mutex_off.bat` | 3 задачі друкують повідомлення з 3 рядків без м'ютекса — рядки перемішуються |
| 5 | `run_5_mutex_on.bat` | Те саме з м'ютексом — кожне повідомлення друкується цілим блоком |
| | `run_all.bat` | Усі п'ять по черзі |

Кожен рядок журналу починається з номера тіку (1 тік = 1 мс). Наприкінці програма друкує підсумок (`SUMMARY`) і завершується.

Приклад виводу DEMO 2 на тіку 4500:

```
 4500  [SensorB] signal #5 -> Give
 4500  [SensorA] signal #9 -> Give
 4500  [SensorA] Give FAILED: semaphore already given (Handler has not taken it yet)
 4500      [Handler] EVENT from SensorA (ID=0)  <- 2 events in one Take
 4500      [Handler] EVENT from SensorB (ID=1)  <- 2 events in one Take
```

## Структура

```
.
├── main.c             — п'ять демонстрацій (DEMO 1..5)
├── FreeRTOSConfig.h   — конфігурація ядра (такт 1 мс, м'ютекси, лічильні семафори)
├── Makefile           — збірка під MinGW (Windows) або POSIX (Linux/macOS)
├── build.bat          — збірка під Visual Studio (MSVC) і запуск: build.bat <DEMO>
├── run_*.bat          — демонстрації: журнал і скриншот у results/
├── shot.ps1, show.ps1 — допоміжні: скриншот вікна консолі, вивід журналу
└── README.md
```

Ядро FreeRTOS у репозиторій не входить.

## Як запустити (Windows, Visual Studio)

Потрібна Visual Studio з компонентом «Desktop development with C++».

```bat
git clone https://github.com/FreeRTOS/FreeRTOS.git
cd FreeRTOS\FreeRTOS\Demo
git clone -b lab4 https://github.com/ivanhaidur/rtos_lab Lab4_FreeRTOS
cd Lab4_FreeRTOS
build.bat 1
```

Або двічі клацніть будь-який `run_*.bat`.

## Як запустити (MinGW або Linux)

```bash
make run DEMO=1
make run DEMO=5
```

Якщо ядро лежить в іншому місці, додайте `FREERTOS_KERNEL=/шлях/до/FreeRTOS/FreeRTOS/Source`.
