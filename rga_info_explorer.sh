#!/bin/bash

###############################################################################
# RGA Information Explorer for RK3566
# This script comprehensively collects RGA (Raster Graphic Acceleration) 
# information from various kernel interfaces
###############################################################################

echo "=========================================="
echo "RGA Information Explorer for RK3566"
echo "=========================================="
echo ""

# Color codes for better readability
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

###############################################################################
# 1. Debugfs RGA Information
###############################################################################
echo -e "${GREEN}[1] Debugfs RGA Information${NC}"
echo "=========================================="

RGA_DEBUGFS="/sys/kernel/debug/rkrga"
if [ -d "$RGA_DEBUGFS" ]; then
    echo -e "${BLUE}Found RGA debugfs at: $RGA_DEBUGFS${NC}"
    echo ""
    
    # List all available files
    echo "Available files:"
    ls -la "$RGA_DEBUGFS"
    echo ""
    
    # Read each file
    for file in "$RGA_DEBUGFS"/*; do
        if [ -f "$file" ]; then
            filename=$(basename "$file")
            echo -e "${YELLOW}--- Contents of $filename ---${NC}"
            cat "$file" 2>/dev/null || echo "  (Permission denied or empty)"
            echo ""
        fi
    done
else
    echo "RGA debugfs not found at $RGA_DEBUGFS"
fi

###############################################################################
# 2. Sysfs RGA Information
###############################################################################
echo -e "${GREEN}[2] Sysfs RGA Information${NC}"
echo "=========================================="

# Common sysfs paths for RGA
SYSFS_PATHS=(
    "/sys/class/rga"
    "/sys/devices/platform/fdd90000.rga"
    "/sys/devices/platform/rga"
    "/sys/devices/platform/fdd90000.rga/rga"
)

for path in "${SYSFS_PATHS[@]}"; do
    if [ -e "$path" ]; then
        echo -e "${BLUE}Found: $path${NC}"
        find "$path" -type f 2>/dev/null | while read -r file; do
            echo "  File: $file"
            cat "$file" 2>/dev/null | head -20 || echo "    (Cannot read)"
        done
        echo ""
    fi
done

###############################################################################
# 3. Devfreq RGA Information (frequency/load)
###############################################################################
echo -e "${GREEN}[3] Devfreq RGA Information${NC}"
echo "=========================================="

DEVFREQ_PATHS=(
    "/sys/class/devfreq/fdd90000.rga"
    "/sys/class/devfreq/rga"
)

for path in "${DEVFREQ_PATHS[@]}"; do
    if [ -d "$path" ]; then
        echo -e "${BLUE}Found devfreq at: $path${NC}"
        
        # Read all available parameters
        for param in cur_freq available_frequencies available_governors governor \
                     max_freq min_freq polling_interval trans_stat load; do
            file="$path/$param"
            if [ -f "$file" ]; then
                echo -e "${YELLOW}  $param:${NC}"
                cat "$file" 2>/dev/null | head -5 || echo "    (Cannot read)"
            fi
        done
        echo ""
    fi
done

###############################################################################
# 4. Procfs RGA Information
###############################################################################
echo -e "${GREEN}[4] Procfs RGA Information${NC}"
echo "=========================================="

# Check for any RGA-related proc entries
if ls /proc/rga* 2>/dev/null; then
    for file in /proc/rga*; do
        echo -e "${YELLOW}Found: $file${NC}"
        cat "$file" 2>/dev/null | head -50
        echo ""
    done
else
    echo "No RGA entries found in /proc"
fi

###############################################################################
# 5. Device Tree RGA Information
###############################################################################
echo -e "${GREEN}[5] Device Tree RGA Information${NC}"
echo "=========================================="

# Find RGA device nodes
DEVICE_NODES=$(find /sys/firmware/devicetree/base -name "*rga*" 2>/dev/null)
if [ ! -z "$DEVICE_NODES" ]; then
    echo "$DEVICE_NODES" | while read -r node; do
        echo -e "${BLUE}Device tree node: $node${NC}"
        # List properties
        ls -1 "$node" 2>/dev/null | while read -r prop; do
            if [ -f "$node/$prop" ]; then
                echo "  $prop:"
                cat "$node/$prop" 2>/dev/null | strings || cat "$node/$prop" 2>/dev/null | od -An -tx1 | head -2
            fi
        done
        echo ""
    done
else
    echo "No RGA device tree nodes found"
fi

###############################################################################
# 6. Kernel Modules Information
###############################################################################
echo -e "${GREEN}[6] Kernel Modules Information${NC}"
echo "=========================================="

echo "Loaded RGA modules:"
lsmod | grep -i rga || echo "  No RGA modules loaded"
echo ""

echo "Module details:"
modinfo rga 2>/dev/null || echo "  Cannot get module info"
echo ""

###############################################################################
# 7. DMA-BUF RGA Information
###############################################################################
echo -e "${GREEN}[7] DMA-BUF Information${NC}"
echo "=========================================="

if [ -d "/sys/kernel/debug/dma_buf" ]; then
    echo "DMA-BUF buffers (first 20):"
    ls /sys/kernel/debug/dma_buf 2>/dev/null | head -20
    
    # Try to get buffer info (may need root)
    if [ -f "/sys/kernel/debug/dma_buf/bufinfo" ]; then
        echo ""
        echo "Buffer info:"
        cat /sys/kernel/debug/dma_buf/bufinfo 2>/dev/null | head -50 || echo "  (Permission denied)"
    fi
else
    echo "DMA-BUF debugfs not available"
fi
echo ""

###############################################################################
# 8. Performance Counters (if available)
###############################################################################
echo -e "${GREEN}[8] Performance Counters${NC}"
echo "=========================================="

PERF_PATHS=(
    "/sys/kernel/debug/rkrga/perf"
    "/sys/kernel/debug/rkrga/monitor"
    "/sys/kernel/debug/rkrga/sessions"
)

for path in "${PERF_PATHS[@]}"; do
    if [ -e "$path" ]; then
        echo -e "${BLUE}$path:${NC}"
        cat "$path" 2>/dev/null || echo "  (Cannot read)"
        echo ""
    fi
done

###############################################################################
# 9. Runtime Information (requires active usage)
###############################################################################
echo -e "${GREEN}[9] Runtime Utilization (from debugfs/load)${NC}"
echo "=========================================="

LOAD_FILE="/sys/kernel/debug/rkrga/load"
if [ -f "$LOAD_FILE" ]; then
    echo "Current RGA load (5 samples, 0.5s interval):"
    for i in {1..5}; do
        printf "  Sample $i: "
        cat "$LOAD_FILE" 2>/dev/null || echo "Cannot read"
        sleep 0.5
    done
else
    echo "Load file not found at $LOAD_FILE"
fi
echo ""

###############################################################################
# 10. Summary of Useful Paths
###############################################################################
echo -e "${GREEN}[10] Summary - Recommended Monitoring Paths${NC}"
echo "=========================================="

RECOMMENDED_PATHS=(
    "/sys/kernel/debug/rkrga/load:Real-time utilization"
    "/sys/kernel/debug/rkrga/version:Driver version"
    "/sys/kernel/debug/rkrga/dump:Hardware info"
    "/sys/class/devfreq/fdd90000.rga/cur_freq:Current frequency"
    "/sys/class/devfreq/fdd90000.rga/load:Load percentage"
)

echo "Best paths for monitoring:"
for entry in "${RECOMMENDED_PATHS[@]}"; do
    IFS=':' read -r path desc <<< "$entry"
    if [ -e "$path" ]; then
        echo -e "  ${GREEN}✓${NC} $path"
        echo "    → $desc"
    else
        echo -e "  ${YELLOW}✗${NC} $path (not found)"
        echo "    → $desc"
    fi
done

echo ""
echo "=========================================="
echo "RGA Information Collection Complete"
echo "=========================================="
