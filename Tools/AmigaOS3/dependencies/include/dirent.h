/* Minimal POSIX directory interface backed by classic AmigaDOS. */
#ifndef ODYSSEY_AMIGAOS3_DIRENT_H
#define ODYSSEY_AMIGAOS3_DIRENT_H
#ifdef __cplusplus
extern "C" {
#endif
#define DT_UNKNOWN 0
#define DT_DIR 4
#define DT_REG 8
struct dirent {
    unsigned long d_ino;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[108];
};
typedef struct OdysseyDirectory DIR;
DIR* opendir(const char*);
struct dirent* readdir(DIR*);
int closedir(DIR*);
void rewinddir(DIR*);
#ifdef __cplusplus
}
#endif
#endif
