#include "stdint.h"
#include "sbi.h"

struct sbiret sbi_ecall(uint64_t eid, uint64_t fid,
                        uint64_t arg0, uint64_t arg1, uint64_t arg2,
                        uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    struct sbiret retval; // 收集返回值的变量
    asm volatile(
        "mv a7, %[eid]\n"
        "mv a6, %[fid]\n"
        "mv a5, %[arg5]\n"
        "mv a4, %[arg4]\n"
        "mv a3, %[arg3]\n"
        "mv a2, %[arg2]\n"
        "mv a1, %[arg1]\n"
        "mv a0, %[arg0]\n"
        "ecall\n"
        "mv %[error], a0\n"
        "mv %[value], a1\n"
        :[error]"=r"(retval.error),[value]"=r"(retval.value) // output
        :[eid] "r" (eid), [fid] "r" (fid), // input
         [arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2),
         [arg3] "r" (arg3), [arg4] "r" (arg4), [arg5] "r" (arg5)
         : "a0","a1","a2","a3","a4","a5","a6","a7","memory" // 这里避免编译器优化我们涉及到的所有寄存器/内存
        
    );
    return retval;
}

struct sbiret sbi_debug_console_write_byte(uint8_t byte) {
    return sbi_ecall(0x4442434e, 0x2, byte, 0,0,0,0,0);
}

struct sbiret sbi_system_reset(uint32_t reset_type, uint32_t reset_reason) {
    return sbi_ecall(0x53525354, 0, reset_type, reset_reason, 0, 0, 0, 0);
}

struct sbiret sbi_set_timer(uint64_t stime_value){
    return sbi_ecall(0x54494d45,0,stime_value,0,0,0,0,0);
}

struct sbiret sbi_debug_console_read(uint64_t num_bytes, uint64_t base_addr_lo, uint64_t base_addr_hi){
    return sbi_ecall(0x4442434E, 0x1, num_bytes, base_addr_lo, base_addr_hi, 0,0,0);
}