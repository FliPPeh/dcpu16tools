SET [vram + A], 0x42
ADD A, 1
SET [vram + A], 0x43
SUB PC, 1

.org 0x20
:vram
