TEST     START   0x1000
F1       BYTE    X'00'
F2       BYTE    X'05'
RDBUFF   MACRO   &INDEV, &BUFADR
         CLEAR   X
T&INDEV  TD      =X'&INDEV'
         JEQ     T&INDEV
         RD      =X'&INDEV'
         STCH    &BUFADR,X
         MEND
         RDBUFF  F1,BUFFER
         RDBUFF  F2,AREA
         END     TEST
BUFFER   RESB    1
AREA     RESB    1