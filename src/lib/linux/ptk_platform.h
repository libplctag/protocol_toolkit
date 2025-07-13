#pragma once

#ifndef __linux__
#error "This file is for Linux platforms only"
#endif

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#define THREADLET_STACK_SIZE (64 * 1024)

#define PTK_EVENT_READ   (1 << 0)
#define PTK_EVENT_WRITE  (1 << 1)
#define PTK_EVENT_ERROR  (1 << 2)

typedef struct {
    int fd;
    uint32_t events;
} platform_event_t;

typedef struct {
    platform_event_t *events;
    int count;
    int capacity;
} platform_event_list_t;