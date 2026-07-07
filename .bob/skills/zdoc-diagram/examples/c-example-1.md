# Golden example — C

## Input

```
DOC:
Remove the first node whose key matches, if any.

DECLARATIONS:
typedef struct node {
    char key[16];
    struct node *next;
} node_t;
extern node_t *g_head;
#define RC_OK 0
#define RC_NOTFOUND 4

FUNCTION (C):
int remove_key(const char *key) {
    node_t *cur = g_head, *prev = NULL;
    while (cur != NULL) {
        if (strcmp(cur->key, key) == 0) {
            if (prev == NULL)
                g_head = cur->next;
            else
                prev->next = cur->next;
            free(cur);
            return RC_OK;
        }
        prev = cur;
        cur = cur->next;
    }
    return RC_NOTFOUND;
}
```

## Expected output

```json
{
  "nodes": [
    { "id": "A", "kind": "step",     "text": "Entry: remove_key" },
    { "id": "B", "kind": "step",     "text": "Start at list head" },
    { "id": "C", "kind": "decision", "text": "More nodes?" },
    { "id": "D", "kind": "return",   "text": "Return RC=4 - not found" },
    { "id": "E", "kind": "decision", "text": "Key matches?" },
    { "id": "F", "kind": "step",     "text": "Advance to next node" },
    { "id": "G", "kind": "step",     "text": "Unlink node from list" },
    { "id": "H", "kind": "step",     "text": "Free node" },
    { "id": "I", "kind": "return",   "text": "Return RC=0" }
  ],
  "edges": [
    { "from": "A", "to": "B" },
    { "from": "B", "to": "C" },
    { "from": "C", "to": "D", "label": "No" },
    { "from": "C", "to": "E", "label": "Yes" },
    { "from": "E", "to": "F", "label": "No" },
    { "from": "F", "to": "C" },
    { "from": "E", "to": "G", "label": "Yes" },
    { "from": "G", "to": "H" },
    { "from": "H", "to": "I" }
  ]
}
```

Granularity notes: the head-vs-middle unlink branch collapses into one
"Unlink node from list" node — it does not change the outcome, only the
mechanics. The loop is a decision node with a back-edge (`F → C`), not
unrolled.
