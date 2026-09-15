/* Minimal stand-in for libnx's <switch.h>, to test `input.c` on the host. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef int64_t s64;

/* Same layout as libnx's. */
typedef struct HidTouchState {
    u64 delta_time;
    u32 attributes;
    u32 finger_id;
    u32 x;
    u32 y;
    u32 diameter_x;
    u32 diameter_y;
    u32 rotation_angle;
    u32 reserved;
} HidTouchState;

typedef struct HidTouchScreenState {
    u64 sampling_number;
    s32 count;
    u32 reserved;
    HidTouchState touches[16];
} HidTouchScreenState;

void hidInitializeTouchScreen(void);
size_t hidGetTouchScreenStates(HidTouchScreenState *states, size_t count);
bool appletMainLoop(void);
int appletGetOperationMode(void);
void svcSleepThread(s64 nano);
