# Golden example — HLASM Assembler

## Input

```
DECLARATIONS:
WORKLEN  EQU   256
RCOK     EQU   0
RCFAIL   EQU   8
SAVEAREA DS    18F
WORKPTR  DS    A

FUNCTION (HLASM):
GETWORK  DS    0H
         STM   R14,R12,12(R13)
         LR    R12,R15
         GETMAIN RU,LV=WORKLEN
         LTR   R15,R15
         BNZ   GWFAIL
         ST    R1,WORKPTR
         XC    0(WORKLEN,R1),0(R1)
         LA    R15,RCOK
         B     GWEXIT
GWFAIL   LA    R15,RCFAIL
GWEXIT   LM    R14,R12,12(R13)
         BR    R14
```

## Expected output

```json
{
  "nodes": [
    { "id": "A", "kind": "step",     "text": "Entry: GETWORK" },
    { "id": "B", "kind": "step",     "text": "Save registers" },
    { "id": "C", "kind": "step",     "text": "GETMAIN work area storage" },
    { "id": "D", "kind": "decision", "text": "GETMAIN successful?" },
    { "id": "E", "kind": "step",     "text": "Set RC=8" },
    { "id": "F", "kind": "step",     "text": "Store work area pointer" },
    { "id": "G", "kind": "step",     "text": "Clear work area" },
    { "id": "H", "kind": "step",     "text": "Set RC=0" },
    { "id": "I", "kind": "return",   "text": "Restore registers and return" }
  ],
  "edges": [
    { "from": "A", "to": "B" },
    { "from": "B", "to": "C" },
    { "from": "C", "to": "D" },
    { "from": "D", "to": "E", "label": "No" },
    { "from": "D", "to": "F", "label": "Yes" },
    { "from": "F", "to": "G" },
    { "from": "G", "to": "H" },
    { "from": "H", "to": "I" },
    { "from": "E", "to": "I" }
  ]
}
```

Granularity notes: register save/restore boilerplate is one node each, not
per-register. The EQUs in the declarations give the RC values their meaning.
Never diagram individual instructions like LR or LA — describe the intent.
