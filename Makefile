# Лабораторна робота №1 — FreeRTOS (симулятор WIN32-MingW / Linux POSIX)
#
#   make                       — варіант пріоритетів (τ1 > τ2 > τ3), time slicing = 1
#   make run                   — зібрати й запустити
#   make run EQUAL=1 TIME_SLICING=1   — рівні пріоритети, квантування увімкнено
#   make run EQUAL=1 TIME_SLICING=0   — рівні пріоритети, квантування вимкнено
#   make PREFIX=VOBH_          — префікс імен задач
#
# Шлях до ядра FreeRTOS (каталог із tasks.c). За замовчуванням проєкт лежить у
# FreeRTOS/FreeRTOS/Demo/<назва_проєкту>, тоді ядро — ../../Source
FREERTOS_KERNEL ?= ../../Source

TIME_SLICING ?= 1
EQUAL        ?= 0
PREFIX       ?= HAIV_
VERBOSE      ?= 1

CC     ?= gcc
CFLAGS += -O0 -g -Wall -I. -I$(FREERTOS_KERNEL)/include \
          -DconfigUSE_TIME_SLICING=$(TIME_SLICING) \
          -DLAB_EQUAL_PRIORITIES=$(EQUAL) \
          -DLAB_PREFIX=\"$(PREFIX)\" \
          -DLAB_VERBOSE=$(VERBOSE)

KERNEL_SRC = $(FREERTOS_KERNEL)/tasks.c \
             $(FREERTOS_KERNEL)/queue.c \
             $(FREERTOS_KERNEL)/list.c \
             $(FREERTOS_KERNEL)/portable/MemMang/heap_4.c

ifeq ($(OS),Windows_NT)
    # ---- WIN32 (MinGW) ----
    PORT_DIR = $(FREERTOS_KERNEL)/portable/MSVC-MingW
    PORT_SRC = $(PORT_DIR)/port.c
    LDLIBS  += -lwinmm
    TARGET   = lab1.exe
else
    # ---- Linux / macOS (POSIX) ----
    PORT_DIR = $(FREERTOS_KERNEL)/portable/ThirdParty/GCC/Posix
    PORT_SRC = $(PORT_DIR)/port.c $(PORT_DIR)/utils/wait_for_event.c
    CFLAGS  += -I$(PORT_DIR)/utils -pthread
    LDLIBS  += -pthread
    TARGET   = lab1
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
	-rm -f lab1 lab1.exe
