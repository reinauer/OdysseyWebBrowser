#define _DEFAULT_SOURCE 1
#include <sys/stat.h>
#include <sys/time.h>
#include <utime.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef FILE_SMOKE_VOLUME
#define FILE_SMOKE_VOLUME "RAM:"
#endif

static unsigned checks, failures;
#define CHECK(condition) do { ++checks; if (!(condition)) { ++failures; \
    printf("FAIL line %u: %s (errno=%d)\n", __LINE__, #condition, errno); } } while (0)

int main(void)
{
    const char* directory = FILE_SMOKE_VOLUME "Odyssey-file-smoke";
    const char* target = FILE_SMOKE_VOLUME "Odyssey-file-smoke/target";
    const char* hard = FILE_SMOKE_VOLUME "Odyssey-file-smoke/hard";
    const char* soft = FILE_SMOKE_VOLUME "Odyssey-file-smoke/soft";
    const char* broken = FILE_SMOKE_VOLUME "Odyssey-file-smoke/broken";
    const char* missing = FILE_SMOKE_VOLUME "Odyssey-file-smoke/missing";
    struct stat value, reference;
    char contents[32] = { 0 };
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Native filesystem test on %s\n", FILE_SMOKE_VOLUME);
    errno = 0;
    CHECK(!isatty(STDOUT_FILENO) && errno == ENOTTY); /* Runner redirects stdout. */
    CHECK(!mkdir(directory, 0700));
    char* originalDirectory = getcwd(NULL, 0);
    CHECK(originalDirectory != NULL);
    char smallDirectory[1];
    errno = 0;
    CHECK(!getcwd(smallDirectory, sizeof(smallDirectory)) && errno == ERANGE);
    CHECK(!chdir(directory));
    char* currentDirectory = getcwd(NULL, 0);
    char* expectedDirectory = realpath(directory, NULL);
    CHECK(currentDirectory && expectedDirectory && !strcmp(currentDirectory, expectedDirectory));
    if (originalDirectory)
        CHECK(!chdir(originalDirectory));
    free(expectedDirectory);
    free(currentDirectory);
    free(originalDirectory);
    int fd = open(target, O_CREAT | O_EXCL | O_RDWR, 0600);
    CHECK(fd >= 0);
    if (fd < 0) return 20;
    CHECK(write(fd, "native-links", 12) == 12);
    CHECK(!fstat(fd, &reference) && S_ISREG(reference.st_mode) && reference.st_size == 12);
    CHECK(!stat(target, &value) && S_ISREG(value.st_mode) && value.st_size == 12);
    CHECK(!lstat(target, &value) && S_ISREG(value.st_mode) && value.st_size == 12);
    CHECK(!isatty(fd) && errno == ENOTTY);
    errno = 0;
    CHECK(!isatty(-1) && errno == EBADF);
    CHECK(!ftruncate(fd, 24));
    CHECK(lseek(fd, 12, SEEK_SET) == 12);
    CHECK(read(fd, contents, 12) == 12 && !memcmp(contents, "\0\0\0\0\0\0\0\0\0\0\0\0", 12));
    CHECK(!fstat(fd, &value) && value.st_size == 24);
    CHECK(lseek(fd, 0, SEEK_SET) == 0);
    CHECK(!ftruncate(fd, 12));
    CHECK(!close(fd));
    struct timeval now;
    CHECK(!gettimeofday(&now, NULL));
    struct timeval dates[2] = { now, now };
    CHECK(!utimes(target, dates));
    CHECK(!stat(target, &reference) && reference.st_mtime <= now.tv_sec && now.tv_sec-reference.st_mtime <= 1);
    // Force a nonzero local/UTC offset independently of the fixture Locale.
    // SDK utime previously used a different conversion from native stat.
    const char* previousDST = getenv("DST_OFFSET");
    char* savedDST = previousDST ? strdup(previousDST) : NULL;
    CHECK(!setenv("DST_OFFSET", "3600", 1));
    struct utimbuf secondsOnly = { now.tv_sec - 1234, now.tv_sec - 4321 };
    CHECK(!utime(target, &secondsOnly));
    CHECK(!stat(target, &value) && value.st_mtime == secondsOnly.modtime);
    dates[1].tv_sec = now.tv_sec - 789;
    dates[1].tv_usec = 500000;
    CHECK(!utimes(target, dates));
    CHECK(!stat(target, &value) && value.st_mtime == dates[1].tv_sec);
    CHECK(!utime(target, NULL));
    CHECK(!gettimeofday(&now, NULL));
    CHECK(!stat(target, &value) && value.st_mtime <= now.tv_sec && now.tv_sec - value.st_mtime <= 1);
    if (savedDST) {
        CHECK(!setenv("DST_OFFSET", savedDST, 1));
        free(savedDST);
    } else
        CHECK(!unsetenv("DST_OFFSET"));
    CHECK(!link(target, hard));
    CHECK(!lstat(hard, &value) && S_ISREG(value.st_mode) && value.st_size == 12);
    CHECK(!symlink(target, soft));
    CHECK(!lstat(soft, &value) && S_ISLNK(value.st_mode));
    printf("Soft link size=%ld target bytes=%lu\n", (long)value.st_size, (unsigned long)strlen(target));
    CHECK(value.st_size == strlen(target));
    CHECK(!stat(soft, &value) && S_ISREG(value.st_mode) && value.st_size == 12);
    fd = open(target, O_WRONLY | O_TRUNC);
    CHECK(fd >= 0 && write(fd, "native-links", 12) == 12);
    CHECK(!stat(soft, &value) && S_ISREG(value.st_mode) && value.st_size == 12);
    CHECK(!lstat(soft, &value) && S_ISLNK(value.st_mode));
    if (fd >= 0) CHECK(!close(fd));
    CHECK(!symlink(missing, broken));
    CHECK(!lstat(broken, &value) && S_ISLNK(value.st_mode) && value.st_size == strlen(missing));
    errno = 0;
    CHECK(stat(broken, &value) == -1 && errno == ENOENT);
    CHECK(!lstat(directory, &value) && S_ISDIR(value.st_mode));
    errno = 0;
    CHECK(lstat(missing, &value) == -1 && errno == ENOENT);
    errno = 0;
    CHECK(link(target, hard) == -1 && errno == EEXIST);
    errno = 0;
    CHECK(link(missing, FILE_SMOKE_VOLUME "no-link-created") == -1 && errno == ENOENT);
    CHECK(!unlink(target));
    fd = open(hard, O_RDONLY);
    CHECK(fd >= 0 && read(fd, contents, 12) == 12 && !memcmp(contents, "native-links", 12));
    if (fd >= 0) CHECK(!close(fd));
    CHECK(!lstat(soft, &value) && S_ISLNK(value.st_mode));
    errno = 0;
    CHECK(stat(soft, &value) == -1 && errno == ENOENT);
    CHECK(!unlink(soft));
    CHECK(!unlink(broken));
    CHECK(!unlink(hard));
    CHECK(!rmdir(directory));
    printf("Native filesystem: %u checks, %u failures\n", checks, failures);
    if (!failures) puts("PASS native filesystem links and metadata");
    return failures ? 20 : 0;
}
