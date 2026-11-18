# Лабораторна робота №5 — Direct to task notification, software timers у FreeRTOS

Демо «Моніторинг псевдо-температури»: три задачі обмінюються даними без черг і семафорів, лише через direct-to-task notifications. Ритм задають два програмні таймери. Проєкт працює на симуляторі FreeRTOS для Windows (MSVC або MinGW) і Linux (POSIX).

Автор: Гайдур Іван, варіант 2.

## Варіант 2

| Елемент | Реалізація |
|---------|------------|
| SensorTask | Генерує псевдо-температуру (`rand()`, `srand(2)`) і передає її в ControlTask як value: `xTaskNotify(hControl, value, eSetValueWithOverwrite)` |
| ControlTask | `xTaskNotifyWait()` отримує температуру, вирішує OK / OVERHEAT (поріг 30.0 °C) і через `xTaskNotify()` будить DisplayTask зі статусом |
| DisplayTask | Друкує температуру і статус; при сповіщенні від таймера друкує ALARM |
| Periodic timer | Раз на 200 мс викликає `xTaskNotifyGive(hSensor)`; SensorTask чекає `ulTaskNotifyTake()` — notification як лічильний семафор |
| One-shot timer | Стартує при перегріві; якщо за 3 с температура не нормалізувалась, callback шле в DisplayTask ALARM. При нормалізації ControlTask зупиняє таймер |

32-бітне value пакується так:

| Біти | Значення |
|------|----------|
| 31..28 | тип повідомлення для DisplayTask: 0 = OK, 1 = OVERHEAT, 2 = ALARM |
| 27..16 | номер вимірювання (seq) |
| 15..0 | температура × 10 |

Сценарій температури (50 вимірювань, 10 с): норма → короткий перегрів (~1 с, таймер зупиняється, аварії немає) → тривалий перегрів (понад 3 с, ALARM) → норма. Поверх профілю накладається шум `rand()` ±0,5 °C.

## eSetValueWithOverwrite vs eSetValueWithoutOverwrite

Щоб показати різницю при частому оновленні, ControlTask «обробляє» кожне значення 450 мс, тобто повільніше, ніж надходять нові (раз на 200 мс):

| Скрипт | Режим `xTaskNotify` | Обробка в ControlTask | Що видно |
|--------|---------------------|-----------------------|----------|
| `run_1_variant.bat` | eSetValueWithOverwrite | 10 мс | Варіант: кожне вимірювання доходить, затримка (lag) 0 |
| `run_2_overwrite_slow.bat` | eSetValueWithOverwrite | 450 мс | Непрочитане значення перезаписується: ControlTask пропускає проміжні вимірювання, але завжди отримує найсвіжіше (lag 0) |
| `run_3_without_overwrite_slow.bat` | eSetValueWithoutOverwrite | 450 мс | Поки старе значення не прочитане, `xTaskNotify` повертає `pdFAIL`, і нове відкидається: ControlTask отримує застарілі дані (lag до 2 вимірювань) |
| `run_all.bat` | | | Усі три по черзі |

`lag` — на скільки вимірювань отримане значення відстає від останнього, яке згенерував SensorTask.

## Структура

```
.
├── main.c             — задачі, таймери, notifications
├── FreeRTOSConfig.h   — configUSE_TIMERS = 1, configUSE_TASK_NOTIFICATIONS = 1, такт 1 мс
├── Makefile           — збірка під MinGW (Windows) або POSIX (Linux/macOS)
├── build.bat          — збірка під Visual Studio (MSVC) і запуск: build.bat <overwrite> <work_ms>
├── run_*.bat          — запуски: журнал і скриншоти в results/
├── shot.ps1, show.ps1 — допоміжні: скриншот вікна консолі, вивід журналу
└── README.md
```

Ядро FreeRTOS у репозиторій не входить. Для таймерів потрібен файл ядра `timers.c` (його підключають і `build.bat`, і `Makefile`).

## Як запустити (Windows, Visual Studio)

Потрібна Visual Studio з компонентом «Desktop development with C++».

```bat
git clone https://github.com/FreeRTOS/FreeRTOS.git
cd FreeRTOS\FreeRTOS\Demo
git clone -b lab5 https://github.com/ivanhaidur/rtos_lab Lab5_FreeRTOS
cd Lab5_FreeRTOS
build.bat            :: варіант: Overwrite, ControlTask 10 мс
build.bat 0 450      :: WithoutOverwrite, ControlTask 450 мс
```

Або двічі клацніть будь-який `run_*.bat`.

## Як запустити (MinGW або Linux)

```bash
make run
make run CONTROL_WORK_MS=450
make run NOTIFY_OVERWRITE=0 CONTROL_WORK_MS=450
```

Якщо ядро лежить в іншому місці, додайте `FREERTOS_KERNEL=/шлях/до/FreeRTOS/FreeRTOS/Source`.

## Приклад виводу

```
 2000  [Control] OVERHEAT detected (31.5 C >= 30.0) -> one-shot alarm timer started (3 s)
 2000  [Display] #10  31.5 C   OVERHEAT (lag 0)
 3000  [Control] temperature normalized (29.5 C) -> alarm timer stopped
 ...
 4600  [Control] OVERHEAT detected (30.0 C >= 30.0) -> one-shot alarm timer started (3 s)
 7600  [Display] !!! ALARM: overheat lasts more than 3 s !!!
```

Примітка: DisplayTask має вищий пріоритет за ControlTask. Інакше в тік 7600, коли ALARM від таймера і черговий статус від ControlTask приходять одночасно, ControlTask перезаписав би ALARM (eSetValueWithOverwrite) раніше, ніж DisplayTask його прочитав.
