# Golden example — PL/X

## Input

```
DECLARATIONS:
DCL 1 CB BASED(CBPTR),
      2 CBEYE   CHAR(4),
      2 CBFLAGS BIT(8),
      2 CBNEXT  PTR;
DCL CBINIT BIT(8) CONSTANT('80'X);
DCL CBSTG FIXED BIN(31);
DCL 1 ANCH BASED(ANCHOR),
      2 ANCHEYE  CHAR(4),
      2 ANCHFRST PTR;

FUNCTION (PL/X):
INITPROC: PROC(ANCHOR) RETURNS(FIXED BIN(31));
    CBSTG = OBTAIN(LENGTH(CB));
    IF CBSTG = 0 THEN
        RETURN(8);
    CBPTR = CBSTG;
    CBPTR->CBEYE = 'ZDCB';
    CBPTR->CBFLAGS = CBINIT;
    CBPTR->CBNEXT = ANCHOR->ANCHFRST;
    ANCHOR->ANCHFRST = CBPTR;
    RETURN(0);
END INITPROC;
```

## Expected output

```json
{
  "nodes": [
    { "id": "A", "kind": "step",     "text": "Entry: INITPROC" },
    { "id": "B", "kind": "step",     "text": "Obtain storage for control block" },
    { "id": "C", "kind": "decision", "text": "Storage obtained?" },
    { "id": "D", "kind": "return",   "text": "Return RC=8 - storage failure" },
    { "id": "E", "kind": "step",     "text": "Initialise CB eyecatcher and init flag" },
    { "id": "F", "kind": "step",     "text": "Chain CB onto anchor list" },
    { "id": "G", "kind": "return",   "text": "Return RC=0" }
  ],
  "edges": [
    { "from": "A", "to": "B" },
    { "from": "B", "to": "C" },
    { "from": "C", "to": "D", "label": "No" },
    { "from": "C", "to": "E", "label": "Yes" },
    { "from": "E", "to": "F" },
    { "from": "F", "to": "G" }
  ]
}
```

Note the granularity: three assignment statements collapse into one
"Initialise" node; the two chaining assignments collapse into one "Chain"
node. The declarations told us CBFLAGS/CBINIT mean an init flag and ANCHFRST
is a list head — that knowledge appears in the node labels, but the
declarations themselves are not diagrammed.
