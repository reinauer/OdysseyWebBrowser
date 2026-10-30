/* Real library operations for the cross-built graphics/XML dependency chain. */
#include <cairo.h>
#include <fontconfig/fontconfig.h>
#include <libxml/parser.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

static unsigned pngSize;
static cairo_status_t pngWrite(void* closure, const unsigned char* data, unsigned length)
{
    (void)closure;
    if (!pngSize && (length < 8 || memcmp(data, "\211PNG\r\n\032\n", 8)))
        return CAIRO_STATUS_WRITE_ERROR;
    pngSize += length;
    return CAIRO_STATUS_SUCCESS;
}

int main(void)
{
    const char* xml = "<root><value>68</value></root>";
    const char* xsl = "<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'><xsl:template match='/'><result><xsl:value-of select='root/value + 20'/></result></xsl:template></xsl:stylesheet>";
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4);
    cairo_t* context = cairo_create(surface);
    cairo_set_source_rgb(context, 1, 0, 0);
    cairo_paint(context);
    cairo_surface_flush(surface);
    unsigned char* pixel = cairo_image_surface_get_data(surface);
    if (cairo_status(context) || pixel[0] != 255 || pixel[1] != 255 || pixel[2] || pixel[3])
        return 1;
    if (cairo_surface_write_to_png_stream(surface, pngWrite, NULL) || pngSize < 24)
        return 2;
    cairo_destroy(context);
    cairo_surface_destroy(surface);
    puts("PASS cairo/pixman ARGB rendering and PNG encoding");

    FcPattern* pattern = FcNameParse((const FcChar8*)"sans:size=12");
    double size = 0;
    if (!pattern || FcPatternGetDouble(pattern, FC_SIZE, 0, &size) != FcResultMatch || size != 12)
        return 3;
    FcPatternDestroy(pattern);
    puts("PASS fontconfig pattern parsing");

    xmlDocPtr input = xmlReadMemory(xml, strlen(xml), "input.xml", NULL, 0);
    xmlDocPtr styleDoc = xmlReadMemory(xsl, strlen(xsl), "style.xsl", NULL, 0);
    xsltStylesheetPtr style = xsltParseStylesheetDoc(styleDoc);
    xmlDocPtr output = style && input ? xsltApplyStylesheet(style, input, NULL) : NULL;
    xmlChar* result = output ? xmlNodeGetContent(xmlDocGetRootElement(output)) : NULL;
    if (!result || strcmp((const char*)result, "88"))
        return 4;
    xmlFree(result);
    xmlFreeDoc(output);
    xsltFreeStylesheet(style);
    xmlFreeDoc(input);
    puts("PASS libxml2 parsing and libxslt transformation");

    DIR* dir = opendir("FONTS:");
    if (!dir)
        return 5;
    unsigned entries = 0;
    errno = 0;
    while (readdir(dir))
        ++entries;
    if (errno || !entries || closedir(dir))
        return 6;
    printf("PASS native directory enumeration (%u entries)\n", entries);
    const char* filename = "T:Odyssey-dependency-smoke.tmp";
    int fd = open(filename, O_CREAT | O_TRUNC | O_RDWR, 0600);
    struct stat value;
    struct timeval now;
    if (fd < 0 || write(fd, "native-stat", 11) != 11 || fstat(fd, &value)
        || !S_ISREG(value.st_mode) || value.st_size != 11)
        return 7;
    gettimeofday(&now, NULL);
    struct timeval dates[2] = { now, now };
    int dateResult = utimes(filename, dates);
    int dateErrno = errno;
    int statResult = stat(filename, &value);
    printf("metadata: utimes=%d errno=%d stat=%d errno=%d size=%ld now=%ld mtime=%ld\n",
        dateResult, dateErrno, statResult, errno, (long)value.st_size,
        (long)now.tv_sec, (long)value.st_mtime);
    if (dateResult || statResult
        || value.st_size != 11 || value.st_mtime > now.tv_sec
        || now.tv_sec - value.st_mtime > 1)
        return 8;
    struct flock lock = { 0 };
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_len = 1;
    if (fcntl(fd, F_SETLK, &lock))
        return 9;
    lock.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &lock))
        return 10;
    close(fd);
    unlink(filename);
    errno = 0;
    if (!stat(filename, &value) || errno != ENOENT)
        return 11;
    errno = 0;
    if (!fstat(-1, &value) || errno != EBADF)
        return 12;
    puts("PASS native file metadata, timestamps, locks and error handling");
    puts("PASS dependency smoke");
    return 0;
}
