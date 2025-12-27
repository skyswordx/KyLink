#!/bin/bash

###############################################################################
# NPU Information Explorer for RK3566
# This script comprehensively collects NPU (Neural Processing Unit) 
# information from various kernel interfaces
###############################################################################

echo "=========================================="
echo "NPU Information Explorer for RK3566"
echo "=========================================="
echo ""

# Color codes for better readability
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

###############################################################################
# 1. Debugfs NPU Information
###############################################################################
echo -e "${GREEN}[1] Debugfs NPU Information${NC}"
echo "=========================================="

NPU_DEBUGFS="/sys/kernel/debug/rknpu"
if [ -d "$NPU_DEBUGFS" ]; then
    echo -e "${BLUE}Found NPU debugfs at: $NPU_DEBUGFS${NC}"
    echo ""
    
    # List all available files
    echo "Available files:"
    ls -la "$NPU_DEBUGFS"
    echo ""
    
    # Read each file
    for file in "$NPU_DEBUGFS"/*; do
        if [ -f "$file" ]; then
            filename=$(basename "$file")
            echo -e "${YELLOW}--- Contents of $filename ---${NC}"
            cat "$file" 2>/dev/null || echo "  (Permission denied or empty)"
            echo ""
        fi
    done
else
    echo -e "${RED}NPU debugfs not found at $NPU_DEBUGFS${NC}"
fi

###############################################################################
# 2. Sysfs NPU Information
###############################################################################
echo -e "${GREEN}[2] Sysfs NPU Information${NC}"
echo "=========================================="

# Common sysfs paths for NPU
SYSFS_PATHS=(
    "/sys/class/npu"
    "/sys/devices/platform/fde40000.npu"
    "/sys/devices/platform/npu"
    "/sys/devices/platform/fde40000.npu/npu"
    "/sys/class/misc/rknpu"
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
# 3. Devfreq NPU Information (frequency/load)
###############################################################################
echo -e "${GREEN}[3] Devfreq NPU Information${NC}"
echo "=========================================="

DEVFREQ_PATHS=(
    "/sys/class/devfreq/fde40000.npu"
    "/sys/class/devfreq/npu"
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
                cat "$file" 2>/dev/null | head -10 || echo "    (Cannot read)"
            fi
        done
        echo ""
    fi
done

###############################################################################
# 4. Procfs NPU Information
###############################################################################
echo -e "${GREEN}[4] Procfs NPU Information${NC}"
echo "=========================================="

# Check for any NPU-related proc entries
if ls /proc/npu* 2>/dev/null; then
    for file in /proc/npu*; do
        echo -e "${YELLOW}Found: $file${NC}"
        cat "$file" 2>/dev/null | head -50
        echo ""
    done
else
    echo "No NPU entries found in /proc"
fi

# Check for RKNN-specific proc entries
if ls /proc/rknn* 2>/dev/null; then
    for file in /proc/rknn*; do
        echo -e "${YELLOW}Found RKNN: $file${NC}"
        cat "$file" 2>/dev/null | head -50
        echo ""
    done
fi

###############################################################################
# 5. Device Tree NPU Information
###############################################################################
echo -e "${GREEN}[5] Device Tree NPU Information${NC}"
echo "=========================================="

# Find NPU device nodes
DEVICE_NODES=$(find /sys/firmware/devicetree/base -name "*npu*" -o -name "*rknn*" 2>/dev/null)
if [ ! -z "$DEVICE_NODES" ]; then
    echo "$DEVICE_NODES" | while read -r node; do
        echo -e "${BLUE}Device tree node: $node${NC}"
        # List properties
        ls -1 "$node" 2>/dev/null | while read -r prop; do
            if [ -f "$node/$prop" ]; then
                echo "  $prop:"
                # Try to display as string first, fallback to hex
                cat "$node/$prop" 2>/dev/null | strings || cat "$node/$prop" 2>/dev/null | od -An -tx1 | head -2
            fi
        done
        echo ""
    done
else
    echo "No NPU device tree nodes found"
fi

###############################################################################
# 6. Kernel Modules Information
###############################################################################
echo -e "${GREEN}[6] Kernel Modules Information${NC}"
echo "=========================================="

echo "Loaded NPU/RKNN modules:"
lsmod | grep -E "(npu|rknn|rknpu)" || echo "  No NPU/RKNN modules loaded"
echo ""

echo "Module details:"
for mod in rknpu galcore; do
    if lsmod | grep -q "$mod"; then
        echo -e "${YELLOW}--- $mod ---${NC}"
        modinfo "$mod" 2>/dev/null || echo "  Cannot get module info"
        echo ""
    fi
done

###############################################################################
# 7. Device Nodes
###############################################################################
echo -e "${GREEN}[7] Device Nodes${NC}"
echo "=========================================="

echo "NPU-related device nodes:"
ls -la /dev/*npu* /dev/*rknn* 2>/dev/null || echo "  No NPU device nodes found in /dev"
echo ""

# Check misc devices
if [ -f "/proc/misc" ]; then
    echo "Misc devices (NPU/RKNN):"
    grep -E "(npu|rknn)" /proc/misc 2>/dev/null || echo "  No NPU/RKNN in misc devices"
fi
echo ""

###############################################################################
# 8. NPU Memory Information
###############################################################################
echo -e "${GREEN}[8] NPU Memory Information${NC}"
echo "=========================================="

# Check CMA (Contiguous Memory Allocator) for NPU
if [ -d "/sys/kernel/debug/cma" ]; then
    echo "CMA information:"
    ls -la /sys/kernel/debug/cma/ 2>/dev/null
    for cma_area in /sys/kernel/debug/cma/*; do
        if [ -d "$cma_area" ]; then
            area_name=$(basename "$cma_area")
            echo -e "${YELLOW}CMA area: $area_name${NC}"
            for file in "$cma_area"/*; do
                if [ -f "$file" ]; then
                    filename=$(basename "$file")
                    echo "  $filename:"
                    cat "$file" 2>/dev/null || echo "    (Cannot read)"
                fi
            done
        fi
    done
else
    echo "CMA debugfs not available"
fi
echo ""

# Check NPU-specific memory from debugfs
if [ -d "/sys/kernel/debug/rknpu" ]; then
    echo "NPU memory stats from debugfs:"
    for mem_file in /sys/kernel/debug/rknpu/*mem* /sys/kernel/debug/rknpu/*alloc*; do
        if [ -f "$mem_file" ]; then
            echo -e "${YELLOW}$(basename "$mem_file"):${NC}"
            cat "$mem_file" 2>/dev/null || echo "  (Cannot read)"
        fi
    done
fi
echo ""

###############################################################################
# 9. Runtime Utilization and Performance
###############################################################################
echo -e "${GREEN}[9] Runtime Utilization (from debugfs)${NC}"
echo "=========================================="

LOAD_FILE="/sys/kernel/debug/rknpu/load"
if [ -f "$LOAD_FILE" ]; then
    echo "Current NPU load (5 samples, 0.5s interval):"
    for i in {1..5}; do
        printf "  Sample $i: "
        cat "$LOAD_FILE" 2>/dev/null || echo "Cannot read"
        sleep 0.5
    done
else
    echo "Load file not found at $LOAD_FILE"
fi
echo ""

# Check for performance monitoring files
PERF_FILES=(
    "/sys/kernel/debug/rknpu/perf"
    "/sys/kernel/debug/rknpu/runtime"
    "/sys/kernel/debug/rknpu/sessions"
    "/sys/kernel/debug/rknpu/status"
)

for file in "${PERF_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${YELLOW}$(basename "$file"):${NC}"
        cat "$file" 2>/dev/null || echo "  (Cannot read)"
        echo ""
    fi
done

###############################################################################
# 10. NPU Clock and Power Information
###############################################################################
echo -e "${GREEN}[10] Clock and Power Information${NC}"
echo "=========================================="

# Check clock information
CLOCK_PATHS=(
    "/sys/kernel/debug/clk/clk_npu"
    "/sys/kernel/debug/clk/aclk_npu"
    "/sys/kernel/debug/clk/hclk_npu"
)

echo "NPU clocks:"
for clk_path in "${CLOCK_PATHS[@]}"; do
    if [ -d "$clk_path" ]; then
        clk_name=$(basename "$clk_path")
        echo -e "${BLUE}$clk_name:${NC}"
        for file in "$clk_path"/*; do
            if [ -f "$file" ]; then
                filename=$(basename "$file")
                echo "  $filename: $(cat "$file" 2>/dev/null)"
            fi
        done
    fi
done
echo ""

# Check regulator/voltage
REGULATOR_PATHS=(
    "/sys/kernel/debug/regulator/vdd_npu"
    "/sys/class/regulator/regulator.*"
)

echo "NPU regulators:"
for reg_pattern in "${REGULATOR_PATHS[@]}"; do
    for reg_path in $reg_pattern; do
        if [ -e "$reg_path" ]; then
            # Check if this is NPU-related
            if [ -f "$reg_path/name" ]; then
                reg_name=$(cat "$reg_path/name" 2>/dev/null)
                if echo "$reg_name" | grep -qi "npu"; then
                    echo -e "${BLUE}Regulator: $reg_name${NC}"
                    for attr in microvolts state; do
                        if [ -f "$reg_path/$attr" ]; then
                            echo "  $attr: $(cat "$reg_path/$attr" 2>/dev/null)"
                        fi
                    done
                fi
            fi
        fi
    done
done
echo ""

###############################################################################
# 11. RKNN Toolkit Version and Driver Info
###############################################################################
echo -e "${GREEN}[11] RKNN Driver Version${NC}"
echo "=========================================="

# Check for version file
VERSION_FILES=(
    "/sys/kernel/debug/rknpu/version"
    "/sys/module/rknpu/version"
    "/sys/class/misc/rknpu/version"
)

for ver_file in "${VERSION_FILES[@]}"; do
    if [ -f "$ver_file" ]; then
        echo -e "${YELLOW}Version from $ver_file:${NC}"
        cat "$ver_file" 2>/dev/null
        echo ""
    fi
done

# Check module parameters
if [ -d "/sys/module/rknpu/parameters" ]; then
    echo "RKNPU module parameters:"
    for param in /sys/module/rknpu/parameters/*; do
        if [ -f "$param" ]; then
            param_name=$(basename "$param")
            param_value=$(cat "$param" 2>/dev/null)
            echo "  $param_name = $param_value"
        fi
    done
fi
echo ""

###############################################################################
# 12. NPU Session and Task Information
###############################################################################
echo -e "${GREEN}[12] Active Sessions and Tasks${NC}"
echo "=========================================="

SESSION_FILES=(
    "/sys/kernel/debug/rknpu/sessions"
    "/sys/kernel/debug/rknpu/tasks"
    "/sys/kernel/debug/rknpu/queue"
)

for file in "${SESSION_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${YELLOW}$(basename "$file"):${NC}"
        cat "$file" 2>/dev/null || echo "  (Cannot read)"
        echo ""
    fi
done

###############################################################################
# 13. Summary - Recommended Monitoring Paths
###############################################################################
echo -e "${GREEN}[13] Summary - Recommended Monitoring Paths${NC}"
echo "=========================================="

RECOMMENDED_PATHS=(
    "/sys/kernel/debug/rknpu/load:Real-time NPU utilization"
    "/sys/kernel/debug/rknpu/version:Driver version"
    "/sys/kernel/debug/rknpu/power_state:Power state (on/off)"
    "/sys/kernel/debug/rknpu/delay_ms:Processing delay"
    "/sys/class/devfreq/fde40000.npu/cur_freq:Current frequency"
    "/sys/class/devfreq/fde40000.npu/load:Load percentage"
    "/sys/kernel/debug/rknpu/mem_usage:Memory usage statistics"
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
echo "NPU Information Collection Complete"
echo "=========================================="
