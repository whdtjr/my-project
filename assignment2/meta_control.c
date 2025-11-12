#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SECTOR_SIZE 512
#define META_FILE "meta.ini"

typedef struct {
    uint64_t bank;
    uint64_t block;
    uint64_t page;
    uint64_t page_size;
    uint64_t provision_rate;
} MetaData;


int parsing_meta(const char* filename, MetaData* meta);
uint64_t calc_disk_capa(const MetaData* meta);
uint64_t calc_sector(uint64_t disk_capacity);
uint64_t calc_practical_usage_sector(uint64_t total_sectors, uint64_t provision_rate);
void print_data(const MetaData* meta);
void create_meta_file(const char* filename);
void modify_meta_file(const char* filename);
void show_menu();

int main() {
    int choice;

    while (1) {
        show_menu();

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }

        while (getchar() != '\n');

        switch (choice) {
            case 1: {
                MetaData meta;
                if (parsing_meta(META_FILE, &meta) == 0) {
                    print_data(&meta);
                } else {
                    printf("Failed to read meta.ini. Creating new file...\n");
                    create_meta_file(META_FILE);
                }
                break;
            }
            case 2:
                modify_meta_file(META_FILE);
                break;
            case 3:
                printf("Exiting program...\n");
                return 0;
            default:
                break;
        }
    }

    return 0;
}

void show_menu() {
    printf("\n=== Meta Control Program ===\n");
    printf("1. Display disk information\n");
    printf("2. Modify meta.ini\n");
    printf("3. Exit\n");
    printf("Enter a option: ");
}

int parsing_meta(const char* filename, MetaData* meta) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        return -1;
    }

    char line[256];
    int fields_found = 0;

    meta->bank = 0;
    meta->block = 0;
    meta->page = 0;
    meta->page_size = 0;
    meta->provision_rate = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        if (sscanf(line, "bank=%lu", &meta->bank) == 1) {
            fields_found++;
        } else if (sscanf(line, "block=%lu", &meta->block) == 1) {
            fields_found++;
        } else if (sscanf(line, "page=%lu", &meta->page) == 1) {
            fields_found++;
        } else if (sscanf(line, "page_size=%lu", &meta->page_size) == 1) {
            fields_found++;
        } else if (sscanf(line, "PROVISION_RATE=%lu", &meta->provision_rate) == 1) {
            fields_found++;
        }
    }

    fclose(fp);

    if (fields_found != 5) {
        return -1;
    }

    return 0;
}

uint64_t calc_disk_capa(const MetaData* meta) {
    // Total capacity = page_size * page * block * bank
    return meta->page_size * meta->page * meta->block * meta->bank;
}

uint64_t calc_sector(uint64_t disk_capacity) {
    // Total sectors = disk_capacity / SECTOR_SIZE
    return disk_capacity / SECTOR_SIZE;
}

uint64_t calc_practical_usage_sector(uint64_t total_sectors, uint64_t provision_rate) {
    uint64_t op_sectors = ((100 - provision_rate) * total_sectors) / 100;
    return total_sectors - op_sectors;
}

void print_data(const MetaData* meta) {
    printf("\n=== Disk Information ===\n");
    printf("Bank: %lu\n", meta->bank);
    printf("Block: %lu\n", meta->block);
    printf("Page: %lu\n", meta->page);
    printf("Page Size: %lu bytes\n", meta->page_size);
    printf("Provision Rate: %lu%%\n", meta->provision_rate);

    uint64_t disk_capacity = calc_disk_capa(meta);
    uint64_t total_sectors = calc_sector(disk_capacity);
    uint64_t practical_sectors = calc_practical_usage_sector(total_sectors, meta->provision_rate);

    // Calculate capacities in different units
    double capacity_kb = (double)disk_capacity / 1024.0;
    double capacity_gb = capacity_kb / 1024.0 / 1024.0;

    uint64_t op_sectors = total_sectors - practical_sectors;
    uint64_t op_capacity = op_sectors * SECTOR_SIZE;
    double op_capacity_kb = (double)op_capacity / 1024.0;
    double op_capacity_gb = op_capacity_kb / 1024.0 / 1024.0;

    uint64_t practical_capacity = practical_sectors * SECTOR_SIZE;
    double practical_capacity_kb = (double)practical_capacity / 1024.0;
    double practical_capacity_gb = practical_capacity_kb / 1024.0 / 1024.0;

    printf("\n=== Capacity Information ===\n");
    printf("Total Disk Capacity: %.2f GB (%.2f KB)\n", capacity_gb, capacity_kb);
    printf("Total Sectors: %lu\n", total_sectors);
    printf("\nOP Area Capacity: %.2f GB (%.2f KB)\n", op_capacity_gb, op_capacity_kb);
    printf("OP Area Sectors: %lu\n", op_sectors);
    printf("\nPractical Usage Capacity: %.2f GB (%.2f KB)\n", practical_capacity_gb, practical_capacity_kb);
    printf("Practical Usage Sectors: %lu\n", practical_sectors);
}

void create_meta_file(const char* filename) {
    MetaData meta;

    printf("\n=== Create Meta File ===\n");
    printf("Enter bank count: ");
    scanf("%lu", &meta.bank);

    printf("Enter block count: ");
    scanf("%lu", &meta.block);

    printf("Enter page count: ");
    scanf("%lu", &meta.page);

    printf("Enter page size (bytes): ");
    scanf("%lu", &meta.page_size);

    printf("Enter provision rate (%%): ");
    scanf("%lu", &meta.provision_rate);

    while (getchar() != '\n');

    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("Failed to create meta file.\n");
        return;
    }

    fprintf(fp, "# bank\n");
    fprintf(fp, "bank=%lu\n", meta.bank);
    fprintf(fp, "block=%lu\n", meta.block);
    fprintf(fp, "page=%lu\n", meta.page);
    fprintf(fp, "# bytes\n");
    fprintf(fp, "page_size=%lu\n", meta.page_size);
    fprintf(fp, "PROVISION_RATE=%lu\n", meta.provision_rate);

    fclose(fp);

    printf("Meta file created successfully!\n");
}

void modify_meta_file(const char* filename) {
    MetaData meta;

    printf("\n=== Modify Meta File ===\n");
    printf("Enter bank count: ");
    scanf("%lu", &meta.bank);

    printf("Enter block count: ");
    scanf("%lu", &meta.block);

    printf("Enter page count: ");
    scanf("%lu", &meta.page);

    printf("Enter page size (bytes): ");
    scanf("%lu", &meta.page_size);

    printf("Enter provision rate (%%): ");
    scanf("%lu", &meta.provision_rate);

    while (getchar() != '\n');

    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("Failed to modify meta file.\n");
        return;
    }

    fprintf(fp, "# bank\n");
    fprintf(fp, "bank=%lu\n", meta.bank);
    fprintf(fp, "block=%lu\n", meta.block);
    fprintf(fp, "page=%lu\n", meta.page);
    fprintf(fp, "# bytes\n");
    fprintf(fp, "page_size=%lu\n", meta.page_size);
    fprintf(fp, "PROVISION_RATE=%lu\n", meta.provision_rate);

    fclose(fp);

    printf("Meta file modified successfully!\n");
}
