    .extern trap_handler
    .section .text.entry
    .align 2
    .globl _traps 
    .globl _traps
_traps:
    csrr t0, sscratch
    li t1, 0
    beq t0, t1, _ssp
    csrw sscratch, sp
    addi sp, t0, 0          # change sp to kernel_sp if it's a user thread
_ssp:
    addi sp, sp, -272
    sd ra, 0(sp)
    sd gp, 8(sp)
    sd tp, 16(sp)
    sd t0, 24(sp)
    sd t1, 32(sp)
    sd t2, 40(sp)
    sd s0, 48(sp)
    sd s1, 56(sp)
    sd a0, 64(sp)
    sd a1, 72(sp)
    sd a2, 80(sp)
    sd a3, 88(sp)
    sd a4, 96(sp)
    sd a5, 104(sp)
    sd a6, 112(sp)
    sd a7, 120(sp)
    sd s2, 128(sp)
    sd s3, 136(sp)
    sd s4, 144(sp)
    sd s5, 152(sp)
    sd s6, 160(sp)
    sd s7, 168(sp)
    sd s8, 176(sp)
    sd s9, 184(sp)
    sd s10, 192(sp)
    sd s11, 200(sp)
    sd t3, 208(sp)
    sd t4, 216(sp)
    sd t5, 224(sp)
    sd t6, 232(sp)
    
    csrr a0, scause
    csrr a1, sepc
    sd a1, 240(sp)
    csrr a2, sstatus
    sd a2, 248(sp)
    csrr a3, stval          
    sd a3, 256(sp)          
    addi a2, sp, 0

    call trap_handler

    .globl __ret_from_fork
__ret_from_fork:
    ld a3, 256(sp)          
    csrw stval, a3          
    ld a2, 248(sp)
    csrw sstatus, a2
    ld a1, 240(sp)
    csrw sepc, a1
    ld t6, 232(sp)
    ld t5, 224(sp)
    ld t4, 216(sp)
    ld t3, 208(sp)
    ld s11, 200(sp)
    ld s10, 192(sp)
    ld s9, 184(sp)
    ld s8, 176(sp)
    ld s7, 168(sp)
    ld s6, 160(sp)
    ld s5, 152(sp)
    ld s4, 144(sp)
    ld s3, 136(sp)
    ld s2, 128(sp)
    ld a7, 120(sp)
    ld a6, 112(sp)
    ld a5, 104(sp)
    ld a4, 96(sp)
    ld a3, 88(sp)
    ld a2, 80(sp)
    ld a1, 72(sp)
    ld a0, 64(sp)
    ld s1, 56(sp)
    ld s0, 48(sp)
    ld t2, 40(sp)
    ld t1, 32(sp)
    ld t0, 24(sp)
    ld tp, 16(sp)
    ld gp, 8(sp)
    ld ra, 0(sp)
    addi sp, sp, 272        

    csrr t0, sscratch
    li t1, 0
    beq t0, t1, _esp
    csrw sscratch, sp
    addi sp, t0, 0
_esp:
    sret

    .extern dummy
    .globl __dummy
__dummy:
    # 交换 sp 和 sscratch，实现用户栈和内核栈的切换
    csrrw sp, sscratch, sp
    
    sret

    .globl __switch_to
__switch_to:
    # save state to prev process
    # 保存 ra (偏移 32)
    sd ra, 32(a0)
    # 保存 sp (偏移 40)
    sd sp, 40(a0)
    # 保存 s0 到 s11 (偏移 48 到 136)
    sd s0, 48(a0)
    sd s1, 56(a0)
    sd s2, 64(a0)
    sd s3, 72(a0)
    sd s4, 80(a0)
    sd s5, 88(a0)
    sd s6, 96(a0)
    sd s7, 104(a0)
    sd s8, 112(a0)
    sd s9, 120(a0)
    sd s10, 128(a0)
    sd s11, 136(a0)

    # lab4: 保存用户态相关寄存器
    # 保存 sepc (偏移 144)
    csrr t0, sepc
    sd t0, 144(a0)

    # 保存 sstatus (偏移 152)  
    csrr t0, sstatus
    sd t0, 152(a0)

    # 保存 sscratch (偏移 160)
    csrr t0, sscratch
    sd t0, 160(a0)

    # 切换页表
    # 获取下一个进程的页表物理地址
    ld t1, 168(a1)
    li t3, 0xffffffdf80000000
    sub t1, t1, t3

    # 计算 PPN (Physical Page Number)
    srli t1, t1, 12       # 右移12位得到PPN
    
    # 设置 SATP: MODE = 8 (Sv39), PPN = 页表物理页号
    li t2, (8 << 60)      # MODE = Sv39
    or t1, t1, t2         # 组合 MODE 和 PPN
    
    # 写入 SATP 寄存器
    csrw satp, t1
    
    # 刷新 TLB 和 ICache
    sfence.vma zero, zero
    # 恢复 sp (偏移 40)
    ld sp, 40(a1)
    # 恢复 ra (偏移 32)
    ld ra, 32(a1)

    # restore state from next process
    # 恢复 sepc (偏移 144)
    ld t0, 144(a1)
    csrw sepc, t0
    
    # 恢复 sstatus (偏移 152)
    ld t0, 152(a1)  
    csrw sstatus, t0
    
    # 恢复 sscratch (偏移 160)
    ld t0, 160(a1)
    csrw sscratch, t0

    # 恢复 s0 到 s11 (偏移 48 到 136)
    ld s0, 48(a1)
    ld s1, 56(a1)
    ld s2, 64(a1)
    ld s3, 72(a1)
    ld s4, 80(a1)
    ld s5, 88(a1)
    ld s6, 96(a1)
    ld s7, 104(a1)
    ld s8, 112(a1)
    ld s9, 120(a1)
    ld s10, 128(a1)
    ld s11, 136(a1)

    ret