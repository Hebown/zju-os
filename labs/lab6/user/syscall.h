#ifndef __SYSCALL_H__
#define __SYSCALL_H__
#include "stdint.h"
#include "stddef.h"
#define SYS_OPENAT  56
#define SYS_CLOSE   57
#define SYS_LSEEK   62
#define SYS_READ    63
#define SYS_WRITE   64
#define SYS_GETPID  172
#define SYS_CLONE   220
struct pt_regs {
    uint64_t x1;    // ra
    uint64_t x3;    // gp
    uint64_t x4;    // tp
    uint64_t x5;    // t0
    uint64_t x6;    // t1
    uint64_t x7;    // t2
    uint64_t x8;    // s0/fp
    uint64_t x9;    // s1
    uint64_t x10;   // a0
    uint64_t x11;   // a1
    uint64_t x12;   // a2
    uint64_t x13;   // a3
    uint64_t x14;   // a4
    uint64_t x15;   // a5
    uint64_t x16;   // a6
    uint64_t x17;   // a7
    uint64_t x18;   // s2
    uint64_t x19;   // s3
    uint64_t x20;   // s4
    uint64_t x21;   // s5
    uint64_t x22;   // s6
    uint64_t x23;   // s7
    uint64_t x24;   // s8
    uint64_t x25;   // s9
    uint64_t x26;   // s10
    uint64_t x27;   // s11
    uint64_t x28;   // t3
    uint64_t x29;   // t4
    uint64_t x30;   // t5
    uint64_t x31;   // t6
    uint64_t sepc;
    uint64_t sstatus;
    uint64_t stval;
};

// 系统调用处理函数
void syscall(struct pt_regs *regs);
int64_t sys_write(uint64_t fd, const char *buf, uint64_t len);
int64_t sys_read(unsigned int fd, const char* buf, uint64_t len);
uint64_t sys_getpid(void);
int64_t sys_open_at(int dfd, char *filename, int flags);
int64_t sys_lseek(int fd, int64_t offset, int whence);
void sys_close(int fd);
#endif