export
CROSS	:=	riscv64-linux-gnu-
GCC		:=	$(CROSS)gcc
LD		:=	$(CROSS)ld
OBJCOPY	:=	$(CROSS)objcopy
OBJDUMP	:=	$(CROSS)objdump

ISA		:=	rv64imafd
ABI		:=	lp64

INCLUDE	:=	-I $(shell pwd)/include -I $(shell pwd)/arch/riscv/include -I $(shell pwd)/user $(shell pwd)/fs
CF		:=	-march=$(ISA) -mabi=$(ABI) -mcmodel=medany -fno-builtin -ffunction-sections -fdata-sections -nostartfiles -nostdlib -nostdinc -static -lgcc -Wl,--nmagic -Wl,--gc-sections -g -fno-pie # 添加fno-pie

LOG     := 1
TEST_SCHED  :=  0
ELF := 0
CFLAG	:=	$(CF) $(INCLUDE) -DTEST_SCHED=$(TEST_SCHED)  -DELF=$(ELF) -DLOG=$(LOG)

.PHONY:all run debug clean
all: clean
	$(MAKE) -C lib all
	$(MAKE) -C init all
	$(MAKE) -C fs all
	$(MAKE) -C user all
	$(MAKE) -C arch/riscv all
	@echo -e '\n'Build Finished OK

run: all
	@echo Launch qemu for run...
	@qemu-system-riscv64 -nographic -machine virt -kernel vmlinux -bios default \
        -global virtio-mmio.force-legacy=false \
        -drive file=disk.img,if=none,format=raw,id=hd0 \
        -device virtio-blk-device,drive=hd0

debug: all
	@echo Launch qemu for debug...
	@qemu-system-riscv64 -nographic -machine virt -kernel vmlinux -bios default \
        -global virtio-mmio.force-legacy=false \
        -drive file=disk.img,if=none,format=raw,id=hd0 \
        -device virtio-blk-device,drive=hd0 -S -s

clean:
	$(MAKE) -C lib clean
	$(MAKE) -C init clean
	$(MAKE) -C fs clean
	$(MAKE) -C user clean
	$(MAKE) -C arch/riscv clean
	$(shell test -f vmlinux && rm vmlinux)
	$(shell test -f vmlinux.asm && rm vmlinux.asm)
	$(shell test -f System.map && rm System.map)
	@echo -e '\n'Clean Finished
