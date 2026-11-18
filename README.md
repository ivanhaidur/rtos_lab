# Лабораторна робота №2 — FreeRTOS Thread Local Storage Pointers (TLS)

Пʼять задач FreeRTOS, кожна з яких тримає свій стан у слотах Thread Local Storage. Модулі журналу та контрольної суми не мають глобальних змінних: дані поточної задачі вони беруть з її TLS через `pvTaskGetThreadLocalStoragePointer(NULL, слот)`. Проєкт працює на симуляторі FreeRTOS для Windows (MSVC або MinGW) і Linux (POSIX).

Автор: Гайдур Іван, варіант IDX = 2.

## Варіант

| Параметр | Значення |
|----------|----------|
| Ntasks | 5 |
| Mode | delay |
| Burst (B) | 2 рядки в пакеті |
| TLS index V1 / V2 | 0 / 1 |
| Base | 110 мс |
| Ni = 10 · Ntasks + IDX | 52 ітерації |
| seed | 2 (= IDX) |
| Затримка задачі i | 110 + 7 · i мс (110, 117, 124, 131, 138) |

## Що робить програма

1. `main()` створює 5 задач `vWorker` з однаковим пріоритетом 1 і параметром `i = 0..4`.
2. Кожна задача виділяє `task_context_t` і `profile_t` через `pvPortMalloc()` і кладе їх у TLS:

   ```c
   vTaskSetThreadLocalStoragePointer(NULL, TLS_V1, ctx);   /* слот 0 */
   vTaskSetThreadLocalStoragePointer(NULL, TLS_V2, prof);  /* слот 1 */
   ```

3. 52 рази поспіль задача:
   - збільшує `ctx->iter`;
   - рахує `ctx->checksum` — CRC-8/SAE-J1850 (поліном 0x1D, init 0xFF, xorout 0xFF) по 8 байтах: `iter` і `seed` у little-endian;
   - додає рядок `[Task T02_i] Tick:<tick> Iter:<iter> Sum:0x%08X` у буфер TLS; коли накопичено B = 2 рядки, `burst_flush()` друкує їх одним пакетом у критичній секції, тож пакет не перемішується з виводом інших задач;
   - чекає `vTaskDelay(pdMS_TO_TICKS(110 + 7 * i))`.
4. Наприкінці задача друкує залишок буфера і `Task finished T02_i (#i)`, обнуляє слоти TLS, звільняє памʼять (`vPortFree`) і видаляє себе (`vTaskDelete(NULL)`).
5. Задача-спостерігач чекає, доки всі 5 задач завершаться, друкує підсумок із вільною купою і закриває програму.

## Структура

```
.
├── main.c            — задачі, TLS, CRC8, burst-журнал
├── FreeRTOSConfig.h  — configNUM_THREAD_LOCAL_STORAGE_POINTERS = 2, такт 1 мс
├── Makefile          — збірка під MinGW (Windows) або POSIX (Linux/macOS)
├── build.bat         — збірка під Visual Studio (MSVC) і запуск
├── run.bat           — збірка, запуск, журнал і скриншоти в results/
├── shot.ps1, show.ps1 — допоміжні: скриншот вікна консолі, вивід журналу
└── README.md
```

Ядро FreeRTOS у репозиторій не входить.

## Як запустити (Windows, Visual Studio)

Потрібна Visual Studio з компонентом «Desktop development with C++».

```bat
git clone https://github.com/FreeRTOS/FreeRTOS.git
cd FreeRTOS\FreeRTOS\Demo
git clone -b lab2 https://github.com/ivanhaidur/rtos_lab Lab2_FreeRTOS
cd Lab2_FreeRTOS
build.bat
```

Або двічі клацніть `run.bat`: він збере й запустить програму, а журнал і два скриншоти (початок і кінець) збереже в `results/`.

## Як запустити (MinGW або Linux)

```bash
make run
```

Якщо ядро лежить в іншому місці, вкажіть шлях до каталогу з `tasks.c`:

```bash
make run FREERTOS_KERNEL=/шлях/до/FreeRTOS/FreeRTOS/Source
```

## Приклад виводу

```
[Task T02_0] start: TLS[0]=ctx 0x...0e0, TLS[1]=prof 0x...390, delay=110 ms, Ni=52, B=2
...
[Task T02_0] Tick:1 Iter:1 Sum:0x00000091
[Task T02_0] Tick:111 Iter:2 Sum:0x00000070
...
Task finished T02_4 (#4)

All 5 tasks finished at tick 7201. Free heap: 209928 -> 255568 bytes
```

Рядки виводяться парами (B = 2). Контрольні суми однакові для всіх задач на однаковій ітерації, бо `seed` у всіх задач дорівнює IDX: так видно, що кожна задача рахує своє, але за однією формулою. Остання задача T02_4 (затримка 138 мс) завершується приблизно за 52 · 138 ≈ 7,2 с.
