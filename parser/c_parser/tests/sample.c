/**
 * @file sample.c
 * Sample C module used to exercise the ZDoc C parser.
 */

#include <stdio.h>

/** Maximum number of retries before giving up. */
#define MAX_RETRIES 5

#define SAMPLE_H_GUARD /* undocumented object-like macro: should be skipped */

/** Compute the smaller of two values. */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief Initialise the widget subsystem.
 *
 * Allocates the anchor control block and chains it.
 *
 * @param anchor pointer to the anchor block
 * @param flags  initialisation flags
 * @return 0 on success, 8 on storage failure
 * @note Not thread safe.
 */
int widget_init(void *anchor, unsigned flags);

/* plain comment: must NOT attach to the variable below */
static int internal_counter = 0;

/**
 * Terminates the widget subsystem.
 * Releases all storage held by the anchor.
 */
void widget_term(void)
{
    const char *s = "string with } brace and \" quote";
    char c = '}';
    if (internal_counter) {
        internal_counter = 0;
    }
#define LOCAL_MACRO 1
}

/** Widget state record. */
struct widget_state {
    int refcount;
    char name[32];
};

/** Event callback type. */
typedef int (*widget_cb)(void *ctx, int event);

/** Colour codes returned by widget_colour(). */
enum widget_colour { WC_RED, WC_GREEN, WC_BLUE };

/** Verbosity level, 0 = silent. */
int widget_debug_level = 0;

/// Line-comment style doc.
/// Second line of the brief.
static int widget_reset(struct widget_state *ws)
{
    (void)ws;
    return 0;
}
