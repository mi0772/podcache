/*
MIT License

  Copyright (c) Carlo Di Giuseppe
  https://github.com/mi0772/clogger

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "../include/clogger.h"
#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>

#include "toml.h"


static LogLevel current_level = LOG_LEVEL_INFO;
static FILE *log_file = NULL;
static bool use_colors = true;
static FILE *output_stream = NULL;
static char log_format[128] = "[%LEVEL%] %TIME% : %MSG%";
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *level_names[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
static const char *level_colors[] = {"\033[36m", "\033[32m", "\033[33m", "\033[31m",
                                     "\033[35m"}; // cyan, green, yellow, red, magenta

void clog_init(LogLevel level, const char *log_file_path) {
    if (!output_stream) {
        output_stream = stdout;
    }
    current_level = level;
    if (log_file_path) {
        log_file = fopen(log_file_path, "a");
        if (!log_file) {
            fprintf(stderr, "[CLOGGER] Failed to open log file: %s\n", log_file_path);
        }
    }
}

bool clog_load_config(const char *config_path) {
    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        fprintf(stderr, "[CLOGGER] Unable to open config file: %s\n", config_path);
        return false;
    }

    char errbuf[200];
    toml_table_t *conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!conf) {
        fprintf(stderr, "[CLOGGER] TOML parse error: %s\n", errbuf);
        return false;
    }

    // log_level
    const char *level_str = NULL;
    toml_datum_t d = toml_string_in(conf, "log_level");
    if (d.ok)
        level_str = d.u.s;

    LogLevel level = LOG_LEVEL_INFO;
    if (level_str) {
        if (strcmp(level_str, "DEBUG") == 0)
            level = LOG_LEVEL_DEBUG;
        else if (strcmp(level_str, "INFO") == 0)
            level = LOG_LEVEL_INFO;
        else if (strcmp(level_str, "WARN") == 0)
            level = LOG_LEVEL_WARN;
        else if (strcmp(level_str, "ERROR") == 0)
            level = LOG_LEVEL_ERROR;
        else if (strcmp(level_str, "FATAL") == 0)
            level = LOG_LEVEL_FATAL;
    }

    // use_colors
    bool use_colors = true;
    d = toml_bool_in(conf, "use_colors");
    if (d.ok)
        use_colors = d.u.b;

    // log_file
    const char *log_path = NULL;
    d = toml_string_in(conf, "log_file");
    if (d.ok)
        log_path = d.u.s;

    // output_stream
    const char *output_str = NULL;
    d = toml_string_in(conf, "output_stream");
    if (d.ok)
        output_str = d.u.s;

    // log_format
    toml_table_t *fmt = toml_table_in(conf, "format");
    const char *format = NULL;
    if (fmt) {
        d = toml_string_in(fmt, "log_format");
        if (d.ok)
            format = d.u.s;
    }

    clog_init(level, log_path);
    clog_enable_colors(use_colors);
    if (format)
        clog_set_log_format(format);
    // if (output_str && strcmp(output_str, "stdout") == 0)
    // clog_set_output_stream(stdout); else if (output_str && strcmp(output_str,
    // "stderr") == 0) clog_set_output_stream(stderr);

    toml_free(conf);
    return true;
}

void clog_close() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

void clog_set_level(LogLevel level) {}

void clog_set_log_format(const char *format) {
    if (format) {
        strncpy(log_format, format, sizeof(log_format) - 1);
        log_format[sizeof(log_format) - 1] = '\0';
    }
}

void clog_enable_colors(bool enable) { use_colors = enable; }

void clog_log(LogLevel level, const char *file, int line, const char *func, const char *fmt, ...) {
    if (level < current_level)
        return;

    pthread_mutex_lock(&log_mutex);

    // Get time
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t);

    pid_t pid = getpid();
    unsigned long tid = (unsigned long) pthread_self();

    // Format message
    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    const char *color = use_colors ? level_colors[level] : "";
    const char *color_reset = use_colors ? "\033[0m" : "";

    // Costruzione log custom da formato
    char log_buf[2048];
    const char *p = log_format;
    char *out = log_buf;

    while (*p && (out - log_buf) < sizeof(log_buf) - 1) {
        if (strncmp(p, "%LEVEL%", 7) == 0) {
            out += snprintf(out, sizeof(log_buf) - (out - log_buf), "%s", level_names[level]);
            p += 7;
        } else if (strncmp(p, "%TIME%", 6) == 0) {
            out += snprintf(out, sizeof(log_buf) - (out - log_buf), "%s", time_buf); // ✅ time_buf
            p += 6;
        } else if (strncmp(p, "%FILE%", 6) == 0) {
            out += snprintf(out, sizeof(log_buf) - (out - log_buf), "%s", file);
            p += 6;
        } else if (strncmp(p, "%LINE%", 6) == 0) {
            out += snprintf(out, sizeof(log_buf) - (out - log_buf), "%d", line);
            p += 6;
        } else if (strncmp(p, "%LINE%", 6) == 0) {
            out += snprintf(out, sizeof(log_buf) - (out - log_buf), "%d", line);
            p += 6;
        } else if (strncmp(p, "%FUNC%", 6) == 0) {
            out += snprintf(out, sizeof(log_buf) - (out - log_buf), "%s", func);
            p += 6;
        } else if (strncmp(p, "%MSG%", 5) == 0) {
            out += snprintf(out, sizeof(log_buf) - (out - log_buf), "%s", msg_buf);
            p += 5;
        } else if (strncmp(p, "%PID%", 5) == 0) {
            out += snprintf(out, sizeof(log_buf) - (out - log_buf), "%d", pid);
            p += 5;
        } else if (strncmp(p, "%THREAD%", 8) == 0) {
            out += snprintf(out, sizeof(log_buf) - (out - log_buf), "%lu", tid);
            p += 8;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';

    fprintf(output_stream, "%s%s%s\n", color, log_buf, color_reset);

    if (log_file) {
        fprintf(log_file, "%s\n", log_buf);
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}
