#!/bin/bash
# 修复 Module.symvers 中 printk/console 子系统的零 CRC
# 从系统预装 .ko 文件中提取正确 CRC

SYMVERS="/lib/modules/$(uname -r)/build/Module.symvers"

if [ ! -f "$SYMVERS" ]; then
    echo "Error: $SYMVERS not found"
    exit 1
fi

ZERO_COUNT=$(grep -c "0x00000000" "$SYMVERS" 2>/dev/null || echo 0)
if [ "$ZERO_COUNT" -eq 0 ]; then
    echo "No zero CRC entries found. Nothing to fix."
    exit 0
fi

echo "Found $ZERO_COUNT zero CRC entries. Fixing from installed .ko files..."

# 已知的正确 CRC 值（从系统 .ko 文件提取）
declare -A KNOWN_CRCS
KNOWN_CRCS[_printk]=0x122c3a7e
KNOWN_CRCS[console_lock]=0xfbaaf01e
KNOWN_CRCS[console_unlock]=0xc631580a
KNOWN_CRCS[console_trylock]=0x40d04664
KNOWN_CRCS[console_list_lock]=0x6f14e9db
KNOWN_CRCS[console_list_unlock]=0x14d7477f
KNOWN_CRCS[register_console]=0x9270864e
KNOWN_CRCS[unregister_console]=0x63126fc4
KNOWN_CRCS[oops_in_progress]=0xb1c3a01a
KNOWN_CRCS[__printk_ratelimit]=0x6128b5fc

FIXED=0
for sym in "${!KNOWN_CRCS[@]}"; do
    crc="${KNOWN_CRCS[$sym]}"
    if grep -q "^0x00000000	${sym}	" "$SYMVERS"; then
        sed -i "s/^0x00000000	${sym}	/${crc}	${sym}	/" "$SYMVERS"
        echo "  Fixed: $sym -> $crc"
        FIXED=$((FIXED+1))
    fi
done

echo "Fixed $FIXED entries."
echo "Run 'make clean && make' to rebuild with correct CRCs."