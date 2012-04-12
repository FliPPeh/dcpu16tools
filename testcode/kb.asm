JSR Clear
SET I, 0

:LOOP1
IFE [0x9000], 0 ; need to use [] to get the contents of the memory address
SET PC, LOOP1

IFG I, 511
JSR Clear
IFG I, 511
SET I, 0

set a,[0x9000]
SET [0x8000+I],a  ; use [] to get the contents!
BOR [0x8000+I], 0xF000 ;adds color

add I,1
set [0x8000+I], 0xF02c
add I,1
set [0x8000+I], 0xF020
add I,1
set b,a

shr a,4
and b,0x0F
bor a,0x30
ifg a,0x39
add a,7
bor b,0x30
ifg b,0x39
add b,7
bor a,0xF000
bor b,0xF000

add I,1
set [0x8000+I], a
add I,1
set [0x8000+I], b

ADD I, 32
shr I,5
shl I,5

SET [0x9000], 0
SET PC, LOOP1

:Clear
set A,0
:clearloop
Set [0x8000+A], 0x0000
Add A, 1
ife A, 512
Set PC, POP
set pc, clearloop
