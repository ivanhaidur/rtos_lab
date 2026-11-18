# Лабораторна робота №6 — stream/message buffers і event groups FreeRTOS (симулятор WIN32-MingW / Linux POSIX)
#
#   make run   — зібрати й запустити
#
# Шлях до ядра FreeRTOS (каталог із tasks.c). За замовчуванням проєкт лежить у
# FreeRTOS/FreeRTOS/Demo/<назва_проєкту>, тоді ядро — ../../Source
FREERTOS_KERNEL ?= ../../Source

CC     ?= gcc
CFLAGS += -O0 -g -Wall -I. -I$(FREERTOS_KERNEL)/include

KERNEL_SRC = $(FREERTOS_KERNEL)/tasks.c \
             $(FREERTOS_KERNEL)/queue.c \
             $(FREERTOS_KERNEL)/list.c \
             $(FREERTOS_KERNEL)/event_groups.c \
             $(FREERTOS_KERNEL)/stream_buffer.c \
             $(FREERTOS_KERNEL)/portable/MemMang/heap_4.c

ifeq ($(OS),Windows_NT)
    # ---- WIN32 (MinGW) ----
    PORT_DIR = $(FREERTOS_KERNEL)/portable/MSVC-MingW
    PORT_SRC = $(PORT_DIR)/port.c
    LDLIBS  += -lwinmm
    TARGET   = lab6.exe
else
    # ---- Linux / macOS (POSIX) ----
    PORT_DIR = $(FREERTOS_KERNEL)/portable/ThirdParty/GCC/Posix
    PORT_SRC = $(PORT_DIR)/port.c $(PORT_DIR)/utils/wait_for_event.c
    CFLAGS  += -I$(PORT_DIR)/utils -pthread
    LDLIBS  += -pthread
    TARGET   = lab6
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
	-rm -f lab6 lab6.exe
