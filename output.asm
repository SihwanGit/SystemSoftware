TEST     START   0x1000
F1       BYTE    X'00'
F2       BYTE    X'05'
         CLEAR_1   X
TF1  TD      =X'F1'
         JEQ_1     TF1
         RD_1      =X'F1'
         STCH_1    BUFFER,X
         CLEAR_2   X
TF2  TD      =X'F2'
         JEQ_2     TF2
         RD_2      =X'F2'
         STCH_2    AREA,X
         END     TEST
BUFFER   RESB    1
AREA     RESB    1
