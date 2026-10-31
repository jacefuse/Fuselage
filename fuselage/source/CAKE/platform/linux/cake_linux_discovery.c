// Cached sysfs discovery and hotplug. SEE: (CAKE.md, Linux controllers)
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "cake_linux_controller_private.h"
#if defined(__linux__)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
static uint64_t cake_linux_last_scan_ms;
#define CAKE_LINUX_MAX_NODES 128
#define CAKE_LINUX_NODE_NAME_MAX 32

typedef enum { CAKE_LINUX_NODE_NOT_GAMEPAD = 0, CAKE_LINUX_NODE_NO_SLOT } cake_linux_node_verdict;

typedef struct {
    char name[CAKE_LINUX_NODE_NAME_MAX];
    cake_linux_node_verdict verdict;
    bool seen;
} cake_linux_node;

static cake_linux_node cake_linux_nodes[CAKE_LINUX_MAX_NODES];
static int cake_linux_node_count = 0;

bool cake_linux_slot_freed = false;

static int cake_linux_watch_descriptor = -1;

static cake_linux_node* cake_linux_find_node_verdict(const char* name) {
    for (int index = 0; index < cake_linux_node_count; index++) {
        if (strncmp(cake_linux_nodes[index].name, name, CAKE_LINUX_NODE_NAME_MAX) == 0) {
            return &cake_linux_nodes[index];
        }
    }

    return NULL;
}

static void cake_linux_set_node_verdict(const char* name, cake_linux_node_verdict verdict) {
    cake_linux_node* node = cake_linux_find_node_verdict(name);

    if (!node) {
        if (cake_linux_node_count >= CAKE_LINUX_MAX_NODES) {
            return;
        }
        node = &cake_linux_nodes[cake_linux_node_count++];

        snprintf(node->name, sizeof(node->name), "%.31s", name);
    }

    node->verdict = verdict;
    node->seen = true;

    return;
}

static void cake_linux_forget_node_verdict(const char* name) {
    for (int index = 0; index < cake_linux_node_count; index++) {
        if (strncmp(cake_linux_nodes[index].name, name, CAKE_LINUX_NODE_NAME_MAX) != 0) {
            continue;
        }
        cake_linux_nodes[index] = cake_linux_nodes[--cake_linux_node_count];
        return;
    }

    return;
}

static bool cake_linux_read_sysfs_attribute(const char* node, const char* leaf, char* output,
                                            size_t capacity) {
    char path[CAKE_CONTROLLER_PATH_MAX];
    snprintf(path, sizeof(path), "/sys/class/input/%.32s/device/%.32s", node, leaf);

    int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return false;
    }

    if (!output || capacity == 0) {
        close(descriptor);
        return false;
    }
    ssize_t bytes_read;
    do {
        bytes_read = read(descriptor, output, capacity - 1);
    } while (bytes_read < 0 && errno == EINTR);
    close(descriptor);

    if (bytes_read < 0) {
        return false;
    }
    output[bytes_read] = '\0';

    return true;
}

static bool cake_linux_test_capability_bit(const char* capability_text, int bit) {
    const int group_width = 8 * (int)sizeof(unsigned long);
    int group = bit / group_width;
    int bit_offset = bit % group_width;

    int group_count = 0;
    for (const char* cursor = capability_text; *cursor;) {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n') {
            cursor++;
        }
        if (!*cursor) {
            break;
        }
        group_count++;
        while (*cursor && *cursor != ' ' && *cursor != '\t' && *cursor != '\n') {
            cursor++;
        }
    }
    if (group >= group_count) {
        return false;
    }

    const int target_group = group_count - 1 - group;
    const char* cursor = capability_text;
    for (int index = 0;; index++) {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n') {
            cursor++;
        }
        if (!*cursor) {
            return false;
        }
        if (index == target_group) {
            break;
        }
        while (*cursor && *cursor != ' ' && *cursor != '\t' && *cursor != '\n') {
            cursor++;
        }
    }

    unsigned long value = strtoul(cursor, NULL, 16);

    return (value >> bit_offset) & 1UL;
}

static bool cake_linux_sysfs_is_gamepad(const char* node, bool* is_gamepad) {
    char capability_text[512];

    if (!cake_linux_read_sysfs_attribute(node, "capabilities/key", capability_text,
                                         sizeof(capability_text))) {
        return false;
    }

    *is_gamepad = cake_linux_test_capability_bit(capability_text, BTN_GAMEPAD);
    return true;
}

static void cake_linux_initialize_hotplug(void) {
    cake_linux_watch_descriptor = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (cake_linux_watch_descriptor < 0) {
        printf("[CAKE] Warning: inotify unavailable; hotplug falls back to the 2s scan\n");
        return;
    }

    if (inotify_add_watch(cake_linux_watch_descriptor, "/dev/input", IN_CREATE | IN_DELETE | IN_ATTRIB) < 0) {
        printf("[CAKE] Warning: cannot watch /dev/input; hotplug falls back to the 2s scan\n");
        close(cake_linux_watch_descriptor);
        cake_linux_watch_descriptor = -1;
    }

    return;
}

static bool cake_linux_drain_hotplug(void) {
    if (cake_linux_watch_descriptor < 0) {
        return false;
    }

    union {
        struct inotify_event event;
        char bytes[4096];
    } buffer;

    bool changed = false;
    for (;;) {
        ssize_t bytes_read = read(cake_linux_watch_descriptor, buffer.bytes, sizeof(buffer.bytes));
        if (bytes_read < 0 && errno == EINTR) {
            continue;
        }
        if (bytes_read <= 0) {
            break;
        }

        changed = true;
    }

    return changed;
}

void cake_linux_initialize_discovery(void) {

    cake_linux_initialize_hotplug();

    return;
}

void cake_linux_scan_controllers(void) {
    DIR* directory = opendir("/dev/input");
    if (!directory) {
        return;
    }

    for (int index = 0; index < cake_linux_node_count; index++) {
        cake_linux_nodes[index].seen = false;
    }

    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        char path[CAKE_CONTROLLER_PATH_MAX];

        snprintf(path, sizeof(path), "/dev/input/%.240s", entry->d_name);

        bool taken = false;
        for (int index = 0; index < CAKE_CONTROLLER_MAX; index++) {
            if (cake_linux_controllers[index].present &&
                strncmp(cake_linux_controllers[index].path, path, sizeof(path)) == 0) {
                taken = true;
                break;
            }
        }
        if (taken) {
            continue;
        }

        cake_linux_node* cached_verdict = cake_linux_find_node_verdict(entry->d_name);
        if (cached_verdict) {
            cached_verdict->seen = true;
            if (cached_verdict->verdict == CAKE_LINUX_NODE_NOT_GAMEPAD) {
                continue;
            }
            if (cached_verdict->verdict == CAKE_LINUX_NODE_NO_SLOT && !cake_linux_slot_freed) {
                continue;
            }
        }

        bool is_gamepad;
        if (!cake_linux_sysfs_is_gamepad(entry->d_name, &is_gamepad)) {
            continue;
        }
        if (!is_gamepad) {
            cake_linux_set_node_verdict(entry->d_name, CAKE_LINUX_NODE_NOT_GAMEPAD);
            continue;
        }

        int descriptor = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        bool writable = descriptor >= 0;
        if (descriptor < 0) {
            descriptor = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        }
        if (descriptor < 0) {

            continue;
        }

        unsigned long key_bits[(KEY_MAX / (8 * sizeof(unsigned long))) + 1] = {0};
        if (ioctl(descriptor, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
            close(descriptor);
            continue;
        }
        if (!cake_linux_test_bit(key_bits, BTN_GAMEPAD)) {

            close(descriptor);
            cake_linux_set_node_verdict(entry->d_name, CAKE_LINUX_NODE_NOT_GAMEPAD);
            continue;
        }

        if (!cake_linux_adopt_controller(descriptor, path, writable)) {
            close(descriptor);
            cake_linux_set_node_verdict(entry->d_name, CAKE_LINUX_NODE_NO_SLOT);
            continue;
        }

        cake_linux_forget_node_verdict(entry->d_name);
    }

    closedir(directory);

    for (int index = 0; index < cake_linux_node_count;) {
        if (cake_linux_nodes[index].seen) {
            index++;
            continue;
        }
        cake_linux_nodes[index] = cake_linux_nodes[--cake_linux_node_count];
    }

    cake_linux_slot_freed = false;

    return;
}

void cake_linux_refresh_discovery(uint64_t now) {
    bool hotplug = cake_linux_drain_hotplug();
    if (hotplug) {

        cake_linux_node_count = 0;
    }

    if (hotplug || cake_linux_slot_freed ||
        (now >= cake_linux_last_scan_ms && now - cake_linux_last_scan_ms >= 2000)) {
        cake_linux_last_scan_ms = now;
        cake_linux_scan_controllers();
    }
}
void cake_linux_shutdown_discovery(void) {
    if (cake_linux_watch_descriptor >= 0) {
        close(cake_linux_watch_descriptor);
        cake_linux_watch_descriptor = -1;
    }
    cake_linux_node_count = 0;
    cake_linux_slot_freed = false;
    cake_linux_last_scan_ms = 0;
}

#endif
