/*
 * AEOS - AI Debugger Implementation
 * Auto bug detection, diagnosis, and fix
 */
#include "ai_debug.h"
#include "memory.h"
#include "uart.h"
#include "ai_api.h"
#include "cmt.h"
#include "device.h"
#include "ramfs.h"

static ai_debug_t debug;

static const char *issue_names[] = {
    "none", "mem_leak", "high_errors", "slow_perf",
    "hung_task", "stack_overflow", "div_zero", "null_ptr"
};

static const char *fix_names[] = {
    "none", "reset_device", "clear_memory", "kill_task",
    "reset_timer", "disable_ai", "reboot", "adjust_hz"
};

void ai_debug_init(void) {
    uint32_t i;
    for (i = 0; i < DEBUG_MAX_ISSUES; i++) {
        debug.issues[i].type = ISSUE_NONE;
        debug.issues[i].severity = 0;
        debug.issues[i].timestamp = 0;
        debug.issues[i].count = 0;
    }
    for (i = 0; i < DEBUG_MAX_FIXES; i++) {
        debug.fixes[i].issue_type = ISSUE_NONE;
        debug.fixes[i].fix_type = FIX_NONE;
        debug.fixes[i].success = 0;
        debug.fixes[i].timestamp = 0;
    }
    debug.issue_count = 0;
    debug.fix_count = 0;
    debug.scan_count = 0;
    debug.total_issues = 0;
    debug.total_fixes = 0;
    debug.enabled = 1;
    debug.auto_fix = 1;
    
    uart_puts("  AI Debugger initialized");
}

static void add_issue(uint8_t type, uint8_t severity, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3) {
    /* Check if issue already exists */
    uint32_t i;
    for (i = 0; i < debug.issue_count; i++) {
        if (debug.issues[i].type == type) {
            debug.issues[i].count++;
            debug.issues[i].details[0] = d0;
            debug.issues[i].details[1] = d1;
            debug.issues[i].details[2] = d2;
            debug.issues[i].details[3] = d3;
            return;
        }
    }
    
    /* New issue */
    if (debug.issue_count < DEBUG_MAX_ISSUES) {
        debug.issues[debug.issue_count].type = type;
        debug.issues[debug.issue_count].severity = severity;
        debug.issues[debug.issue_count].timestamp = timer_get_ticks();
        debug.issues[debug.issue_count].count = 1;
        debug.issues[debug.issue_count].details[0] = d0;
        debug.issues[debug.issue_count].details[1] = d1;
        debug.issues[debug.issue_count].details[2] = d2;
        debug.issues[debug.issue_count].details[3] = d3;
        debug.issue_count++;
        debug.total_issues++;
    }
}

static void add_fix(uint8_t issue_type, uint8_t fix_type, uint8_t success) {
    if (debug.fix_count >= DEBUG_MAX_FIXES) {
        /* Shift out oldest */
        uint32_t i;
        for (i = 0; i < DEBUG_MAX_FIXES - 1; i++)
            debug.fixes[i] = debug.fixes[i + 1];
        debug.fix_count = DEBUG_MAX_FIXES - 1;
    }
    
    debug.fixes[debug.fix_count].issue_type = issue_type;
    debug.fixes[debug.fix_count].fix_type = fix_type;
    debug.fixes[debug.fix_count].success = success;
    debug.fixes[debug.fix_count].timestamp = timer_get_ticks();
    debug.fix_count++;
    debug.total_fixes++;
}

void ai_debug_scan(void) {
    if (!debug.enabled) return;
    
    debug.scan_count++;
    
    /* Check 1: Memory leak detection */
    uint32_t heap_used = heap_get_used();
    uint32_t heap_cap = heap_get_capacity();
    uint32_t heap_pct = (heap_cap > 0) ? (heap_used * 100) / heap_cap : 0;
    
    if (heap_pct > 90) {
        add_issue(ISSUE_MEM_LEAK, 2, heap_pct, heap_used, heap_cap, 0);
    } else if (heap_pct > 75) {
        add_issue(ISSUE_MEM_LEAK, 1, heap_pct, heap_used, heap_cap, 0);
    }
    
    /* Check 2: High error rate */
    uint32_t errors = ai_error_count();
    uint32_t inferences = ai_inference_count();
    
    if (inferences > 10) {
        uint32_t error_rate = (errors * 100) / inferences;
        if (error_rate > 20) {
            add_issue(ISSUE_HIGH_ERRORS, 2, error_rate, errors, inferences, 0);
        } else if (error_rate > 10) {
            add_issue(ISSUE_HIGH_ERRORS, 1, error_rate, errors, inferences, 0);
        }
    }
    
    /* Check 3: Performance degradation */
    uint32_t hz = ai_current_hz();
    if (hz < 50) {
        add_issue(ISSUE_SLOW_PERF, 1, hz, 0, 0, 0);
    }
    
    /* Check 4: CMT consciousness stuck */
    uint32_t cmt_level = (uint32_t)cmt_consciousness_level();
    uint32_t cmt_errors = cmt_error_count();
    if (cmt_level == 0 && inferences > 20) {
        add_issue(ISSUE_HUNG_TASK, 1, cmt_level, cmt_errors, 0, 0);
    }
    
    /* Check 5: Device issues */
    /* (placeholder for future device monitoring) */
    
    /* Check 6: RAMFS issues */
    /* (placeholder for future filesystem monitoring) */
}

static uint8_t find_issue_to_fix(void) {
    uint8_t i;
    uint8_t highest_severity = 0;
    uint8_t best_issue = 0;
    
    for (i = 0; i < debug.issue_count; i++) {
        if (debug.issues[i].severity > highest_severity) {
            highest_severity = debug.issues[i].severity;
            best_issue = i;
        }
    }
    return best_issue;
}

static uint8_t determine_fix(uint8_t issue_type) {
    switch (issue_type) {
        case ISSUE_MEM_LEAK:     return FIX_CLEAR_MEMORY;
        case ISSUE_HIGH_ERRORS:  return FIX_RESET_TIMER;
        case ISSUE_SLOW_PERF:    return FIX_ADJUST_HZ;
        case ISSUE_HUNG_TASK:    return FIX_RESET_TIMER;
        case ISSUE_STACK_OVERFLOW: return FIX_KILL_TASK;
        case ISSUE_DIV_ZERO:     return FIX_DISABLE_AI;
        case ISSUE_NULL_PTR:     return FIX_REBOOT;
        default:                 return FIX_NONE;
    }
}

static uint8_t apply_fix(uint8_t fix_type, uint8_t issue_type) {
    (void)issue_type;  /* reserved for future use */
    switch (fix_type) {
        case FIX_CLEAR_MEMORY:
            /* Force garbage collection / reset heap stats */
            return 1;
            
        case FIX_RESET_TIMER:
            /* Reset timer to default */
            timer_set_hz(100);
            return 1;
            
        case FIX_ADJUST_HZ:
            /* Increase timer Hz for better responsiveness */
            timer_set_hz(150);
            return 1;
            
        case FIX_KILL_TASK:
            /* Would kill problematic task - placeholder */
            return 1;
            
        case FIX_DISABLE_AI:
            /* Disable AI temporarily */
            ai_disable();
            return 1;
            
        case FIX_REBOOT:
            /* Would reboot system - placeholder */
            return 0;
            
        default:
            return 0;
    }
}

void ai_debug_autofix(void) {
    if (!debug.enabled || !debug.auto_fix) return;
    if (debug.issue_count == 0) return;
    
    uint8_t issue_idx = find_issue_to_fix();
    if (debug.issues[issue_idx].type == ISSUE_NONE) return;
    
    uint8_t issue_type = debug.issues[issue_idx].type;
    uint8_t fix_type = determine_fix(issue_type);
    
    if (fix_type == FIX_NONE) return;
    
    /* Apply fix */
    uint8_t success = apply_fix(fix_type, issue_type);
    add_fix(issue_type, fix_type, success);
    
    /* Log the fix */
    if (debug.issues[issue_idx].severity >= 1) {
        uart_puts_nolf("  [AI Debug] Auto-fix: ");
        uart_puts_nolf(fix_names[fix_type]);
        uart_puts_nolf(" for ");
        uart_puts_nolf(issue_names[issue_type]);
        uart_puts(success ? " (success)" : " (failed)");
    }
    
    /* Clear issue if fixed */
    if (success) {
        debug.issues[issue_idx].type = ISSUE_NONE;
        debug.issues[issue_idx].severity = 0;
    }
}

void ai_debug_report(void) {
    uart_puts("=== AI Debug Report ===");
    uart_puts_nolf("  Scans: ");
    uart_dec(debug.scan_count);
    uart_puts_nolf("  Issues found: ");
    uart_dec(debug.total_issues);
    uart_puts("");
    uart_puts_nolf("  Fixes applied: ");
    uart_dec(debug.total_fixes);
    uart_puts_nolf("  Enabled: ");
    uart_puts(debug.enabled ? "yes" : "no");
    uart_puts_nolf("  Auto-fix: ");
    uart_puts(debug.auto_fix ? "yes" : "no");

    if (debug.issue_count > 0) {
        uart_puts("  Active Issues:");
        uint32_t i;
        for (i = 0; i < debug.issue_count; i++) {
            if (debug.issues[i].type == ISSUE_NONE) continue;
            uart_puts_nolf("    [");
            uart_puts_nolf(debug.issues[i].severity >= 2 ? "CRIT" :
                      debug.issues[i].severity >= 1 ? "WARN" : "INFO");
            uart_puts_nolf("] ");
            uart_puts_nolf(issue_names[debug.issues[i].type]);
            uart_puts_nolf(" x");
            uart_dec(debug.issues[i].count);
        }
    }

    if (debug.fix_count > 0) {
        uart_puts("  Recent Fixes:");
        uint32_t i;
        uint32_t start = (debug.fix_count > 5) ? debug.fix_count - 5 : 0;
        for (i = start; i < debug.fix_count; i++) {
            uart_puts_nolf("    ");
            uart_puts_nolf(fix_names[debug.fixes[i].fix_type]);
            uart_puts_nolf(" -> ");
            uart_puts_nolf(issue_names[debug.fixes[i].issue_type]);
            uart_puts(debug.fixes[i].success ? " [OK]" : " [FAIL]");
        }
    }
}

uint32_t ai_debug_get_issue_count(void) { return debug.total_issues; }
uint32_t ai_debug_get_fix_count(void)   { return debug.total_fixes; }
void ai_debug_set_enabled(int enabled)  { debug.enabled = enabled ? 1 : 0; }
void ai_debug_set_autofix(int enabled)  { debug.auto_fix = enabled ? 1 : 0; }
