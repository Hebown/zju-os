ASM_SRC		= $(filter-out uapp.S, $(sort $(wildcard *.S)))
C_SRC		= $(sort $(wildcard *.c))
OBJ			= $(patsubst %.S,%.o,$(ASM_SRC)) $(patsubst %.c,%.o,$(C_SRC)) ../lib/string.o

CFLAG		= -march=$(ISA) -mabi=$(ABI) -mcmodel=medany -fno-builtin -ffunction-sections -fdata-sections -nostartfiles -nostdlib -nostdinc -static -lgcc -Wl,--nmagic,--build-id=none -O0
INCLUDE	:=	-I $(shell pwd)/../include
all: uapp.o

uapp.o: uapp.S uapp.bin uapp
	${GCC} ${INCLUDE} ${CFLAG} -c uapp.S
	${OBJDUMP} -S uapp > uapp.asm
	${OBJDUMP} -S uapp.elf > uapp.elf.asm

%.o:%.c
	${GCC} ${INCLUDE} ${CFLAG} -c $<

%.o:%.S
	${GCC} ${INCLUDE} ${CFLAG} -c $<

uapp.bin: $(OBJ)
	${GCC} ${INCLUDE} ${CFLAG} -fpie -T link.lds -o uapp.elf ${OBJ}
	${OBJCOPY} uapp.elf -O binary uapp.bin

clean:
	$(shell rm uapp *.o uapp.o uapp.elf uapp.bin *.asm 2>/dev/null)

uapp: $(OBJ)
	${GCC} ${INCLUDE} ${CFLAG} -o uapp ${OBJ}
