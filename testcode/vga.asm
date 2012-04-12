;Simple tron game by Cyclone

SET A, 0x8000
SET C, 0x712A
SET X, 0x801F
SET Y, 0x742A
:init SET [A], C
      SET [X], Y
:loop SET B, [0x9000]

      IFE B, 0
          SET PC, loop
      IFE B, 0x0001
          SET PC, left2
      IFE B, 0x0002
          SET PC, right2
      IFE B, 0x0003
          SET PC, up2
      IFE B, 0x0004
          SET PC, down2
      IFE B, 0x0061
          SET PC, left
      IFE B, 0x0064
          SET PC, right
      IFE B, 0x0077
          SET PC, up
      IFE B, 0x0073
          SET PC, down
      SET [0x9000], 0
      SET PC, loop
:left SET [A], 0x7120
      SUB A, 1
      IFE [A], 0x7420
          SET PC, end2
      SET [A], C
      SET [0x9000], 0
      IFE A, X
          SET PC, end2
      SET PC, loop
:right    SET [A], 0x7120
          ADD A, 1
          IFE [A], 0x7420
              SET PC, end2
          SET [A], C
          SET [0x9000], 0
          IFE A, X
              SET PC, end2
          SET PC, loop
:up   SET [A], 0x7120
      SUB A, 32
      IFE [A], 0x7420
          SET PC, end2
      SET [A], C
      SET [0x9000], 0
      IFE A, X
          SET PC, end2
      SET PC, loop
:down SET [A], 0x7120
      ADD A, 32
      IFE [A], 0x7420
          SET PC, end2
      SET [A], C
      SET [0x9000], 0
      IFE A, X
          SET PC, end2
      SET PC, loop
:left2    SET [X], 0x7420
          SUB X, 1
          IFE [X], 0x7120
              SET PC, end1
          SET [X], Y
          SET [0x9000], 0
          IFE A, X
              SET PC, end1
          SET PC, loop
:right2   SET [X], 0x7420
          ADD X, 1
          IFE [X], 0x7120
              SET PC, end1
          SET [X], Y
          SET [0x9000], 0
          IFE A, X
              SET PC, end1
          SET PC, loop
:up2  SET [X], 0x7420
      SUB X, 32
      IFE [X], 0x7120
          SET PC, end1
      SET [X], Y
      SET [0x9000], 0
      IFE A, X
          SET PC, end1
      SET PC, loop
:down2    SET [X], 0x7420
          ADD X, 32
          IFE [X], 0x7120
              SET PC, end1
          SET [X], Y
          SET [0x9000], 0
          IFE A, X
              SET PC, end1
          SET PC, loop
:end1 SET [0x8000], 0x7031
      SUB PC, 1
:end2 SET [0x8000], 0x7032
      SUB PC, 1
