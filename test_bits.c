#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

enum node_states {
    NODE_STATE_UNKNOWN,     /* node's initial state, unknown */
    NODE_STATE_DOWN,        /* node in non-usable state */
    NODE_STATE_IDLE,        /* node idle and available for use */
    NODE_STATE_ALLOCATED,   /* node has been allocated to a job */
    NODE_STATE_FUTURE,      /* node slot reserved for future use */
    NODE_STATE_END          /* last entry in table */
};
#define NODE_STATE_BASE       0x00ff
#define NODE_STATE_FLAGS      0xff00
#define NODE_RESUME           0x0100    /* Restore a DRAINED, DRAINING, DOWN
                                         * or FAILING node to service (e.g.
                                         * IDLE or ALLOCATED). Used in
                                         * slurm_update_node() request
                                         */
#define NODE_STATE_DRAIN      0x0200    /* node do not new allocated work */
#define NODE_STATE_COMPLETING 0x0400    /* node is completing allocated job */
#define NODE_STATE_NO_RESPOND 0x0800    /* node is not responding */
#define NODE_STATE_POWER_SAVE 0x1000    /* node in power save mode */
#define NODE_STATE_FAIL       0x2000    /* node is failing, do not allocate
                                         * new work */
#define NODE_STATE_POWER_UP   0x4000    /* restore power to a node */
#define NODE_STATE_MAINT      0x8000    /* node in maintenance reservation */

/* Defined node states */
#define IS_NODE_UNKNOWN(_X)             \
        ((_X & NODE_STATE_BASE) == NODE_STATE_UNKNOWN)
#define IS_NODE_DOWN(_X)                \
        ((_X & NODE_STATE_BASE) == NODE_STATE_DOWN)
#define IS_NODE_IDLE(_X)                \
        ((_X & NODE_STATE_BASE) == NODE_STATE_IDLE)
#define IS_NODE_ALLOCATED(_X)           \
        ((_X & NODE_STATE_BASE) == NODE_STATE_ALLOCATED)
#define IS_NODE_FUTURE(_X)              \
        ((_X & NODE_STATE_BASE) == NODE_STATE_FUTURE)

#define CYCLE_BASE_STATES(_X)                                   \
{                                                               \
    printf("IS_NODE_UNKNOWN    = %d\n", IS_NODE_UNKNOWN(_X));   \
    printf("IS_NODE_DOWN       = %d\n", IS_NODE_DOWN(_X));      \
    printf("IS_NODE_IDLE       = %d\n", IS_NODE_IDLE(_X));      \
    printf("IS_NODE_ALLOCATED  = %d\n", IS_NODE_ALLOCATED(_X)); \
    printf("IS_NODE_FUTURE     = %d\n", IS_NODE_FUTURE(_X));    \
}                                                               \

/* Derived node states */
#define IS_NODE_DRAIN(_X)               \
        (_X & NODE_STATE_DRAIN)
#define IS_NODE_DRAINING(_X)            \
        ((_X & NODE_STATE_DRAIN) && IS_NODE_ALLOCATED(_X))
#define IS_NODE_DRAINED(_X)             \
        (IS_NODE_DRAIN(_X) && !IS_NODE_DRAINING(_X))
#define IS_NODE_COMPLETING(_X)  \
        (_X & NODE_STATE_COMPLETING)
#define IS_NODE_NO_RESPOND(_X)          \
        (_X & NODE_STATE_NO_RESPOND)
#define IS_NODE_POWER_SAVE(_X)          \
        (_X & NODE_STATE_POWER_SAVE)
#define IS_NODE_FAIL(_X)                \
        (_X & NODE_STATE_FAIL)
#define IS_NODE_POWER_UP(_X)            \
        (_X & NODE_STATE_POWER_UP)
#define IS_NODE_MAINT(_X)               \
        (_X & NODE_STATE_MAINT)

#define CYCLE_DERIVED_STATES(_X)                                 \
{                                                                \
    printf("IS_NODE_DRAIN      = %d\n", IS_NODE_DRAIN(_X));      \
    printf("IS_NODE_DRAINING   = %d\n", IS_NODE_DRAINING(_X));   \
    printf("IS_NODE_DRAINED    = %d\n", IS_NODE_DRAINED(_X));    \
    printf("IS_NODE_COMPLETING = %d\n", IS_NODE_COMPLETING(_X)); \
    printf("IS_NODE_NO_RESPOND = %d\n", IS_NODE_NO_RESPOND(_X)); \
    printf("IS_NODE_POWER_SAVE = %d\n", IS_NODE_POWER_SAVE(_X)); \
    printf("IS_NODE_FAIL       = %d\n", IS_NODE_FAIL(_X));       \
    printf("IS_NODE_POWER_UP   = %d\n", IS_NODE_POWER_UP(_X));   \
    printf("IS_NODE_MAINT      = %d\n", IS_NODE_MAINT(_X));      \
}                                                                \

enum job_states {
    JOB_PENDING,            /* queued waiting for initiation */
    JOB_RUNNING,            /* allocated resources and executing */
    JOB_SUSPENDED,          /* allocated resources, execution suspended */
    JOB_COMPLETE,           /* completed execution successfully */
    JOB_CANCELLED,          /* cancelled by user */
    JOB_FAILED,             /* completed execution unsuccessfully */
    JOB_TIMEOUT,            /* terminated on reaching time limit */
    JOB_NODE_FAIL,          /* terminated on node failure */
    JOB_END                 /* not a real state, last entry in table */
};

#define JOB_COMPLETING (0x8000)

int main(int argc, char const *argv[])
{
    enum job_states * state_id;
    uint16_t node_state;
    uint16_t derived_state;
    uint16_t base_state = 0, node_flags = 0; /* all options turned off from start */
    uint16_t state_val_hex, i;
    char hex[5];
    int state_val[9] = {0, 1, 2, 513, 514, 2049, 2050, 2561, 32767};

    printf("NODE_STATE_UNKNOWN       = %d \n", NODE_STATE_UNKNOWN);
    printf("NODE_STATE_DOWN          = %d \n", NODE_STATE_DOWN);
    printf("NODE_STATE_IDLE          = %d \n", NODE_STATE_IDLE);
    printf("NODE_STATE_ALLOCATED     = %d \n", NODE_STATE_ALLOCATED);
    printf("NODE_STATE_FUTURE        = %d \n", NODE_STATE_FUTURE);

    printf("NODE_STATE_BASE       = %d [0x00ff] \n", NODE_STATE_BASE);
    printf("NODE_STATE_FLAGS      = %d [0xff00] \n", NODE_STATE_FLAGS);
    printf("NODE_RESUME           = %d [0x0100] \n", NODE_RESUME);
    printf("NODE_STATE_DRAIN      = %d [0x0200] \n", NODE_STATE_DRAIN);
    printf("NODE_STATE_COMPLETING = %d [0x0400] \n", NODE_STATE_COMPLETING);
    printf("NODE_STATE_NO_RESPOND = %d [0x0800] \n", NODE_STATE_NO_RESPOND);
    printf("NODE_STATE_POWER_SAVE = %d [0x1000] \n", NODE_STATE_POWER_SAVE);
    printf("NODE_STATE_FAIL       = %d [0x2000] \n", NODE_STATE_FAIL);
    printf("NODE_STATE_POWER_UP   = %d [0x4000] \n", NODE_STATE_POWER_UP);
    printf("NODE_STATE_MAINT      = %d [0x8000] \n", NODE_STATE_MAINT);
    printf("======================================\n");

    /* Small manual test */
    if (0)
    {
        node_state = NODE_STATE_FUTURE;
        node_state |= NODE_STATE_MAINT;
        node_state |= NODE_STATE_DRAIN;

        /* Extract the base state of the Node */
        printf("START: node_state & NODE_STATE_BASE = %d\n", node_state & NODE_STATE_BASE);
        /* Extract the flags that have been set on
           the base state to create a derived base */
        printf("START: node_state & NODE_STATE_FLAGS = %d\n", node_state & NODE_STATE_FLAGS);
        base_state = node_state & NODE_STATE_BASE;
        derived_state = node_state & NODE_STATE_FLAGS;

        /* BASE STATES */
        printf("======== BASE STATES ========");
        CYCLE_BASE_STATES(base_state);

        /* DERIVED STATES */
        printf("======== DERIVED STATES ========");
        CYCLE_DERIVED_STATES(derived_state);
    }

    /*
     * Let us check the states listed in our database for cluster veredas.
     * The states retrieved from table `cluster_event_table` are listed
     * in state_val[]
     */
    for (i = 0; i < 9; ++i)
    {
        snprintf(hex, 5, "%04x", state_val[i]);
        printf("mysql: cluster_event_table.state = %d => 0x%s\n", state_val[i], hex);
        if (sscanf(hex, "%" SCNx16, &state_val_hex) == 1)
        {
            printf("original = %d, actual = %d\n", state_val[i], state_val_hex);
            base_state = state_val_hex & NODE_STATE_BASE;
            derived_state = state_val_hex & NODE_STATE_FLAGS;
            CYCLE_BASE_STATES(base_state);
            printf("-------\n");
            CYCLE_DERIVED_STATES(derived_state);
            printf("============================================================\n");
        }
    }

    return 0;
}