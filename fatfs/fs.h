#ifndef COMPONENTS_FATFS_FS_H_
#define COMPONENTS_FATFS_FS_H_

#include "ff.h"

struct dirent {
    int d_ino;          /*!< file number */
    uint8_t d_type;     /*!< not defined in POSIX, but present in BSD and Linux */
    char d_name[256];   /*!< zero-terminated file name */
    FILINFO finfo;
};

struct ffblk {
    uint8_t attr;
    DIR dir;
    FILINFO finfo;
};

#ifndef SEEK_CUR
    #define SEEK_CUR    1
#endif

#ifndef SEEK_END
    #define SEEK_END    2
#endif

#ifndef SEEK_SET
    #define SEEK_SET    0
#endif

#ifndef EOF
    #define EOF         (-1)
#endif

#ifndef FA_RDONLY
    #define FA_RDONLY 0x01
#endif

#ifndef FA_HIDDEN
    #define FA_HIDDEN 0x02
#endif

#ifndef FA_SYSTEM
    #define FA_SYSTEM 0x04
#endif

//#define FA_LABEL  0x08

#ifndef FA_DIREC
    #define FA_DIREC  0x10
#endif

#ifndef FA_ARCH
    #define FA_ARCH   0x20
#endif

typedef int mode_t;
typedef _off_t off_t;

#define FILE                FIL
#define opendir(a)          fs_opendir(a)
#define closedir(a)         fs_closedir(a)
#define readdir(a)          fs_readdir(a)
#define findfirst(a,b,c)    fs_findfirst(a,b,c)
#define findnext(a)         fs_findnext(a)
#define fopen(a,b)          fs_fopen(a,b)
#define fclose(a)           fs_fclose(a)
#define fread(a,b,c,d)      fs_fread(a,b,c,d)
#define fwrite(a,b,c,d)     fs_fwrite(a,b,c,d)
#define fseek(a,b,c)        fs_fseek(a,b,c)
#define rewind(a)           fseek(a, 0, SEEK_SET)
#define ftruncate(a,b)      fs_ftruncate(a,b)
#define fflush(a)           fs_fflush(a)
#define fgets(a,b,c)        fs_fgets(a,b,c)
#define fputc(a,b)          fs_fputc(a,b)
#define fputs(a,b)          f_puts(a,b)
#define fprintf(a,b,...)    f_printf(a,b,##__VA_ARGS__)
#define ftell(a)            f_tell(a)
#define feof(a)             f_eof(a)
#define fsize(a)            f_size(a)
#define stat(a,b)           f_stat(a,b)
#define unlink(a)           fs_unlink(a)
#define rename(a,b)         f_rename(a,b)
#define chmod(a,b)          fs_chmod(a,b)
#define mkdir(a)            f_mkdir(a)
#define chdir(a)            f_chdir(a)
#define getcwd(a,b)         fs_getcwd(a,b)

DIR *fs_opendir(const char *dir_name);
int fs_closedir(DIR *dir);
struct dirent *fs_readdir(DIR *dir);
int fs_findfirst(const char *name, struct ffblk *f, uint8_t attr);
int fs_findnext(struct ffblk *f);
FILE *fs_fopen(const char *name, const char *mode);
int fs_fclose(FILE *file);
size_t fs_fread(void *ptrvoid, size_t size, size_t count, FILE *file);
size_t fs_fwrite(const void *ptrvoid, size_t size, size_t count, FILE *file);
int fs_fseek(FILE *file, size_t offset, int origin);
void fs_rewind(FILE *file);
int fs_ftruncate(FILE *file, off_t length);
int fs_fflush(FILE *file);
char *fs_fgets(char *string, int num, FILE *file);
int fs_fputc(int character, FILE *file);
int fs_unlink(const char *name);
int fs_chmod(const char *name, mode_t mode);
char *fs_getcwd(char *dir, int len);

#endif /* COMPONENTS_FATFS_FS_H_ */
