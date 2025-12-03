#include "fat32.h"
#include "printk.h"
#include "virtio.h"
#include "string.h"
#include "mbr.h"
#include "mm.h"

struct fat32_bpb fat32_header;
struct fat32_volume fat32_volume;

uint8_t fat32_buf[VIRTIO_BLK_SECTOR_SIZE];
uint8_t fat32_table_buf[VIRTIO_BLK_SECTOR_SIZE];

uint64_t cluster_to_sector(uint64_t cluster) {
    return (cluster - 2) * fat32_volume.sec_per_cluster + fat32_volume.first_data_sec;
}

uint32_t next_cluster(uint64_t cluster) {
    uint64_t fat_offset = cluster * 4;
    uint64_t fat_sector = fat32_volume.first_fat_sec + fat_offset / VIRTIO_BLK_SECTOR_SIZE;
    virtio_blk_read_sector(fat_sector, fat32_table_buf);
    int index_in_sector = fat_offset % (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
    return *(uint32_t*)(fat32_table_buf + index_in_sector);
}

void fat32_init(uint64_t lba, uint64_t size) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    fat32_volume.sec_per_cluster = fat32_header.sec_per_clus;
    printk(RED"lba = %d\n"CLEAR);
    fat32_volume.first_fat_sec = lba+fat32_header.rsvd_sec_cnt;
    fat32_volume.fat_sz = fat32_header.fat_sz32;
    fat32_volume.first_data_sec = lba+fat32_header.rsvd_sec_cnt + (fat32_header.num_fats * fat32_volume.fat_sz);
}

int is_fat32(uint64_t lba) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    if (fat32_header.boot_sector_signature != 0xaa55) {
        return 0;
    }
    return 1;
}

int next_slash(const char* path) {  // util function to be used in fat32_open_file
    int i = 0;
    while (path[i] != '\0' && path[i] != '/') {
        i++;
    }
    if (path[i] == '\0') {
        return -1;
    }
    return i;
}

void to_upper_case(char *str) {     // util function to be used in fat32_open_file
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] -= 32;
        }
    }
}

struct fat32_file fat32_open_file(const char *path) {
    struct fat32_file file;
    memset(&file, 0, sizeof(file));
    
    // 跳过 "/fat32/" 前缀
    const char *filename = path + 7;  // "/fat32/"
    
    // 从根目录开始查找
    uint32_t current_cluster = fat32_header.root_clus;
    
    // 遍历根目录簇链
    while (current_cluster < 0x0FFFFFF8) {
        uint64_t sector = cluster_to_sector(current_cluster);
        
        // 读取一个簇的所有扇区
        for (int s = 0; s < fat32_volume.sec_per_cluster; s++) {
            virtio_blk_read_sector(sector + s, fat32_buf);
            
            // 遍历扇区中的目录项（每个32字节）
            struct fat32_dir_entry *entry;
            for (int i = 0; i < 16; i++) {  // 512/32=16
                entry = (struct fat32_dir_entry*)(fat32_buf + i * 32);
                
                // 空项表示搜索结束
                if (entry->name[0] == 0x00) {
                    file.cluster = 0;  // 文件不存在
                    return file;
                }
                
                // 跳过删除项
                if (entry->name[0] == 0xE5) continue;
                
                // 跳过目录和卷标
                if ((entry->attr & 0x10) || (entry->attr & 0x08)) continue;
                
                // 跳过系统文件、隐藏文件等
                if (entry->attr & (0x02 | 0x04)) continue;  // 隐藏或系统
                
                // 提取主文件名（8字符）
                char fat_name[9] = {0};
                memcpy(fat_name, entry->name, 8);
                
                // 去掉尾部空格
                int len = 8;
                while (len > 0 && fat_name[len-1] == ' ') {
                    fat_name[len-1] = '\0';
                    len--;
                }
                
                // 如果长度为0，跳过（无效文件名）
                if (len == 0) continue;
                
                // 检查扩展名，如果有扩展名则跳过（因为你说没有后缀名）
                bool has_ext = false;
                for (int j = 0; j < 3; j++) {
                    if (entry->ext[j] != ' ') {
                        has_ext = true;
                        break;
                    }
                }
                if (has_ext) continue;
                
                // 转换为大写比较（FAT短文件名是大写的）
                char upper_fat_name[9];
                strcpy(upper_fat_name, fat_name);
                for (int j = 0; upper_fat_name[j] != '\0'; j++) {
                    if (upper_fat_name[j] >= 'a' && upper_fat_name[j] <= 'z') {
                        upper_fat_name[j] = upper_fat_name[j] - 'a' + 'A';
                    }
                }
                
                // 目标文件名也转换为大写
                char upper_filename[256];
                strncpy(upper_filename, filename, 255);
                upper_filename[255] = '\0';
                for (int j = 0; upper_filename[j] != '\0'; j++) {
                    if (upper_filename[j] >= 'a' && upper_filename[j] <= 'z') {
                        upper_filename[j] = upper_filename[j] - 'a' + 'A';
                    }
                }
                
                // 调试输出
                printk("Comparing: FAT='%s' (len=%d), Req='%s' (len=%d)\n", 
                       upper_fat_name, strlen(upper_fat_name), 
                       upper_filename, strlen(upper_filename));
                
                // 比较文件名
                if (strcmp(upper_fat_name, upper_filename) == 0) {
                    // 找到文件
                    file.cluster = (entry->starthi << 16) | entry->startlow;
                    file.dir.cluster = current_cluster;
                    file.dir.index = i;
                    
                    printk("Found file: name='%s', cluster=%u\n", 
                           upper_fat_name, file.cluster);
                    return file;
                }
            }
        }
        
        // 获取下一个簇
        current_cluster = next_cluster(current_cluster);
        
        // 检查簇号是否有效
        if (current_cluster >= 0x0FFFFFF8 || current_cluster < 2) {
            break;
        }
    }
    
    // 没找到
    file.cluster = 0;
    printk("File '%s' not found\n", filename);
    return file;
}
int64_t fat32_lseek(struct file* file, int64_t offset, uint64_t whence) {
    int64_t new_pos = 0;

    printk("fat32_lseek: current cfo=%ld, offset=%ld, whence=%lu\n",
           file->cfo, offset, whence);
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;

        case SEEK_CUR:
            new_pos = file->cfo + offset;
            break;
            
        case SEEK_END:
            // 获取文件大小
            if (file->fat32_file.cluster == 0) {
                printk("fat32_lseek: file not opened\n");
                return -1;
            }
            
            // 从目录项读取文件大小
            uint64_t dir_sector = cluster_to_sector(file->fat32_file.dir.cluster);
            virtio_blk_read_sector(dir_sector, fat32_buf);
            
            struct fat32_dir_entry *entry = 
                (struct fat32_dir_entry*)(fat32_buf + file->fat32_file.dir.index * 32);
            
            new_pos = entry->size + offset;
            printk("fat32_lseek: file size=%u, new_pos=%ld\n", entry->size, new_pos);
            break;
            
        default:
            printk("fat32_lseek: invalid whence\n");
            return -1;
    }
    
    // 边界检查
    if (new_pos < 0) {
        printk("fat32_lseek: negative position\n");
        return -1;
    }
    
    file->cfo = new_pos;
    printk("fat32_lseek: new cfo=%ld\n", file->cfo);
    
    return new_pos;
}
uint64_t fat32_table_sector_of_cluster(uint32_t cluster) {
    return fat32_volume.first_fat_sec + cluster / (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
}

int64_t fat32_read(struct file* file, void* buf, uint64_t len) {
    if (file->fat32_file.cluster == 0) {
        printk("Error: File not opened\n");
        return 0;
    }
    
    printk("fat32_read: cluster=%u, cfo=%lu, len=%lu\n", 
           file->fat32_file.cluster, file->cfo, len);
    
    uint32_t current_cluster = file->fat32_file.cluster;
    int64_t bytes_read = 0;
    uint8_t *dest = (uint8_t*)buf;
    
    // 计算每个簇的字节数
    uint32_t bytes_per_cluster = fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE;
    
    // 如果文件偏移为0，从头开始读取
    if (file->cfo == 0) {
        // 读取第一簇的第一个扇区
        uint64_t sector = cluster_to_sector(current_cluster);
        virtio_blk_read_sector(sector, fat32_buf);
        
        // 简单调试：打印前64个字节
        printk("First 64 bytes of file:\n");
        for (int i = 0; i < 64; i++) {
            printk("%02x ", fat32_buf[i]);
            if ((i + 1) % 16 == 0) printk("\n");
        }
        printk("\n");
        
        // 尝试读取整个文件内容
        uint32_t total_bytes = 0;
        while (current_cluster < 0x0FFFFFF8 && current_cluster >= 2) {
            uint64_t cluster_start_sector = cluster_to_sector(current_cluster);
            printk("Reading cluster %u (sector %lu)\n", 
                   current_cluster, cluster_start_sector);
            
            // 读取簇的所有扇区
            for (int s = 0; s < fat32_volume.sec_per_cluster; s++) {
                virtio_blk_read_sector(cluster_start_sector + s, fat32_buf);
                
                // 复制到用户缓冲区
                uint32_t to_copy = VIRTIO_BLK_SECTOR_SIZE;
                if (total_bytes + to_copy > len) {
                    to_copy = len - total_bytes;
                }
                
                if (to_copy > 0) {
                    memcpy(dest + total_bytes, fat32_buf, to_copy);
                    total_bytes += to_copy;
                    
                    printk("  Sector %d: copied %u bytes, total %u bytes\n", 
                           s, to_copy, total_bytes);
                }
                
                if (total_bytes >= len) {
                    file->cfo = total_bytes;
                    return total_bytes;
                }
            }
            
            current_cluster = next_cluster(current_cluster);
        }
        
        file->cfo = total_bytes;
        return total_bytes;
    }
    
    return 0;
}
int64_t fat32_write(struct file* file, const void* buf, uint64_t len) {
    if (file->fat32_file.cluster == 0) {
        printk("fat32_write: file not opened\n");
        return 0;
    }
    
    printk("fat32_write: cluster=%u, cfo=%lu, len=%lu\n",
           file->fat32_file.cluster, file->cfo, len);
    
    uint32_t current_cluster = file->fat32_file.cluster;
    int64_t bytes_written = 0;
    const uint8_t *src = (const uint8_t*)buf;
    
    // 计算每个簇的字节数
    uint32_t bytes_per_cluster = fat32_volume.sec_per_cluster * VIRTIO_BLK_SECTOR_SIZE;
    
    // 计算从哪个簇开始写
    uint32_t start_cluster_index = file->cfo / bytes_per_cluster;
    uint32_t start_cluster_offset = file->cfo % bytes_per_cluster;
    
    printk("bytes_per_cluster=%u, start_cluster_index=%u, start_cluster_offset=%u\n",
           bytes_per_cluster, start_cluster_index, start_cluster_offset);
    
    // 跳到起始簇
    for (uint32_t i = 0; i < start_cluster_index; i++) {
        current_cluster = next_cluster(current_cluster);
        if (current_cluster >= 0x0FFFFFF8) {
            printk("fat32_write: need to extend file (not implemented)\n");
            return -1;
        }
        printk("Moving to cluster %u for write\n", current_cluster);
    }
    
    // 开始写入
    while (bytes_written < len) {
        if (current_cluster >= 0x0FFFFFF8 || current_cluster < 2) {
            printk("fat32_write: end of file chain reached\n");
            break;
        }
        
        // 获取当前簇的起始扇区
        uint64_t cluster_start_sector = cluster_to_sector(current_cluster);
        printk("Writing to cluster %u at sector %lu\n", current_cluster, cluster_start_sector);
        
        // 计算簇内还有多少空间
        uint32_t bytes_left_in_cluster = bytes_per_cluster - start_cluster_offset;
        uint32_t bytes_to_write = len - bytes_written;
        if (bytes_to_write > bytes_left_in_cluster) {
            bytes_to_write = bytes_left_in_cluster;
        }
        
        // 如果要写入0字节，跳出循环
        if (bytes_to_write == 0) break;
        
        // 写入数据（可能跨多个扇区）
        uint32_t bytes_written_from_this_cluster = 0;
        while (bytes_written_from_this_cluster < bytes_to_write) {
            // 计算当前扇区
            uint32_t sector_offset_in_cluster = start_cluster_offset / VIRTIO_BLK_SECTOR_SIZE;
            uint64_t sector_to_write = cluster_start_sector + sector_offset_in_cluster;
            
            // 计算扇区内偏移和要写入的字节数
            uint32_t offset_in_sector = start_cluster_offset % VIRTIO_BLK_SECTOR_SIZE;
            uint32_t bytes_in_sector = VIRTIO_BLK_SECTOR_SIZE - offset_in_sector;
            uint32_t bytes_needed = bytes_to_write - bytes_written_from_this_cluster;
            
            // 计算实际要写入的字节数
            uint32_t write_this_sector = (bytes_in_sector < bytes_needed) ? 
                                         bytes_in_sector : bytes_needed;
            
            printk("Writing sector %lu, offset %u, bytes %u\n",
                   sector_to_write, offset_in_sector, write_this_sector);
            
            // 读取扇区（如果只写部分数据，需要先读取整个扇区）
            virtio_blk_read_sector(sector_to_write, fat32_buf);
            
            // 复制数据到缓冲区
            memcpy(fat32_buf + offset_in_sector, 
                   src + bytes_written + bytes_written_from_this_cluster,
                   write_this_sector);
            
            // 写回磁盘
            virtio_blk_write_sector(sector_to_write, fat32_buf);
            
            // 更新计数器
            bytes_written_from_this_cluster += write_this_sector;
            start_cluster_offset += write_this_sector;
        }
        
        bytes_written += bytes_written_from_this_cluster;
        
        // 重置簇内偏移（对于下一个簇）
        start_cluster_offset = 0;
        
        // 移动到下一个簇
        current_cluster = next_cluster(current_cluster);
    }
    
    return bytes_written;
}