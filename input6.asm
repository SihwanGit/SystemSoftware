TEST     START   0x1000

OUTER    MACRO &A, &B
INNER    MACRO &X, &Y
         LDA &X
         STA &Y
         MEND
         INNER &A, &B
         MEND

DATA1    WORD    10
DATA2    WORD    20
DATA3    WORD    30

OUTER    DATA1, DATA2
OUTER    DATA2, DATA3
OUTER    DATA3, DATA1

END      TEST
