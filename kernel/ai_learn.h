/*
 * AEOS - AI Learning Engine
 * Q-learning based online learning for kernel AI
 * 
 * Learns from system behavior:
 *   - Reward: system stable, low errors, good performance
 *   - Penalty: high memory pressure, errors, crashes
 *   - Updates Q-table after each inference cycle
 */
#ifndef AI_LEARN_H
#define AI_LEARN_H

#include <stdint.h>

#define LEARN_STATES     8    /* discretized state buckets */
#define LEARN_ACTIONS    3    /* SUSTAIN, BOOST, REST */
#define LEARN_MEMORY     32   /* experience replay buffer */
#define LEARN_ALPHA      1638 /* learning rate 0.025 in Q16.16 */
#define LEARN_GAMMA      52428 /* discount factor 0.8 in Q16.16 */
#define LEARN_EPSILON    1638 /* exploration rate 0.025 in Q16.16 */

typedef struct {
    uint8_t  state;        /* current state bucket */
    uint8_t  action;       /* action taken */
    int16_t  reward;       /* reward received */
    uint8_t  next_state;   /* resulting state */
    uint8_t  done;         /* episode finished */
} experience_t;

typedef struct {
    int32_t  q_table[LEARN_STATES][LEARN_ACTIONS]; /* Q-values */
    experience_t replay[LEARN_MEMORY];
    uint32_t replay_count;
    uint32_t replay_head;
    uint32_t total_updates;
    uint32_t total_reward;
    uint32_t episodes;
    uint8_t  enabled;
} ai_learn_t;

/* Initialize learning engine */
void ai_learn_init(void);

/* Get state bucket from system metrics */
uint8_t ai_learn_get_state(uint32_t heap_pct, uint32_t page_pct, uint32_t error_count);

/* Choose action using epsilon-greedy */
uint8_t ai_learn_choose_action(uint8_t state);

/* Store experience in replay buffer */
void ai_learn_store(uint8_t state, uint8_t action, int16_t reward, uint8_t next_state);

/* Update Q-table from replay buffer */
void ai_learn_update(void);

/* Calculate reward from system state */
int16_t ai_learn_calculate_reward(uint32_t heap_pct, uint32_t page_pct, uint32_t errors, uint32_t inferences);

/* Get best action for a state (no exploration) */
uint8_t ai_learn_best_action(uint8_t state);

/* Enable/disable learning */
void ai_learn_set_enabled(int enabled);

/* Get learning stats */
uint32_t ai_learn_get_updates(void);
uint32_t ai_learn_get_reward(void);
uint32_t ai_learn_get_episodes(void);
int32_t  ai_learn_get_q_value(uint8_t state, uint8_t action);

/* Report */
void ai_learn_report(void);

#endif /* AI_LEARN_H */
