# Лабораторна робота №3 — Черги (Queues) у FreeRTOS

Три задачі FreeRTOS обмінюються повідомленнями через чергу. Producer формує показання «датчика» і кладе їх у чергу без очікування, Consumer читає їх із тайм-аутом 500 мс, а Monitor періодично перевіряє, скільки елементів у черзі. Проєкт працює на симуляторі FreeRTOS для Windows (MSVC або MinGW) і Linux (POSIX).

Автор: Гайдур Іван, варіант 2.

## Варіант

| Параметр | Значення |
|----------|----------|
| Кількість задач | 3: Producer, Consumer, Monitor |
| Тип даних у черзі | `struct { float temp; uint32_t id; }` (8 байт) |
| Тайм-аут відправки | 0 — неблокуюча `xQueueSend()` |
| Тайм-аут прийому | 500 мс |
| Додатково | Monitor читає `uxQueueMessagesWaiting()` |

## Як працює програма

| Задача | Пріоритет | Що робить |
|--------|-----------|-----------|
| Producer | 2 (або 1) | 40 разів кожні 100 мс формує `{temp, id}` і викликає `xQueueSend(q, &msg, 0)`. Якщо черга повна, повідомлення відкидається з написом `queue full!` |
| Consumer | 1 (або 2) | `xQueueReceive(q, &msg, 500 мс)`, після кожного прийому 150 мс «обробки». Якщо 500 мс нічого не надійшло — `timeout waiting for data` |
| Monitor | 3 | Кожні 500 мс друкує `uxQueueMessagesWaiting()` і `uxQueueSpacesAvailable()`. Через 7 с друкує підсумок і завершує програму |

Consumer повільніший за Producer (приблизно 6,7 проти 10 повідомлень за секунду), тому черга поступово заповнюється, і неблокуюча відправка починає повертати `errQUEUE_FULL`. Після 40-го повідомлення Producer зупиняється, Consumer дочитує чергу, а далі отримує тайм-аути прийому.

Кожен рядок журналу починається з номера тіку (1 тік = 1 мс):

```
  101  [Producer] sent id=2   temp=22.0
  151  [Consumer] got  id=2   temp=22.0
 3001  [Monitor]  queue: waiting=10 free=0 <- FULL
 3001  [Producer] queue full! id=31 dropped
 5901  [Consumer] timeout waiting for data (500 ms)
```

## Експерименти

| Скрипт | Довжина черги | Пріоритети |
|--------|---------------|------------|
| `run_1_len10_prod_high.bat` | 10 | Producer (2) > Consumer (1) |
| `run_2_len10_cons_high.bat` | 10 | Consumer (2) > Producer (1) |
| `run_3_len1_prod_high.bat` | 1 | Producer (2) > Consumer (1) |
| `run_4_len1_cons_high.bat` | 1 | Consumer (2) > Producer (1) |
| `run_all.bat` | усі чотири по черзі | |

## Структура

```
.
├── main.c             — Producer, Consumer, Monitor, черга
├── FreeRTOSConfig.h   — конфігурація ядра (такт 1 мс)
├── Makefile           — збірка під MinGW (Windows) або POSIX (Linux/macOS)
├── build.bat          — збірка під Visual Studio (MSVC) і запуск
├── run_*.bat          — експерименти: журнал і скриншот у results/
├── shot.ps1, show.ps1 — допоміжні: скриншот вікна консолі, вивід журналу
└── README.md
```

Ядро FreeRTOS у репозиторій не входить.

## Як запустити (Windows, Visual Studio)

Потрібна Visual Studio з компонентом «Desktop development with C++».

```bat
git clone https://github.com/FreeRTOS/FreeRTOS.git
cd FreeRTOS\FreeRTOS\Demo
git clone -b lab3 https://github.com/ivanhaidur/rtos_lab Lab3_FreeRTOS
cd Lab3_FreeRTOS
build.bat            :: довжина 10, Producer > Consumer
build.bat 10 1 2     :: Consumer > Producer
build.bat 1 2 1      :: черга на 1 елемент
```

Або двічі клацніть будь-який `run_*.bat`.

## Як запустити (MinGW або Linux)

```bash
make run                            # довжина 10, Producer > Consumer
make run PROD_PRIO=1 CONS_PRIO=2    # Consumer > Producer
make run QUEUE_LEN=1                # черга на 1 елемент
```

Якщо ядро лежить в іншому місці, додайте `FREERTOS_KERNEL=/шлях/до/FreeRTOS/FreeRTOS/Source`.
