#     .extern start_kernel
#     .section .text.entry # 定义代码入口section
#     .globl _start
# _start:
#     la sp, boot_stack_top # 把sp初始化到我们的栈顶上
#     j start_kernel # 直接跳转到内核起点

#     .section .bss.stack # 这里定义栈section
#     .globl boot_stack # 指定栈底
# boot_stack:
#     .space 4096 # 通过space伪指令划出4kb内存（4096bytes）

#     .globl boot_stack_top # 指定栈顶
# boot_stack_top:


    .extern start_kernel
    .extern mm_init
    .extern task_init
    .extern setup_vm
    .extern setup_vm_final
    .extern _traps
    .extern virtio_dev_init
    .extern mbr_init
    .section .text.init
    .globl _start
_start:

    la sp, boot_stack_top

    call setup_vm
    call relocate
    call mm_init
    call setup_vm_final

    la t0, _traps
    csrw stvec, t0
    csrr t0, sie        # 读取 sie
    ori t0, t0, 0x20    # 设置第 5 位
    csrw sie, t0        # 写回 sie

    call task_init
    call virtio_dev_init
    call mbr_init
    
    rdtime t0           # 读mtime
    li t1, 10000000     # 设定interval
    add t0,t0,t1        
    mv a0,t0 
    li a7, 0x54494d45
    li a6, 0
    ecall               # 调用settimer函数

    # csrsi sstatus, 0x2 # 设置第1位


    j start_kernel 

relocate:
    # PA2VA_OFFSET = 0xffffffe000000000 - 0x80000000 = 0xffffffdf80000000
    li t0, 0xffffffdf80000000
    add ra,ra,t0
    add sp,sp,t0

    sfence.vma zero, zero

    la t1,early_pgtbl
    srli t1,t1,12 # 设定PPN
    li t2,8<<60 # 设定mode
    or t1,t1,t2 # 结合起来
    csrw satp,t1
    ret

    .section .bss.stack
    .globl boot_stack 
boot_stack:
    .space 4096 

    .globl boot_stack_top
boot_stack_top: