/*
 * AEOS - AI Learning Engine Implementation
 * Q-learning with experience replay
 */
#include "ai_learn.h"
#include "uart.h"
#include "fp.h"

static ai_learn_t learn;

void ai_learn_init(void) {
    uint32_t i, j;
    
    /* Zero Q-table */
    for (i = 0; i < LEARN_STATES; i++)
        for (j = 0; j < LEARN_ACTIONS; j++)
            learn.q_table[i][j] = 0;
    
    /* Zero replay buffer */
    for (i = 0; i < LEARN_MEMORY; i++) {
        learn.replay[i].state = 0;
        learn.replay[i].action = 0;
        learn.replay[i].reward = 0;
        learn.replay[i].next_state = 0;
        learn.replay[i].done = 0;
    }
    
    learn.replay_count = 0;
    learn.replay_head = 0;
    learn.total_updates = 0;
    learn.total_reward = 0;
    learn.episodes = 0;
    learn.enabled = 1;
    
    /* Initialize with some prior knowledge */
    /* SUSTAIN is safe default */
    for (i = 0; i < LEARN_STATES; i++) {
        learn.q_table[i][0] = 6553;   /* SUSTAIN: 0.1 */
        learn.q_table[i][1] = 0;      /* BOOST: 0 */
        learn.q_table[i][2] = -3276;  /* REST: -0.5 */
    }
    
    /* High pressure states prefer REST */
    for (i = LEARN_STATES / 2; i < LEARN_STATES; i++) {
        learn.q_table[i][2] = 3276;   /* REST: 0.5 */
    }
    
    /* Low pressure states prefer BOOST */
    for (i = 0; i < LEARN_STATES / 3; i++) {
        learn.q_table[i][1] = 6553;   /* BOOST: 0.1 */
    }
    
    uart_puts("  AI Learning engine initialized");
}

uint8_t ai_learn_get_state(uint32_t heap_pct, uint32_t page_pct, uint32_t error_count) {
    /* Combine metrics into state bucket (0-7) */
    uint32_t pressure = (heap_pct + page_pct) / 2;
    uint32_t state = pressure * LEARN_STATES / 100;
    
    /* High errors push to higher states */
    if (error_count > 10) state = LEARN_STATES - 1;
    else if (error_count > 5) {
        if (state < LEARN_STATES - 2) state += 2;
    }
    
    if (state >= LEARN_STATES) state = LEARN_STATES - 1;
    return (uint8_t)state;
}

static int32_t fp_rand(uint32_t seed) {
    /* Simple LCG pseudo-random for exploration */
    seed = seed * 1103515245 + 12345;
    return (int32_t)((seed >> 16) & 0x7FFF);
}

uint8_t ai_learn_choose_action(uint8_t state) {
    uint8_t action;
    int32_t r = fp_rand(learn.total_updates + learn.replay_count);
    
    /* Epsilon-greedy: explore with probability epsilon */
    if (r < LEARN_EPSILON) {
        /* Explore: random action */
        action = (uint8_t)(r % LEARN_ACTIONS);
    } else {
        /* Exploit: best known action */
        action = ai_learn_best_action(state);
    }
    
    return action;
}

uint8_t ai_learn_best_action(uint8_t state) {
    if (state >= LEARN_STATES) return 0;
    
    uint8_t best = 0;
    int32_t best_val = learn.q_table[state][0];
    uint8_t i;
    
    for (i = 1; i < LEARN_ACTIONS; i++) {
        if (learn.q_table[state][i] > best_val) {
            best_val = learn.q_table[state][i];
            best = i;
        }
    }
    return best;
}

void ai_learn_store(uint8_t state, uint8_t action, int16_t reward, uint8_t next_state) {
    experience_t *e = &learn.replay[learn.replay_head];
    e->state = state;
    e->action = action;
    e->reward = reward;
    e->next_state = next_state;
    e->done = (next_state >= LEARN_STATES - 1) ? 1 : 0;
    
    learn.replay_head = (learn.replay_head + 1) % LEARN_MEMORY;
    if (learn.replay_count < LEARN_MEMORY) learn.replay_count++;
    learn.total_reward += (uint32_t)(reward > 0 ? reward : -reward);
}

void ai_learn_update(void) {
    if (!learn.enabled || learn.replay_count < 4) return;
    
    /* Sample random experiences from buffer */
    uint32_t samples = (learn.replay_count < 8) ? learn.replay_count : 8;
    uint32_t i;
    
    for (i = 0; i < samples; i++) {
        uint32_t idx = (learn.total_updates + i) % learn.replay_count;
        experience_t *e = &learn.replay[idx];
        
        /* Q-learning update: Q(s,a) = Q(s,a) + alpha * (r + gamma * max(Q(s',a')) - Q(s,a)) */
        int32_t current_q = learn.q_table[e->state][e->action];
        int32_t max_next = learn.q_table[e->next_state][0];
        uint8_t j;
        
        for (j = 1; j < LEARN_ACTIONS; j++) {
            if (learn.q_table[e->next_state][j] > max_next)
                max_next = learn.q_table[e->next_state][j];
        }
        
        /* Q16.16 arithmetic */
        int32_t target = (int32_t)e->reward + fp_mul(LEARN_GAMMA, max_next);
        int32_t delta = target - current_q;
        int32_t update = fp_mul(LEARN_ALPHA, delta);
        
        learn.q_table[e->state][e->action] = current_q + update;
        
        /* Clamp to reasonable range */
        if (learn.q_table[e->state][e->action] > 65536)
            learn.q_table[e->state][e->action] = 65536;
        if (learn.q_table[e->state][e->action] < -65536)
            learn.q_table[e->state][e->action] = -65536;
    }
    
    learn.total_updates += samples;
    learn.episodes++;
}

int16_t ai_learn_calculate_reward(uint32_t heap_pct, uint32_t page_pct, uint32_t errors, uint32_t inferences) {
    int16_t reward = 0;
    
    /* Positive rewards */
    if (inferences > 0) reward += 10;
    if (heap_pct < 50) reward += 5;
    if (page_pct < 50) reward += 5;
    
    /* Negative rewards */
    if (errors > 0) reward -= (int16_t)(errors * 20);
    if (heap_pct > 80) reward -= 15;
    if (page_pct > 80) reward -= 15;
    if (heap_pct > 95) reward -= 50;
    
    return reward;
}

void ai_learn_set_enabled(int enabled) {
    learn.enabled = enabled ? 1 : 0;
}

uint32_t ai_learn_get_updates(void) { return learn.total_updates; }
uint32_t ai_learn_get_reward(void)  { return learn.total_reward; }
uint32_t ai_learn_get_episodes(void) { return learn.episodes; }

int32_t ai_learn_get_q_value(uint8_t state, uint8_t action) {
    if (state >= LEARN_STATES || action >= LEARN_ACTIONS) return 0;
    return learn.q_table[state][action];
}

void ai_learn_report(void) {
    uart_puts("=== AI Learning Report ===");
    uart_puts_nolf("  Updates: ");
    uart_dec(learn.total_updates);
    uart_puts_nolf("  Episodes: ");
    uart_dec(learn.episodes);
    uart_puts("");
    uart_puts_nolf("  Total Reward: ");
    uart_dec(learn.total_reward);
    uart_puts_nolf("  Replay Buffer: ");
    uart_dec(learn.replay_count);
    uart_puts_nolf("/");
    uart_dec(LEARN_MEMORY);
    uart_puts("");
    uart_puts_nolf("  Enabled: ");
    uart_puts(learn.enabled ? "yes" : "no");

    /* Show Q-table summary */
    uart_puts("  Q-table (best actions per state):");
    uint8_t i;
    for (i = 0; i < LEARN_STATES; i++) {
        uart_puts_nolf("    State ");
        uart_dec(i);
        uart_puts_nolf(": ");
        uint8_t best = ai_learn_best_action(i);
        uart_puts_nolf(best == 0 ? "SUSTAIN" : (best == 1 ? "BOOST" : "REST"));
        uart_puts_nolf(" (Q=");
        uart_dec(learn.q_table[i][best]);
        uart_puts(")");
    }
}
