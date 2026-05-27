# ═══════════════════════════════════════════════
# Makefile — Ducati PRO-III Minimal Firmware
# Компілятор: arm-none-eabi-gcc
# Цільовий MCU: GD32F103C8T6 (= STM32F103C8T6)
# ═══════════════════════════════════════════════

TARGET   = ducati_pro3
MCU      = cortex-m3
LDSCRIPT = GD32F103x8.ld

# Директорії
SRC_DIR  = src
INC_DIR  = inc
LIB_DIR  = lib/GD32F10x_standard_peripheral

# Компілятор
CC       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy
SIZE     = arm-none-eabi-size

# Прапорці компіляції
CFLAGS  = -mcpu=$(MCU) -mthumb
CFLAGS += -O2 -Wall -Wextra
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -DGDBOARDV1 -DGD32F10X_MD
CFLAGS += -I$(INC_DIR)
CFLAGS += -I$(LIB_DIR)/Include
CFLAGS += -I$(LIB_DIR)/CMSIS/Include

# Лінкер
LDFLAGS  = -mcpu=$(MCU) -mthumb
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(TARGET).map
LDFLAGS += --specs=nosys.specs

# Вихідні файли
SRCS  = $(SRC_DIR)/main.c
SRCS += $(SRC_DIR)/motor.c
SRCS += $(SRC_DIR)/peripherals.c
SRCS += $(LIB_DIR)/Source/gd32f10x_adc.c
SRCS += $(LIB_DIR)/Source/gd32f10x_gpio.c
SRCS += $(LIB_DIR)/Source/gd32f10x_timer.c
SRCS += $(LIB_DIR)/Source/gd32f10x_rcu.c
SRCS += $(LIB_DIR)/Source/gd32f10x_exti.c
SRCS += $(LIB_DIR)/Source/gd32f10x_misc.c
SRCS += startup_gd32f10x_md.s

OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.s=.o)

# ───────────────────────────────────────────────
all: $(TARGET).bin

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@echo ""
	@echo "✅ Готово: $(TARGET).bin"
	@echo "   Заливай командою:"
	@echo "   openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \\"
	@echo "     -c \"program $(TARGET).bin verify reset exit 0x08000000\""

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).map

flash: $(TARGET).bin
	openocd -f interface/stlink.cfg \
	        -f target/stm32f1x.cfg \
	        -c "program $(TARGET).bin verify reset exit 0x08000000"

.PHONY: all clean flash
