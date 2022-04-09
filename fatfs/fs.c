#include "osapi.h"
#include "mem.h"
#include "fs.h"

DIR ICACHE_FLASH_ATTR *fs_opendir(const char *dir_name) {
    DIR *dir;

    dir = os_malloc(sizeof(DIR));

    if (!dir) return NULL;

    if (f_opendir(dir, dir_name) == FR_OK) return dir;

    os_free(dir);
    return NULL;
}

int ICACHE_FLASH_ATTR fs_closedir(DIR *dir) {
    if (dir) {
        f_closedir(dir);
        os_free(dir);
        return 0;
    }
    return EOF;
}

struct dirent ICACHE_FLASH_ATTR *fs_readdir(DIR *dir) {

    static struct dirent dr;

    if (f_readdir(dir, &(dr.finfo)) == FR_OK && dr.finfo.fname[0] != 0) {
        os_strcpy(dr.d_name, dr.finfo.fname);
        return &dr;
    }

    return NULL;
}

int fs_findfirst(const char *name, struct ffblk *f, uint8_t attr) {

    f->attr = attr;

    if (f_findfirst(&(f->dir), &(f->finfo), "", name) == FR_OK && f->finfo.fname[0] != 0) {
        if (f->attr == 0) return 0;
        if (f->attr & f->finfo.fattrib) return 0;
        return fs_findnext(f);
    }

    return EOF;
}

int fs_findnext(struct ffblk *f) {

    while(f_findnext(&(f->dir), &(f->finfo)) == FR_OK && f->finfo.fname[0] != 0) {
        if (f->attr == 0) return 0;
        if (f->attr & f->finfo.fattrib) return 0;
    }

    return EOF;
}

FILE ICACHE_FLASH_ATTR *fs_fopen(const char *name, const char *mode) {

    FILE *file;
    uint8_t fmode = 0;

    file = os_malloc(sizeof(FILE));

    if (!file) return NULL;

    if (mode) {
        if (strchr(mode, 'r')) {
            fmode |= FA_READ;
            if (strchr(mode, '+')) {
                fmode |= FA_WRITE;
            }
        } else if (strchr(mode, 'w')) {
            if (strchr(mode, 'x')) {
                fmode |= FA_CREATE_NEW | FA_WRITE;
            } else {
                fmode |= FA_CREATE_ALWAYS | FA_WRITE;
            }
            if (strchr(mode, '+')) {
                fmode |= FA_READ;
            }
        } else if (strchr(mode, 'a')) {
            fmode |= FA_OPEN_APPEND | FA_WRITE;
            if (strchr(mode, '+')) {
                fmode |= FA_READ;
            }
        } else {
            fmode = 0;
        }

        if (fmode) {
            if (f_open(file, name, fmode) == FR_OK) {
                return file;
            }
        }
    }

    os_free(file);
    return NULL;
}

int ICACHE_FLASH_ATTR fs_fclose(FILE *file) {

    int ret = EOF;

    if (f_close(file) == FR_OK) {
        ret = 0;
        os_free(file);
    }

    return ret;
}

size_t ICACHE_FLASH_ATTR fs_fread(void *ptrvoid, size_t size, size_t count, FILE *file) {
    size_t out_len;

    if (f_read(file, ptrvoid, size*count, &out_len) == FR_OK) {
        return out_len;
    }

    return 0;
}

size_t ICACHE_FLASH_ATTR fs_fwrite(const void *ptrvoid, size_t size, size_t count, FILE *file) {
    size_t out_len;

    if(f_write(file, ptrvoid, size*count, &out_len) == FR_OK) {
        return out_len;
    }

    return 0;
}

int ICACHE_FLASH_ATTR fs_fseek(FILE *file, size_t offset, int origin) {

    size_t pos;

    switch (origin) {
        case SEEK_SET:
            pos = 0;
            break;
        case SEEK_CUR:
            pos = f_tell(file);
            break;
        case SEEK_END:
            pos = f_size(file);
            break;
        default:
            pos = 0;
            break;
    }

    pos += offset;

    if (f_lseek(file, pos) == FR_OK) {
        return 0;
    }

    return EOF;
}

int ICACHE_FLASH_ATTR fs_ftruncate(FILE *file, off_t length) {
    if (f_lseek(file, length) == FR_OK)
        if (f_truncate(file) == FR_OK)
            return 0;

    return EOF;
}

int ICACHE_FLASH_ATTR fs_fflush(FILE *file) {
    if (f_sync(file) == FR_OK)
        return 0;

    return EOF;
}

char *fs_fgets(char *string, int num, FILE *file) {

    return f_gets(string, num, file);
}

int fs_fputc(int character, FILE *file) {

    if (f_putc(character, file) == 1) return character;

    return EOF;
}

int ICACHE_FLASH_ATTR fs_unlink(const char *name) {
    if (f_unlink(name) == FR_OK)
        return 0;

    return EOF;
}

int fs_chmod(const char *name, mode_t mode) {

    if (f_chmod(name, mode , AM_ARC | AM_RDO  | AM_HID | AM_SYS) == FR_OK)
        return 0;

    return EOF;
}

char *fs_getcwd(char *dir, int len) {
    char *pdir;
    if (len) {
        if (dir == NULL) {
            pdir = os_malloc(len+1);
            if (pdir == NULL) return NULL;
            dir = pdir;
        }
        if (f_getcwd(dir, len) == FR_OK) return dir;
    }

    return NULL;
}
