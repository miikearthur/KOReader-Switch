/* Host test for `switch/input.c`, against a fake HID (c.f., `fake/switch.h`). */

#include <switch.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "koswitch.h"

#define HISTORY 17

static HidTouchScreenState history[HISTORY]; /* newest first, like HID's */
static size_t history_count;
static bool main_loop = true;
static int operation_mode;
static u64 next_sampling = 100;
static int failures;

void hidInitializeTouchScreen(void) {}

size_t hidGetTouchScreenStates(HidTouchScreenState *states, size_t count)
{
    size_t n = history_count < count ? history_count : count;
    memcpy(states, history, n * sizeof(*states));
    return n;
}

bool appletMainLoop(void) { return main_loop; }

int appletGetOperationMode(void) { return operation_mode; }

void svcSleepThread(s64 nano)
{
    struct timespec ts = { nano / 1000000000, nano % 1000000000 };
    nanosleep(&ts, NULL);
}

typedef struct {
    u32 id, x, y;
} finger;

static void push_sample(int count, const finger *fingers)
{
    memmove(&history[1], &history[0], (HISTORY - 1) * sizeof(history[0]));
    HidTouchScreenState *s = &history[0];
    memset(s, 0, sizeof(*s));
    s->sampling_number = next_sampling++;
    s->count = count;
    for (int i = 0; i < count; i++) {
        s->touches[i].finger_id = fingers[i].id;
        s->touches[i].x = fingers[i].x;
        s->touches[i].y = fingers[i].y;
    }
    if (history_count < HISTORY)
        history_count++;
}

#define PUSH(...)                                                   \
    do {                                                            \
        finger f_[] = { __VA_ARGS__ };                              \
        push_sample((int)(sizeof(f_) / sizeof(f_[0])), f_);         \
    } while (0)

static ko_input_event events[256];

static const char *describe(int n)
{
    static char buf[4096];
    buf[0] = '\0';
    for (int i = 0; i < n; i++) {
        const ko_input_event *e = &events[i];
        char token[32];
        if (e->type == KO_EV_SYN && e->code == KO_SYN_REPORT)
            snprintf(token, sizeof(token), "R");
        else if (e->type == KO_EV_ABS && e->code == KO_ABS_MT_SLOT)
            snprintf(token, sizeof(token), "S%d", e->value);
        else if (e->type == KO_EV_ABS && e->code == KO_ABS_MT_TRACKING_ID)
            snprintf(token, sizeof(token), "T%d", e->value);
        else if (e->type == KO_EV_ABS && e->code == KO_ABS_MT_POSITION_X)
            snprintf(token, sizeof(token), "X%d", e->value);
        else if (e->type == KO_EV_ABS && e->code == KO_ABS_MT_POSITION_Y)
            snprintf(token, sizeof(token), "Y%d", e->value);
        else if (e->type == KO_EV_SDL && e->code == KO_SDL_QUIT)
            snprintf(token, sizeof(token), "Q");
        else if (e->type == KO_EV_SDL && e->code == KO_SDL_OPERATION_MODE)
            snprintf(token, sizeof(token), "M%d", e->value);
        else
            snprintf(token, sizeof(token), "?%d/%d/%d", e->type, e->code, e->value);
        if (i)
            strcat(buf, " ");
        strcat(buf, token);
    }
    return buf;
}

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);                 \
            failures++;                                                             \
        }                                                                           \
    } while (0)

#define CHECK_EVENTS(max, timeout, expected)                                        \
    do {                                                                            \
        const char *got_ = describe(ko_input_poll(events, (max), (timeout)));       \
        if (strcmp(got_, (expected)) != 0) {                                        \
            fprintf(stderr, "FAIL line %d: got \"%s\", expected \"%s\"\n",          \
                    __LINE__, got_, (expected));                                    \
            failures++;                                                             \
        }                                                                           \
    } while (0)

static double elapsed_ms(const struct timespec *a, const struct timespec *b)
{
    return (double)(b->tv_sec - a->tv_sec) * 1e3 + (double)(b->tv_nsec - a->tv_nsec) / 1e6;
}

int main(void)
{
    /* Nothing yet. */
    CHECK_EVENTS(128, 0, "");

    /* New contact, then nothing new. */
    PUSH({ 7, 100, 200 });
    CHECK_EVENTS(128, 0, "S0 T0 X100 Y200 R");
    CHECK_EVENTS(128, 0, "");

    /* Motion then lift, both between two polls: replayed in order. */
    PUSH({ 7, 110, 200 });
    push_sample(0, NULL);
    CHECK_EVENTS(128, 0, "S0 X110 Y200 R S0 T-1 R");

    /* A quick tap, entirely between two polls, isn't lost. */
    PUSH({ 9, 300, 400 });
    push_sample(0, NULL);
    CHECK_EVENTS(128, 0, "S0 T0 X300 Y400 R S0 T-1 R");

    /* A resting finger doesn't generate events. */
    PUSH({ 3, 50, 60 });
    PUSH({ 3, 50, 60 });
    CHECK_EVENTS(128, 0, "S0 T0 X50 Y60 R");

    /* A second finger gets the next slot, coordinates are clamped to the panel. */
    PUSH({ 3, 50, 60 }, { 4, 5000, 900 });
    CHECK_EVENTS(128, 0, "S1 T1 X1279 Y719 R");
    push_sample(0, NULL);
    CHECK_EVENTS(128, 0, "S0 T-1 S1 T-1 R");

    /* HID reset (sampling numbers go back): resynchronize on the latest sample. */
    next_sampling = 5;
    PUSH({ 1, 10, 20 });
    CHECK_EVENTS(128, 0, "S0 T0 X10 Y20 R");
    push_sample(0, NULL);
    CHECK_EVENTS(128, 0, "S0 T-1 R");

    /* With room for a single sample at a time, the next polls pick up the rest. */
    finger ten[10];
    for (int i = 0; i < 10; i++)
        ten[i] = (finger){ 20 + (u32)i, 10 * (u32)i, 10 * (u32)i };
    push_sample(10, ten); /* 10 new contacts: 41 events */
    for (int i = 0; i < 10; i++)
        ten[i].x += 1;
    push_sample(10, ten); /* 10 motions: 31 events */
    push_sample(0, NULL); /* 10 lifts: 21 events */
    CHECK(ko_input_poll(events, 41, 0) == 41);
    CHECK(ko_input_poll(events, 41, 0) == 31);
    CHECK(ko_input_poll(events, 41, 0) == 21);
    CHECK_EVENTS(41, 0, "");

    /* Operation mode changes are reported. */
    operation_mode = 1;
    CHECK_EVENTS(128, 0, "M1");
    CHECK_EVENTS(128, 0, "");

    /* Timeouts are honored, both while idle, and right after touch activity. */
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    CHECK_EVENTS(128, 50, "");
    clock_gettime(CLOCK_MONOTONIC, &b);
    CHECK(elapsed_ms(&a, &b) >= 50 && elapsed_ms(&a, &b) < 120);
    clock_gettime(CLOCK_MONOTONIC, &a);
    CHECK_EVENTS(128, 3, "");
    clock_gettime(CLOCK_MONOTONIC, &b);
    CHECK(elapsed_ms(&a, &b) >= 3 && elapsed_ms(&a, &b) < 30);

    /* A blocking poll returns as soon as something happens. */
    PUSH({ 2, 1, 1 });
    CHECK_EVENTS(128, -1, "S0 T0 X1 Y1 R");

    /* Exit request: reported once, and touch input keeps working meanwhile. */
    main_loop = false;
    CHECK_EVENTS(128, 0, "Q");
    CHECK_EVENTS(128, 0, "");
    push_sample(0, NULL);
    CHECK_EVENTS(128, 0, "S0 T-1 R");

    printf(failures ? "input_test: %d failure(s)\n" : "input_test: all checks passed\n", failures);
    return failures != 0;
}
