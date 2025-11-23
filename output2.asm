TEST     START   0x1000
BUF1     BYTE    X'00'
BUF2     BYTE    X'AA'
BUF3     BYTE    X'FF'
LABEL1 CLEAR  A
          ADD_1    #1
          STCH_1   BUF1
LABEL2 CLEAR  B
          ADD_2    #2
          STCH_2   BUF2
LABEL3 CLEAR  C
          ADD_3    #3
          STCH_3   BUF3
END      TEST
