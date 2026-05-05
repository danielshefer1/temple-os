#pragma once
#include "includes.h"

typedef struct tuple_t {
    uint64_t first;
    uint64_t second;
} tuple_t;

typedef struct int_node_t {
    uint64_t val;
    struct int_node_t* next;
} int_node_t;

typedef struct tuple_node_t {
    tuple_t val;
    struct tuple_node_t* next;
} tuple_node_t;

typedef struct u64_node_t {
    uint64_t value;
    struct u64_node_t* next;
} u64_node_t;

typedef struct date {
    uint8_t day;
    uint8_t month;
    uint16_t year;
} date_t;

typedef struct time {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
} time_t;

typedef struct total_time {
    date_t date;
    time_t time;
} total_time_t;
