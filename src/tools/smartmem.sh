#!/bin/bash
# SmartMemEngine 快速启停脚本

MODULE="/root/project/smartmem_engine/src/smartmem.ko"
PROC="/proc/smartmem"

case "$1" in
    start)
        if lsmod | grep -q smartmem; then
            echo "smartmem already loaded"
            exit 0
        fi
        insmod "$MODULE" && echo "smartmem loaded" || echo "failed to load"
        ;;
    stop)
        if ! lsmod | grep -q smartmem; then
            echo "smartmem not loaded"
            exit 0
        fi
        rmmod smartmem && echo "smartmem unloaded" || echo "failed to unload"
        ;;
    restart)
        $0 stop
        sleep 1
        $0 start
        ;;
    status)
        if lsmod | grep -q smartmem; then
            echo "smartmem: loaded"
            if [ -d "$PROC" ]; then
                echo "procfs:  available"
                echo ""
                memstat summary 2>/dev/null || cat "$PROC/stats" | head -10
            fi
        else
            echo "smartmem: not loaded"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac
