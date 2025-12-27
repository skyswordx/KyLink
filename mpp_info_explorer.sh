#!/bin/bash

echo "================================================================================"
echo "RK3566 MPP/VPU Information Explorer"
echo "================================================================================"
echo ""

echo "--- MPP Service Basic Info ---"
echo "Version:"
cat /proc/mpp_service/version 2>/dev/null || echo "  Not available"
echo ""

echo "Supported Devices:"
cat /proc/mpp_service/supports-device 2>/dev/null || echo "  Not available"
echo ""

echo "Supported Commands:"
cat /proc/mpp_service/supports-cmd 2>/dev/null || echo "  Not available"
echo ""

echo "Sessions Summary:"
cat /proc/mpp_service/sessions-summary 2>/dev/null || echo "  Not available"
echo ""

echo "Timing Enabled:"
cat /proc/mpp_service/timing_en 2>/dev/null || echo "  Not available"
echo ""

echo "================================================================================"
echo "--- JPEG Decoder (jpegd) ---"
echo "================================================================================"
if [ -d "/proc/mpp_service/jpegd" ]; then
    for file in /proc/mpp_service/jpegd/*; do
        if [ -f "$file" ]; then
            echo "File: $(basename $file)"
            cat "$file" 2>/dev/null | head -50
            echo ""
        fi
    done
else
    echo "  Directory not found"
fi
echo ""

echo "================================================================================"
echo "--- Video Decoder (rkvdec0) ---"
echo "================================================================================"
if [ -d "/proc/mpp_service/rkvdec0" ]; then
    for file in /proc/mpp_service/rkvdec0/*; do
        if [ -f "$file" ]; then
            echo "File: $(basename $file)"
            cat "$file" 2>/dev/null | head -50
            echo ""
        fi
    done
else
    echo "  Directory not found"
fi
echo ""

echo "================================================================================"
echo "--- Video Encoder (rkvenc) ---"
echo "================================================================================"
if [ -d "/proc/mpp_service/rkvenc" ]; then
    for file in /proc/mpp_service/rkvenc/*; do
        if [ -f "$file" ]; then
            echo "File: $(basename $file)"
            cat "$file" 2>/dev/null | head -50
            echo ""
        fi
    done
else
    echo "  Directory not found"
fi
echo ""

echo "================================================================================"
echo "--- VDPU (Video Decode Processing Unit) ---"
echo "================================================================================"
if [ -d "/proc/mpp_service/vdpu" ]; then
    for file in /proc/mpp_service/vdpu/*; do
        if [ -f "$file" ]; then
            echo "File: $(basename $file)"
            cat "$file" 2>/dev/null | head -50
            echo ""
        fi
    done
else
    echo "  Directory not found"
fi
echo ""

echo "================================================================================"
echo "--- VEPU (Video Encode Processing Unit) ---"
echo "================================================================================"
if [ -d "/proc/mpp_service/vepu" ]; then
    for file in /proc/mpp_service/vepu/*; do
        if [ -f "$file" ]; then
            echo "File: $(basename $file)"
            cat "$file" 2>/dev/null | head -50
            echo ""
        fi
    done
else
    echo "  Directory not found"
fi
echo ""

echo "================================================================================"
echo "--- IEP (Image Enhancement Processor) ---"
echo "================================================================================"
if [ -d "/proc/mpp_service/iep" ]; then
    for file in /proc/mpp_service/iep/*; do
        if [ -f "$file" ]; then
            echo "File: $(basename $file)"
            cat "$file" 2>/dev/null | head -50
            echo ""
        fi
    done
else
    echo "  Directory not found"
fi
echo ""

echo "================================================================================"
echo "--- SYS Class MPP Service ---"
echo "================================================================================"
if [ -d "/sys/class/mpp_class/mpp_service" ]; then
    echo "Available attributes:"
    ls -la /sys/class/mpp_class/mpp_service/ 2>/dev/null
    echo ""
    for file in /sys/class/mpp_class/mpp_service/*; do
        if [ -f "$file" ] && [ -r "$file" ]; then
            echo "Attribute: $(basename $file)"
            cat "$file" 2>/dev/null | head -20
            echo ""
        fi
    done
else
    echo "  Directory not found"
fi
echo ""

echo "================================================================================"
echo "--- Device File ---"
echo "================================================================================"
ls -la /dev/mpp_service 2>/dev/null || echo "  Device not found"
echo ""

echo "================================================================================"
echo "Script completed"
echo "================================================================================"
