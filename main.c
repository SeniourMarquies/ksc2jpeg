
    // Project: KSC to JPEG Converter (cross-platform)
    // Author: Seniour marquies (+ cross-platform QoL)
    // Date: 24-07-2025
    // License: GNU GPL v3.0

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#if defined(_WIN32)
#include <Windows.h>
#include <Shellapi.h>   // for ShellExecuteA
#include <io.h>

#pragma comment(lib, "Shell32.lib")
#define PATH_SEP '\\'
#define ALT_PATH_SEP '/'
#define PATH_MAX_LOCAL MAX_PATH
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <strings.h>    // for strcasecmp
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define PATH_MAX_LOCAL PATH_MAX
#define PATH_SEP '/'
#define ALT_PATH_SEP '\\'
#endif

// ---------------- Decrypt PRNG ----------------
static uint16_t m_r = 1124;
static const uint16_t m_c1 = 52845;
static const uint16_t m_c2 = 22719;

static inline uint8_t Decrypt(uint8_t cipher) {
    uint8_t plain = (uint8_t)(cipher ^ (m_r >> 8));
    m_r = (uint16_t)(((uint16_t)cipher + m_r) * m_c1 + m_c2);
    return plain;
}
static inline void reset_rng(void) { m_r = 1124; }

// -------------- CLI options -------------------
typedef struct {
    int recursive;                            // -r
    int auto_open;                            // --open
    int quiet;                                // -q
    char start_dir[PATH_MAX_LOCAL];           // -r <dir>
    char out_dir[PATH_MAX_LOCAL];             // -o <outdir>
    char log_path[PATH_MAX_LOCAL];            // --log <file>
    char single_in[PATH_MAX_LOCAL];           // positional
    char single_out[PATH_MAX_LOCAL];          // positional optional
} Options;

static void print_usage(const char* exe) {
    fprintf(stderr,
        "Usage:\n"
        "  %s <input.ksc> [output.jpeg] [--open] [--log <file>]\n"
        "  %s -r [<dir>] [-o <outdir>] [--open] [--log <file>] [-q]\n"
        "\nOptions:\n"
        "  -r [dir]        Recursively convert all .ksc from start directory (default: current dir)\n"
        "  -o <outdir>     Write JPEGs into this directory, preserving relative structure\n"
        "  --open          Auto-open each converted JPEG\n"
        "  --log <file>    Append progress to a log file (default: ksc2jpeg.log)\n"
        "  -q              Quiet mode (less console output)\n",
        exe, exe);
}

// -------- Platform shims (paths, FS, timers, opener) --------
static int is_dir_sep(char c) { return c == PATH_SEP || c == ALT_PATH_SEP; }

static void path_dirname(const char* in, char* out, size_t n) {
    size_t len = strlen(in);
    if (len == 0) { out[0] = 0; return; }
    size_t i = len;
    while (i > 0 && !is_dir_sep(in[i - 1])) --i;
    if (i == 0) { out[0] = 0; return; }
    size_t dlen = i - 1;
    while (dlen > 0 && is_dir_sep(in[dlen])) --dlen; // trim trailing sep run
    size_t copy = (dlen + 1 < n - 1) ? (dlen + 1) : (n - 1);
    memcpy(out, in, copy);
    out[copy] = 0;
}

static const char* path_basename(const char* in) {
    const char* p = in + strlen(in);
    while (p > in && !is_dir_sep(*(p - 1))) --p;
    return p;
}

static void path_join(const char* a, const char* b, char* out, size_t n) {
    if (!a || !a[0]) {
        snprintf(out, n, "%s", b ? b : "");
        return;
    }
    size_t la = strlen(a);
    int need_sep = (la > 0 && !is_dir_sep(a[la - 1]));
    if (b && b[0]) {
        if (need_sep)
            snprintf(out, n, "%s%c%s", a, PATH_SEP, b);
        else
            snprintf(out, n, "%s%s", a, b);
    }
    else {
        snprintf(out, n, "%s", a);
    }
}

static char* path_find_ext(char* s) {
    char* last_dot = NULL;
    for (char* p = s; *p; ++p) {
        if (*p == '.') last_dot = p;
        if (is_dir_sep(*p)) last_dot = NULL;
    }
    return last_dot;
}

static void path_replace_ext(char* path, const char* newext, size_t n) {
    char buf[PATH_MAX_LOCAL];
    strncpy(buf, path, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
    char* dot = path_find_ext(buf);
    if (!dot) {
        snprintf(path, n, "%s%s", buf, newext);
    }
    else {
        *dot = 0;
        snprintf(path, n, "%s%s", buf, newext);
    }
}

static void path_stem(const char* path, char* stem, size_t n) {
    const char* base = path_basename(path);
    strncpy(stem, base, n - 1); stem[n - 1] = 0;
    char* dot = path_find_ext(stem);
    if (dot) *dot = 0;
}

static int file_exists(const char* p) {
#if defined(_WIN32)
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(p, &st) == 0 && S_ISREG(st.st_mode));
#endif
}

static int dir_exists(const char* p) {
#if defined(_WIN32)
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(p, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

static int mkdir_one(const char* p) {
#if defined(_WIN32)
    return CreateDirectoryA(p, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    return mkdir(p, 0775) == 0 || errno == EEXIST;
#endif
}

static void ensure_parent_dir(const char* path) {
    char buf[PATH_MAX_LOCAL]; path_dirname(path, buf, sizeof(buf));
    if (!buf[0]) return;
    if (dir_exists(buf)) return;
    // create recursively
    char partial[PATH_MAX_LOCAL] = { 0 };
    size_t len = strlen(buf);
    for (size_t i = 0; i < len; ++i) {
        partial[i] = buf[i];
        if (is_dir_sep(buf[i]) || i == len - 1) {
            partial[i + 1] = 0;
            if (partial[0] && !dir_exists(partial)) mkdir_one(partial);
        }
    }
}

static double now_seconds(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq = { 0 };
    LARGE_INTEGER t;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}

static void ts_rfc3339(char* out, size_t n) {
#if defined(_WIN32)
    SYSTEMTIME st; GetLocalTime(&st);
    snprintf(out, n, "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#else
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm; localtime_r(&ts.tv_sec, &tm);
    snprintf(out, n, "%04d-%02d-%02dT%02d:%02d:%02d.%03ld",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000);
#endif
}

static void auto_open_file(const char* path) {
#if defined(_WIN32)
    ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
#else
    // Best-effort; ignore exit status
    if (fork() == 0) {
        execlp("xdg-open", "xdg-open", path, (char*)NULL);
        _exit(0);
    }
#endif
}

// -------------- Logging -------------------
static FILE* open_log(const Options * opt) {
    const char* defname = "ksc2jpeg.log";
    const char* path = opt->log_path[0] ? opt->log_path : defname;
    FILE* f = fopen(path, "ab");
    return f;
}

static void log_line(FILE * flog, const Options * opt, const char* level, const char* fmt, ...) {
    (void)opt;
    if (!flog) return;
    char timestamp[64]; ts_rfc3339(timestamp, sizeof timestamp);
    fprintf(flog, "[%s] [%s] ", timestamp, level);
    va_list ap; va_start(ap, fmt); vfprintf(flog, fmt, ap); va_end(ap);
    fputc('\n', flog);
    fflush(flog);
}

// -------------- Conversion -------------------
static int convert_ksc_to_jpeg(const char* ksc_path, const char* jpeg_path, const Options * opt, FILE * flog) {
    double t0 = now_seconds();

    FILE* f_in = fopen(ksc_path, "rb");
    if (!f_in) {
        if (!opt->quiet) fprintf(stderr, "Failed to open input file: %s\n", ksc_path);
        log_line(flog, opt, "ERROR", "open input failed: %s", ksc_path);
        return 1;
    }

    fseek(f_in, 0, SEEK_END);
    long fsize = ftell(f_in);
    rewind(f_in);
    if (fsize <= 8) {
        if (!opt->quiet) fprintf(stderr, "File too small: %s\n", ksc_path);
        log_line(flog, opt, "ERROR", "file too small: %s", ksc_path);
        fclose(f_in);
        return 1;
    }

    uint8_t* buffer = (uint8_t*)malloc((size_t)fsize);
    if (!buffer) {
        if (!opt->quiet) fprintf(stderr, "Memory allocation failed\n");
        log_line(flog, opt, "ERROR", "malloc failed: %s", ksc_path);
        fclose(f_in);
        return 1;
    }
    fread(buffer, 1, (size_t)fsize, f_in);
    fclose(f_in);

    ensure_parent_dir(jpeg_path);
    FILE* f_out = fopen(jpeg_path, "wb");
    if (!f_out) {
        if (!opt->quiet) fprintf(stderr, "Failed to open output: %s\n", jpeg_path);
        log_line(flog, opt, "ERROR", "open output failed: %s", jpeg_path);
        free(buffer);
        return 1;
    }

    // Match the basic converter exactly:
    m_r = 1124;
    for (int i = 0; i < 8; ++i)
        (void)Decrypt(buffer[i]);
    for (long i = 8; i < fsize; ++i) {
        uint8_t plain = Decrypt(buffer[i]);
        fwrite(&plain, 1, 1, f_out);
    }

    fclose(f_out);
    free(buffer);

    double secs = now_seconds() - t0;
    if (!opt->quiet) printf("Converted: %s -> %s (%.3fs)\n", ksc_path, jpeg_path, secs);
    log_line(flog, opt, "INFO", "converted: %s -> %s (%.3fs)", ksc_path, jpeg_path, secs);

    if (opt->auto_open) auto_open_file(jpeg_path);
    return 0;
}

// Build output path for a given input, honoring -o <outdir>
static void make_output_path(const char* input_path, const Options * opt, char out_path[PATH_MAX_LOCAL]) {
    char stem[PATH_MAX_LOCAL];
    path_stem(input_path, stem, sizeof(stem));

    char base[PATH_MAX_LOCAL];
    if (opt->out_dir[0]) {
        // mirror in out_dir: out_dir / stem.jpeg
        path_join(opt->out_dir, stem, base, sizeof(base));
    }
    else {
        // same folder as input
        char dir[PATH_MAX_LOCAL]; path_dirname(input_path, dir, sizeof(dir));
        path_join(dir[0] ? dir : ".", stem, base, sizeof(base));
    }

    strncpy(out_path, base, PATH_MAX_LOCAL - 1); out_path[PATH_MAX_LOCAL - 1] = 0;
    path_replace_ext(out_path, ".jpeg", PATH_MAX_LOCAL);
}

// Skip if already converted
static int output_exists_for_input(const char* input_path, const Options * opt) {
    char out_path[PATH_MAX_LOCAL];
    make_output_path(input_path, opt, out_path);
    return file_exists(out_path);
}

// Process a single file
static void process_single_file(const char* in_path, const char* out_path_or_null, const Options * opt, FILE * flog) {
    char out_path[PATH_MAX_LOCAL];
    if (out_path_or_null && out_path_or_null[0]) {
        strncpy(out_path, out_path_or_null, sizeof(out_path) - 1); out_path[sizeof(out_path) - 1] = 0;
    }
    else {
        make_output_path(in_path, opt, out_path);
    }

    if (file_exists(out_path)) {
        if (!opt->quiet) printf("Skip (already exists): %s\n", out_path);
        log_line(flog, opt, "INFO", "skip exists: %s", out_path);
        return;
    }

    printf("Converting %s -> %s ...\n", in_path, out_path);
    int rc = convert_ksc_to_jpeg(in_path, out_path, opt, flog);
    if (rc != 0) {
        fprintf(stderr, "Failed: %s\n", in_path);
    }
}

// Recursive directory walker
static void walk_dir_ksc(const char* root, const Options * opt, FILE * flog) {
#if defined(_WIN32)
    char pattern[PATH_MAX_LOCAL];
    snprintf(pattern, sizeof(pattern), "%s%c*", root && root[0] ? root : ".", PATH_SEP);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        if (!opt->quiet) fprintf(stderr, "No entries under: %s\n", root);
        return;
    }
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;

        char path[PATH_MAX_LOCAL];
        path_join(root, fd.cFileName, path, sizeof(path));

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            walk_dir_ksc(path, opt, flog);
        }
        else {
            char* ext = path_find_ext(path);
            if (ext && _stricmp(ext, ".ksc") == 0) {
                if (output_exists_for_input(path, opt)) {
                    if (!opt->quiet) printf("Skip (already exists): %s\n", path);
                    log_line(flog, opt, "INFO", "skip exists (dirwalk): %s", path);
                }
                else {
                    process_single_file(path, NULL, opt, flog);
                }
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(root && root[0] ? root : ".");
    if (!d) { if (!opt->quiet) fprintf(stderr, "Cannot open dir: %s\n", root); return; }
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;

        char path[PATH_MAX_LOCAL];
        path_join(root && root[0] ? root : ".", de->d_name, path, sizeof(path));

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            walk_dir_ksc(path, opt, flog);
        }
        else if (S_ISREG(st.st_mode)) {
            char* extp = path_find_ext(path);
            if (extp && strcasecmp(extp, ".ksc") == 0) {
                if (output_exists_for_input(path, opt)) {
                    if (!opt->quiet) printf("Skip (already exists): %s\n", path);
                    log_line(flog, opt, "INFO", "skip exists (dirwalk): %s", path);
                }
                else {
                    process_single_file(path, NULL, opt, flog);
                }
            }
        }
    }
    closedir(d);
#endif
}

// -------------- CLI parsing -------------------
static void parse_args(int argc, char* argv[], Options * opt) {
    memset(opt, 0, sizeof(*opt));
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (strcmp(a, "-r") == 0) {
            opt->recursive = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                strncpy(opt->start_dir, argv[++i], sizeof(opt->start_dir) - 1);
            }
        }
        else if (strcmp(a, "-o") == 0 && i + 1 < argc) {
            strncpy(opt->out_dir, argv[++i], sizeof(opt->out_dir) - 1);
        }
        else if (strcmp(a, "--open") == 0) {
            opt->auto_open = 1;
        }
        else if (strcmp(a, "--log") == 0 && i + 1 < argc) {
            strncpy(opt->log_path, argv[++i], sizeof(opt->log_path) - 1);
        }
        else if (strcmp(a, "-q") == 0) {
            opt->quiet = 1;
        }
        else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
        else if (a[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", a);
        }
        else {
            if (!opt->single_in[0]) {
                strncpy(opt->single_in, a, sizeof(opt->single_in) - 1);
            }
            else if (!opt->single_out[0]) {
                strncpy(opt->single_out, a, sizeof(opt->single_out) - 1);
            }
            else {
                fprintf(stderr, "Unexpected extra argument: %s\n", a);
            }
        }
    }
}

// ------------------- main ---------------------
int main(int argc, char* argv[]) {
    Options opt = { 0 };
    parse_args(argc, argv, &opt);

    // Default: recursive from current dir if no single input given
    if (!opt.recursive && !opt.single_in[0]) {
        opt.recursive = 1;
        opt.start_dir[0] = '\0';
    }

    if (opt.out_dir[0] && !dir_exists(opt.out_dir)) {
        if (!mkdir_one(opt.out_dir) && !dir_exists(opt.out_dir)) {
            fprintf(stderr, "Failed to create output directory: %s\n", opt.out_dir);
            return 1;
        }
    }

    FILE* flog = open_log(&opt);
    if (!flog && opt.log_path[0]) {
        fprintf(stderr, "WARNING: couldn't open log file: %s (logging disabled)\n", opt.log_path);
    }

    int exit_code = 0;

    if (opt.single_in[0]) {
        if (!file_exists(opt.single_in)) {
            fprintf(stderr, "Input not found: %s\n", opt.single_in);
            log_line(flog, &opt, "ERROR", "input not found: %s", opt.single_in);
            exit_code = 1;
        }
        else {
            // respect explicit output if provided
            process_single_file(opt.single_in, opt.single_out[0] ? opt.single_out : NULL, &opt, flog);
        }
    }
    else if (opt.recursive) {
        const char* root = opt.start_dir[0] ? opt.start_dir : ".";
        if (!dir_exists(root)) {
            fprintf(stderr, "Start directory not found: %s\n", root);
            log_line(flog, &opt, "ERROR", "start dir not found: %s", root);
            exit_code = 1;
        }
        else {
            if (!opt.quiet) printf("Scanning recursively from: %s\n", root);
            walk_dir_ksc(root, &opt, flog);
        }
    }
    else {
        print_usage(argv[0]);
    }

    if (flog) fclose(flog);
    return exit_code;
}

