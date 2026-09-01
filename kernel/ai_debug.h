/*
 * AEOS - AI Debugger
 * Auto bug detection, diagnosis, and fix
 */
#ifndef AI_DEBUG_H
#define AI_DEBUG_H

#include <stdint.h>

#define DEBUG_MAX_ISSUES    16
#define DEBUG_MAX_FIXES     8
#define DEBUG_HISTORY       32

/* Issue types */
#define ISSUE_NONE          0
#define ISSUE_MEM_LEAK      1
#define ISSUE_HIGH_ERRORS   2
#define ISSUE_SLOW_PERF     3
#define ISSUE_HUNG_TASK     4
#define ISSUE_STACK_OVERFLOW 5
#define ISSUE_DIV_ZERO      6
#define ISSUE_NULL_PTR      7

/* Fix actions */
#define FIX_NONE            0
#define FIX_RESET_DEVICE    1
#define FIX_CLEAR_MEMORY    2
#define FIX_KILL_TASK       3
#define FIX_RESET_TIMER     4
#define FIX_DISABLE_AI      5
#define FIX_REBOOT          6
#define FIX_ADJUST_HZ       7

typedef struct {
    uint8_t  type;          /* ISSUE_* */
    uint8_t  severity;      /* 0=info, 1=warn, 2=critical */
    uint32_t timestamp;
    uint32_t count;         /* how many times seen */
    uint32_t details[4];    /* issue-specific data */
} debug_issue_t;

typedef struct {
    uint8_t  issue_type;    /* what issue was fixed */
    uint8_t  fix_type;      /* FIX_* action taken */
    uint8_t  success;       /* 1=fixed, 0=failed */
    uint32_t timestamp;
    uint32_t details[2];
} debug_fix_t;

typedef struct {
    debug_issue_t issues[DEBUG_MAX_ISSUES];
    debug_fix_t   fixes[DEBUG_MAX_FIXES];
    uint32_t issue_count;
    uint32_t fix_count;
    uint32_t scan_count;
    uint32_t total_issues;
    uint32_t total_fixes;
    uint8_t  enabled;
    uint8_t  auto_fix;
} ai_debug_t;

/* Initialize debugger */
void ai_debug_init(void);

/* Scan system for issues */
void ai_debug_scan(void);

/* Auto-fix detected issues */
void ai_debug_autofix(void);

/* Report all issues */
void ai_debug_report(void);

/* Get issue count */
uint32_t ai_debug_get_issue_count(void);

/* Get fix count */
uint32_t ai_debug_get_fix_count(void);

/* Enable/disable debugger */
void ai_debug_set_enabled(int enabled);

/* Enable/disable auto-fix */
void ai_debug_set_autofix(int enabled);

#endif /* AI_DEBUG_H */
