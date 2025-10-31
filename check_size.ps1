# PowerShell 快速检查脚本 - 检查项目大小和优化效果
# 用于验证 RK3566 部署前的优化效果

param(
    [switch]$Verbose
)

$projectRoot = Split-Path -Parent $PSScriptRoot
$chatroomPath = Join-Path $projectRoot "Chatroom"

Write-Host "`n=== Kylin Messenger 项目大小检查 ===" -ForegroundColor Cyan
Write-Host "项目路径: $chatroomPath`n" -ForegroundColor Gray

# 1. 检查总大小
Write-Host "[1] 项目总大小:" -ForegroundColor Yellow
$allFiles = Get-ChildItem -Path $chatroomPath -Recurse -File -ErrorAction SilentlyContinue
$totalSize = ($allFiles | Measure-Object -Property Length -Sum).Sum
$fileCount = $allFiles.Count
Write-Host "   文件数: $fileCount" -ForegroundColor White
Write-Host "   总大小: $([math]::Round($totalSize/1MB, 2)) MB" -ForegroundColor White

# 2. 检查 Python 缓存
Write-Host "`n[2] Python 缓存检查:" -ForegroundColor Yellow
$pycacheDirs = Get-ChildItem -Path $chatroomPath -Recurse -Directory -Filter "__pycache__" -ErrorAction SilentlyContinue
if ($pycacheDirs) {
    $cacheSize = ($pycacheDirs | Get-ChildItem -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    Write-Host "   发现缓存: $([math]::Round($cacheSize/1MB, 2)) MB" -ForegroundColor Red
    Write-Host "   建议: 运行 cleanup.ps1 清理缓存" -ForegroundColor Yellow
} else {
    Write-Host "   ✓ 无缓存文件" -ForegroundColor Green
}

# 3. 检查表情包大小
Write-Host "`n[3] 表情包资源:" -ForegroundColor Yellow
$emojiPath = Join-Path $chatroomPath "resources\emojis\gifs"
if (Test-Path $emojiPath) {
    $emojiFiles = Get-ChildItem -Path $emojiPath -Filter "*.gif" -ErrorAction SilentlyContinue
    if ($emojiFiles) {
        $emojiSize = ($emojiFiles | Measure-Object -Property Length -Sum).Sum
        $avgSize = [math]::Round($emojiSize / $emojiFiles.Count / 1KB, 1)
        Write-Host "   文件数: $($emojiFiles.Count)" -ForegroundColor White
        Write-Host "   总大小: $([math]::Round($emojiSize/1MB, 2)) MB" -ForegroundColor White
        Write-Host "   平均大小: $avgSize KB/个" -ForegroundColor White
        
        if ($emojiSize -gt 5MB) {
            Write-Host "   ⚠ 建议: 压缩 GIF 文件可节省 50-70% 空间" -ForegroundColor Yellow
        } else {
            Write-Host "   ✓ 表情包大小合理" -ForegroundColor Green
        }
    }
}

# 4. 检查代码文件
Write-Host "`n[4] 代码文件统计:" -ForegroundColor Yellow
$pyFiles = Get-ChildItem -Path $chatroomPath -Recurse -Filter "*.py" -ErrorAction SilentlyContinue
$pySize = ($pyFiles | Measure-Object -Property Length -Sum).Sum
Write-Host "   Python 文件: $($pyFiles.Count) 个" -ForegroundColor White
Write-Host "   代码大小: $([math]::Round($pySize/1MB, 2)) MB" -ForegroundColor White

# 5. 优化建议
Write-Host "`n[5] 优化建议:" -ForegroundColor Yellow
$suggestions = @()

if ($pycacheDirs) {
    $suggestions += "运行 cleanup.ps1 清理缓存"
}

if ($emojiSize -gt 5MB) {
    $suggestions += "压缩表情包 GIF 文件（目标：减少 50%+）"
}

if ($totalSize -gt 100MB) {
    $suggestions += "考虑删除不必要的资源文件"
}

if ($suggestions.Count -eq 0) {
    Write-Host "   ✓ 项目已优化，无需额外操作" -ForegroundColor Green
} else {
    foreach ($suggestion in $suggestions) {
        Write-Host "   • $suggestion" -ForegroundColor Cyan
    }
}

# 6. 详细统计（如果启用 -Verbose）
if ($Verbose) {
    Write-Host "`n[详细统计]" -ForegroundColor Yellow
    Write-Host "   按目录统计:" -ForegroundColor Gray
    
    $dirs = Get-ChildItem -Path $chatroomPath -Directory -ErrorAction SilentlyContinue
    foreach ($dir in $dirs) {
        $dirFiles = Get-ChildItem -Path $dir.FullName -Recurse -File -ErrorAction SilentlyContinue
        if ($dirFiles) {
            $dirSize = ($dirFiles | Measure-Object -Property Length -Sum).Sum
            if ($dirSize -gt 100KB) {
                $dirName = Split-Path $dir.FullName -Leaf
                Write-Host "   $dirName`: $([math]::Round($dirSize/1MB, 2)) MB" -ForegroundColor White
            }
        }
    }
}

# 7. 空间评估
Write-Host "`n[6] RK3566 部署评估:" -ForegroundColor Yellow
$estimatedSize = $totalSize + 100MB  # 加上 Python 运行时估算
Write-Host "   预计部署大小: $([math]::Round($estimatedSize/1MB, 2)) MB" -ForegroundColor White

if ($estimatedSize -lt 300MB) {
    Write-Host "   ✓ 空间充足（600MB 限制）" -ForegroundColor Green
} elseif ($estimatedSize -lt 500MB) {
    Write-Host "   ⚠ 空间紧张，建议进一步优化" -ForegroundColor Yellow
} else {
    Write-Host "   ✗ 超出限制，必须优化" -ForegroundColor Red
}

Write-Host "`n检查完成！" -ForegroundColor Green

