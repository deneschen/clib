#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// 使用 pragma pack(1) 确保编译器按1字节对齐
#pragma pack(1)

/**
 * @brief 定义16字节的MBR分区条目结构
 */
typedef struct {
    uint8_t status;             // 0x80 = 可引导, 0x00 = 不可引导
    uint8_t start_head;         // 起始磁头号 (CHS)
    uint16_t start_sector_cyl;  // 起始扇区和柱面号 (CHS)
    uint8_t type;               // 分区类型ID (例如: 0x83 for Linux)
    uint8_t end_head;           // 结束磁头号 (CHS)
    uint16_t end_sector_cyl;    // 结束扇区和柱面号 (CHS)
    uint32_t start_lba;         // 起始LBA (Logical Block Address)
    uint32_t total_sectors;     // 分区总扇区数
} MBRPartitionEntry;

/**
 * @brief 定义512字节的MBR扇区结构
 */
typedef struct {
    uint8_t bootstrap_code[446];             // 引导代码
    MBRPartitionEntry partitions[4];         // 4个分区条目 (4 * 16 = 64字节)
    uint16_t boot_signature;                 // 启动签名 (必须是 0xAA55)
} MBRSector;

// 恢复默认的内存对齐设置
#pragma pack()

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <磁盘镜像文件>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];

    // 1. 打开文件
    FILE *fp = fopen(filename, "rb"); // 以只读二进制模式打开
    if (!fp) {
        perror("无法打开文件");
        return 1;
    }

    // 2. 读取前512字节到MBR结构体
    MBRSector mbr;
    size_t bytes_read = fread(&mbr, 1, sizeof(MBRSector), fp);

    if (bytes_read != sizeof(MBRSector)) {
        fprintf(stderr, "读取MBR失败或文件大小不足512字节\n");
        fclose(fp);
        return 1;
    }

    fclose(fp);

    // 3. 校验启动签名
    if (mbr.boot_signature != 0xAA55) {
        fprintf(stderr, "无效的MBR签名: 0x%04X\n", mbr.boot_signature);
        return 1;
    }

    printf("成功读取MBR，启动签名为 0x%04X\n", mbr.boot_signature);
    printf("------------------------------------------------------------------\n");
    printf("分区\t可引导\t类型\t起始LBA\t\t总扇区数\t大小 (MB)\n");
    printf("------------------------------------------------------------------\n");

    // 4. 遍历并解析4个分区条目
    for (int i = 0; i < 4; i++) {
        MBRPartitionEntry p = mbr.partitions[i];

        // 如果分区类型为0，则为空分区条目，跳过
        if (p.type == 0x00) {
            continue;
        }
        
        // 假设扇区大小为512字节
        double size_mb = (double)p.total_sectors * 512.0 / (1024.0 * 1024.0);

        printf("P%d\t%s\t0x%02X\t%-10u\t%-10u\t%.2f\n",
               i + 1,
               p.status == 0x80 ? "是" : "否",
               p.type,
               p.start_lba,
               p.total_sectors,
               size_mb);
    }
    printf("------------------------------------------------------------------\n");

    return 0;
}
