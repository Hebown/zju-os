#include "fs.h"
#include "stdint.h"
#include "stdlib.h"
#include "syscall.h"
#include "mm.h"
#include "printk.h"
#include "proc.h"
#include "defs.h"
#include "unistd.h"
uint64_t do_fork(struct pt_regs *regs);
void syscall(struct pt_regs *regs) {
    uint64_t syscall_num = regs->x17;
    
    uint64_t ret_val = 0;
    
    switch (syscall_num) {
        case SYS_WRITE:
            ret_val = sys_write(
                (regs->x10),    // fd (a0)
                (const char*)(regs->x11),     // buf (a1)
                (regs->x12)           // count (a2)
            );
            break;
        case SYS_READ:
            ret_val= sys_read(
                regs->x10,
                (const char*)(regs->x11), 
                regs->x12
            );
            break;
        case SYS_GETPID:
            ret_val = sys_getpid();
            break;
        case SYS_CLONE:
            ret_val = do_fork(regs);
            break;
        case SYS_OPENAT:
            ret_val = sys_open_at(
                (int)(regs->x10),    // fd (a0)
                (const char*)(regs->x11),     // buf (a1)
                (regs->x12)         // flags
            );
            break;
        case SYS_CLOSE:
            sys_close((int)(regs->x10));
            break;
        case SYS_LSEEK:
            ret_val=sys_lseek(regs->x10,regs->x11, regs->x12);
            break;
        default:
            printk("Unknown syscall: %ld\n", syscall_num);
            ret_val = -1;
            break;
    }
    
    regs->x10 = ret_val;
}
extern struct task_struct* current;
int64_t sys_write(uint64_t fd, const char *buf, uint64_t len) {
    int64_t ret;
    struct file *file = &(current->files->fd_array[fd]);
    if (file->opened == 0) {
        printk("file not opened\n");
        return ERROR_FILE_NOT_OPEN;
    } else {
        switch (file->perms) {
            case O_RDWR:
            case FILE_WRITABLE:{
                ret=file->write(file,buf,len);
                break;
            }
            default:{
                ret=ERROR_FILE_NOT_WRITEABLE;
                break;
            }
        }
    }
    return ret;
}
extern struct task_struct *current;
uint64_t sys_getpid() {
    if (current == NULL) {
        return -1;
    }
    return current->pid;
}

int64_t sys_read(unsigned int fd, const char* buf, uint64_t len){
    int64_t ret;
    struct file *file = &(current->files->fd_array[fd]);
    if (file->opened == 0) {
        printk("file not opened\n");
        return ERROR_FILE_NOT_OPEN;
    } else {
        if(file->perms & 1){
            ret = file->read(file, buf, len);
        }
    }
    return ret;
}

extern struct task_struct *current;
int64_t sys_open_at(int dfd, char *filename, int flags) {
    
    // 查找空闲的文件描述符
    int fd = -1;
    for (int i = 0; i < MAX_FILE_NUMBER; i++) {
        if (current->files->fd_array[i].opened == 0) {
            fd = i;
            break;
        }
    }
    
    if (fd == -1) return -1;
    
    // 打开文件
    struct file *file = &current->files->fd_array[fd];
    int ret = file_open(file, filename, flags);
    if (ret < 0) return -1;
    
    return fd;
}

void sys_close(int fd){
    current->files->fd_array[fd].opened=0;
}


extern char swapper_pg_dir[];
extern struct task_struct* task[];
extern struct task_struct* current;
extern uint64_t nr_tasks;
extern void __dummy();
extern void __ret_from_fork();
void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, uint64_t perm);


#define SSTATUS_SPP (1L << 8)   // Previous privilege mode (1=S, 0=U)
#define SSTATUS_SPIE (1L << 5)  // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4)  // User Previous Interrupt Enable  
#define SSTATUS_SUM (1L` << 18)  // Supervisor User Memory access enable

int64_t sys_lseek(int fd, int64_t offset, int whence) {
    struct file* file=&current->files->fd_array[fd];
    if (file == NULL) {
        return -1;
    }
    
    // 检查文件类型，调用对应的 lseek 函数
    if(file->fs_type == FS_TYPE_FAT32){
        return file->lseek(file,offset,whence);
    }
        
    return -1;
}
uint64_t do_fork(struct pt_regs* regs){
    // 找到空闲的任务槽位
    uint64_t i;
    for (i = 1; i < NR_TASKS; i++) {
        if (task[i] == NULL) {
            break;
        }
    }
    if (i >= NR_TASKS) {
        return -1;
    }
    
    // 分配新任务结构
    task[i] = (struct task_struct*)alloc_page();
    
    // 拷贝父进程的 task_struct
    char *src = (char*)current;
    char *dst = (char*)task[i];
    for (int j = 0; j < sizeof(struct task_struct); j++) {
        dst[j] = src[j];
    }
    
    // 设置新进程的特定属性
    task[i]->pid = i;
    task[i]->state = TASK_RUNNING;
    
    // 设置返回地址
    task[i]->thread.ra = (uint64_t)__ret_from_fork;
    
    // 分配和拷贝页表
    task[i]->pgd = (uint64_t*)kalloc();
    uint64_t *src_pgd = swapper_pg_dir;
    uint64_t *dst_pgd = task[i]->pgd;
    for (int j = 0; j < PGSIZE / sizeof(uint64_t); j++) {
        dst_pgd[j] = src_pgd[j];
    }
    
    // 初始化内存管理结构
    task[i]->mm.mmap = NULL;
    
    // 拷贝 VMA 链表
    struct vm_area_struct *parent_vma = current->mm.mmap;
    while (parent_vma != NULL) {
        // 分配新 VMA
        struct vm_area_struct *new_vma = (struct vm_area_struct*)alloc_page();
        
        // 拷贝 VMA 内容（使用循环）
        char *vma_src = (char*)parent_vma;
        char *vma_dst = (char*)new_vma;
        for (int j = 0; j < sizeof(struct vm_area_struct); j++) {
            vma_dst[j] = vma_src[j];
        }
        
        // 插入到新进程的 VMA 链表头部
        new_vma->vm_next = task[i]->mm.mmap;
        new_vma->vm_prev = NULL;
        if (task[i]->mm.mmap != NULL) {
            task[i]->mm.mmap->vm_prev = new_vma;
        }
        task[i]->mm.mmap = new_vma;
        
        // 深拷贝已映射的页面
        uint64_t page_addr = new_vma->vm_start;
        while (page_addr < new_vma->vm_end) {
            uint64_t *tmptbl = current->pgd;
            uint64_t vpn2 = (page_addr >> 30) & 0x1ff;
            uint64_t vpn1 = (page_addr >> 21) & 0x1ff;
            uint64_t vpn0 = (page_addr >> 12) & 0x1ff;
            uint64_t pte = tmptbl[vpn2];
            
            if (!(pte & 0x1)) {
                page_addr = PGROUNDDOWN(page_addr + PGSIZE);
                continue;
            }
            
            tmptbl = (uint64_t*)(((pte >> 10) << 12) + PA2VA_OFFSET);
            pte = tmptbl[vpn1];
            if (!(pte & 0x1)) {
                page_addr = PGROUNDDOWN(page_addr + PGSIZE);
                continue;
            }
            
            tmptbl = (uint64_t*)(((pte >> 10) << 12) + PA2VA_OFFSET);
            pte = tmptbl[vpn0];
            if (!(pte & 0x1)) {
                page_addr = PGROUNDDOWN(page_addr + PGSIZE);
                continue;
            }
            
            // 页面已映射，进行深拷贝
            uint64_t *copy_addr = (uint64_t*)(((pte >> 10) << 12) + PA2VA_OFFSET);
            uint64_t *phy_addr = alloc_page();
            
            // 清零并拷贝页面内容
            for (int k = 0; k < PGSIZE; k++) {
                ((char*)phy_addr)[k] = 0; // 先清零
            }
            for (int k = 0; k < PGSIZE; k++) {
                ((char*)phy_addr)[k] = ((char*)copy_addr)[k]; // 再拷贝
            }
            
            // 建立子进程映射
            create_mapping(task[i]->pgd, PGROUNDDOWN(page_addr), 
                          (uint64_t)phy_addr - PA2VA_OFFSET, PGSIZE, 0x1f);
            
            page_addr = PGROUNDDOWN(page_addr + PGSIZE);
        }
        
        parent_vma = parent_vma->vm_next;
    }
    
    // 设置子进程的栈和上下文
    task[i]->thread.sp = ((uint64_t)regs & 0xfff) + (uint64_t)task[i];
    struct pt_regs *child_regs = (struct pt_regs*)task[i]->thread.sp;
    
    // 拷贝寄存器上下文
    char *regs_src = (char*)regs;
    char *regs_dst = (char*)child_regs;
    for (int j = 0; j < sizeof(struct pt_regs); j++) {
        regs_dst[j] = regs_src[j];
    }
    
    // 子进程返回 0
    child_regs->x10 = 0;
    
    // 调整sepc
    task[i]->thread.sepc += 4;
    child_regs->sepc = task[i]->thread.sepc;
    
    // 设置sscratch
    task[i]->thread.sscratch = csr_read(sscratch);
    
    // 父进程返回子进程 PID
    nr_tasks++;
    return i;
}