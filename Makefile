# Лабораторна робота №3 — черги FreeRTOS (симулятор WIN32-MingW / Linux POSIX)
#
#   make run                              — довжина 10, Producer(2) > Consumer(1)
#   make run PROD_PRIO=1 CONS_PRIO=2      — Consumer > Producer
#   make run QUEUE_LEN=1                  — черга на 1 елемент
#
# Шлях до ядра FreeRTOS (каталог із tasks.c). За замовчуванням проєкт лежить у
# FreeRTOS/FreeRTOS/Demo/<назва_проєкту>, тоді ядро — ../../Source
FREERTOS_KERNEL ?= ../../Source

QUEUE_LEN ?= 10
PROD_PRIO ?= 2
CONS_PRIO ?= 1

CC     ?= gcc
CFLAGS += -O0 -g -Wall -I. -I$(FREERTOS_KERNEL)/include \
          -DQUEUE_LEN=$(QUEUE_LEN) -DPROD_PRIO=$(PROD_PRIO) -DCONS_PRIO=$(CONS_PRIO)

KERNEL_SRC = $(FREERTOS_KERNEL)/tasks.c \
             $(FREERTOS_KERNEL)/queue.c \
             $(FREERTOS_KERNEL)/list.c \
             $(FREERTOS_KERNEL)/portable/MemMang/heap_4.c

ifeq ($(OS),Windows_NT)
    # ---- WIN32 (MinGW) ----
    PORT_DIR = $(FREERTOS_KERNEL)/portable/MSVC-MingW
    PORT_SRC = $(PORT_DIR)/port.c
    LDLIBS  += -lwinmm
    TARGET   = lab3.exe
else
    # ---- Linux / macOS (POSIX) ----
    PORT_DIR = $(FREERTOS_KERNEL)/portable/ThirdParty/GCC/Posix
    PORT_SRC = $(PORT_DIR)/port.c $(PORT_DIR)/utils/wait_for_event.c
    CFLAGS  += -I$(PORT_DIR)/utils -pthread
    LDLIBS  += -pthread
    TARGET   = lab3
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
	-rm -f lab3 lab3.exe
