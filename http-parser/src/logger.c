//
// Created by s.fionov on 25.11.16.
//

#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include "logger.h"

#if !defined(ANDROID)
#include <syscall.h>
#undef gettid
#define gettid() syscall(SYS_gettid)
#endif /* !defined(ANDROID) */

#define TIME_FORMAT "%d.%m.%Y %H:%M:%S%z"

static const char *log_level_name(int level);
static void logger_log_to_file(logger *ctx, logger_log_level_t log_level, const char *thread_info, const char *message);

logger *logger_open(const char *filename, logger_log_level_t log_level, logger_callback_t callback, void *attachment) {
    logger *ctx = calloc(1, sizeof(logger));

    // If callback is specified, use it, otherwise open log file.
    if (callback) {
        ctx->callback_func = callback;
    } else {
        if (filename) {
            ctx->log_file = fopen(filename, "w+");
            if (ctx->log_file == NULL) {
                fprintf(stderr, "Error opening logfile \"%s\":%s\n", filename, strerror(errno));
            }
        }
        if (ctx->log_file == NULL) {
            fprintf(stderr, "Using stderr for log output.\n");
            ctx->log_file = stderr;
        }
        ctx->callback_func = logger_log_to_file;
    }
    ctx->log_level = log_level;
    ctx->attachment = attachment;
}

int logger_close(logger *ctx) {
    int r = 0;
    if (ctx->log_file) {
        fflush(ctx->log_file);
        if (fileno(ctx->log_file) < 3 && fclose(ctx->log_file) != 0) {
            fprintf(stderr, "Error closing logfile: %s", strerror(errno));
            r = 1;
        }
        ctx->log_file = NULL;
        ctx->callback_func = NULL;
    }
    return r;
}

static void logger_log_to_file(logger *ctx, logger_log_level_t log_level, const char *thread_info, const char *message) {
    if (ctx->log_file != NULL) {
        time_t now = time(0);
        char time_str[26];
        strftime(time_str, 26, TIME_FORMAT, localtime(&now));
        fprintf(ctx->log_file, "%s %s %s %s\n", time_str, thread_info, log_level_name(log_level), message);
    }
}

/**
 * Get log level name by level code (internal function)
 * @param level Level code
 * @return Level name
 */
static const char *log_level_name(int level) {
    switch (level) {
        case LOG_LEVEL_ERROR:
            return "ERROR";
        case LOG_LEVEL_WARN:
            return "WARN";
        default:
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_DEBUG:
            return "DEBUG";
        case LOG_LEVEL_TRACE:
            return "TRACE";
    }
}

void logger_log(logger *ctx, logger_log_level_t log_level, const char *message, ...) {
    va_list args;
    va_start(args, message);
    if (ctx->callback_func && log_level <= ctx->log_level) {
        char fmt_message[256];
        vsnprintf(fmt_message, 256, message, args);
        char thread_info[64];
        snprintf(thread_info, 64, "[tid=%ld]", (long int) gettid());
        ctx->callback_func(ctx, log_level, thread_info, fmt_message);
    }
    va_end(args);
}

void logger_set_log_level(logger *ctx, logger_log_level_t log_level) {
    ctx->log_level = log_level;
}

int logger_is_open(logger *ctx) {
    return ctx->log_file != NULL;
}
