# Лабораторна робота №4 — семафори і м'ютекс FreeRTOS (симулятор WIN32-MingW / Linux POSIX)
#
#   make run DEMO=1   — Binary Semaphore, варіант 2 (Handler вищий)
#   make run DEMO=2   — Binary Semaphore, Handler нижчий за сенсори
#   make run DEMO=3   — Counting Semaphore
#   make run DEMO=4   — консоль без м'ютекса
#   make run DEMO=5   — консоль з м'ютексом
#
# Шлях до ядра FreeRTOS (каталог із tasks.c). За замовчуванням проєкт лежить у
# FreeRTOS/FreeRTOS/Demo/<назва_проєкту>, тоді ядро — ../../Source
FREERTOS_KERNEL ?= ../../Source

DEMO ?= 1

CC     ?= gcc
CFLAGS += -O0 -g -Wall -I. -I$(FREERTOS_KERNEL)/include \
          -DDEMO=$(DEMO)

KERNEL_SRC = $(FREERTOS_KERNEL)/tasks.c \
             $(FREERTOS_KERNEL)/queue.c \
             $(FREERTOS_KERNEL)/list.c \
             $(FREERTOS_KERNEL)/portable/MemMang/heap_4.c

ifeq ($(OS),Windows_NT)
    # ---- WIN32 (MinGW) ----
    PORT_DIR = $(FREERTOS_KERNEL)/portable/MSVC-MingW
    PORT_SRC = $(PORT_DIR)/port.c
    LDLIBS  += -lwinmm
    TARGET   = lab4.exe
else
    # ---- Linux / macOS (POSIX) ----
    PORT_DIR = $(FREERTOS_KERNEL)/portable/ThirdParty/GCC/Posix
    PORT_SRC = $(PORT_DIR)/port.c $(PORT_DIR)/utils/wait_for_event.c
    CFLAGS  += -I$(PORT_DIR)/utils -pthread
    LDLIBS  += -pthread
    TARGET   = lab4
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
	-rm -f lab4 lab4.exe
