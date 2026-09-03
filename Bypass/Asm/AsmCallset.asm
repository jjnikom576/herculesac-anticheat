.data
extern ptr_FindWindowA: dq

.code

public _New_01_Crc32

_New_01_Crc32 proc
    mov rcx, ptr_FindWindowA
    ret
_New_01_Crc32 endp

end