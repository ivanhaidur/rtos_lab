# Лабораторна робота №5 — task notifications і програмні таймери FreeRTOS (симулятор WIN32-MingW / Linux POSIX)
#
#   make run                                   — варіант: Overwrite, ControlTask встигає (10 мс)
#   make run CONTROL_WORK_MS=450               — Overwrite, ControlTask не встигає
#   make run NOTIFY_OVERWRITE=0 CONTROL_WORK_MS=450 — WithoutOverwrite, ControlTask не встигає
#
# Шлях до ядра FreeRTOS (каталог із tasks.c). За замовчуванням проєкт лежить у
# FreeRTOS/FreeRTOS/Demo/<назва_проєкту>, тоді ядро — ../../Source
FREERTOS_KERNEL ?= ../../Source

NOTIFY_OVERWRITE ?= 1
CONTROL_WORK_MS  ?= 10

CC     ?= gcc
CFLAGS += -O0 -g -Wall -I. -I$(FREERTOS_KERNEL)/include \
          -DNOTIFY_OVERWRITE=$(NOTIFY_OVERWRITE) -DCONTROL_WORK_MS=$(CONTROL_WORK_MS)

KERNEL_SRC = $(FREERTOS_KERNEL)/tasks.c \
             $(FREERTOS_KERNEL)/queue.c \
             $(FREERTOS_KERNEL)/list.c \
             $(FREERTOS_KERNEL)/timers.c \
             $(FREERTOS_KERNEL)/portable/MemMang/heap_4.c

ifeq ($(OS),Windows_NT)
    # ---- WIN32 (MinGW) ----
    PORT_DIR = $(FREERTOS_KERNEL)/portable/MSVC-MingW
    PORT_SRC = $(PORT_DIR)/port.c
    LDLIBS  += -lwinmm
    TARGET   = lab5.exe
else
    # ---- Linux / macOS (POSIX) ----
    PORT_DIR = $(FREERTOS_KERNEL)/portable/ThirdParty/GCC/Posix
    PORT_SRC = $(PORT_DIR)/port.c $(PORT_DIR)/utils/wait_for_event.c
    CFLAGS  += -I$(PORT_DIR)/utils -pthread
    LDLIBS  += -pthread
    TARGET   = lab5
endif

CFLAGS += -I$(PORT_DIR)
SRC     = main.c $(KERNEL_SRC) $(PORT_SRC)

.PHONY: all run clean
all: $(TARGET)

$(TARGET): $(SRC) FreeRTOSConfig.h
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LDLIBS)

run: clean $(TARGET)
	./$(TARGET)

clean:
	-rm -f lab5 lab5.exe
