# PowerShell 清理脚本 - 清理项目缓存和临时文件
# 用于 RK3566 设备部署前的空间优化

Write-Host "开始清理项目缓存..." -ForegroundColor Green

$projectRoot = Split-Path -Parent $PSScriptRoot
$chatroomPath = Join-Path $projectRoot "Chatroom"

# 1. 删除所有 __pycache__ 文件夹
Write-Host "`n[1/4] 清理 Python 缓存文件..." -ForegroundColor Yellow
$pycacheDirs = Get-ChildItem -Path $chatroomPath -Recurse -Directory -Filter "__pycache__" -ErrorAction SilentlyContinue
if ($pycacheDirs) {
    $totalSize = 0
    foreach ($dir in $pycacheDirs) {
        $size = (Get-ChildItem $dir.FullName -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
        $totalSize += $size
        Remove-Item $dir.FullName -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "  删除: $($dir.FullName)" -ForegroundColor Gray
    }
    Write-Host "  清理完成，释放空间: $([math]::Round($totalSize/1MB, 2)) MB" -ForegroundColor Green
} else {
    Write-Host "  未找到缓存文件" -ForegroundColor Gray
}

# 2. 删除 .pyc 文件
Write-Host "`n[2/4] 清理 .pyc 文件..." -ForegroundColor Yellow
$pycFiles = Get-ChildItem -Path $chatroomPath -Recurse -Filter "*.pyc" -ErrorAction SilentlyContinue
if ($pycFiles) {
    $size = ($pycFiles | Measure-Object -Property Length -Sum).Sum
    $pycFiles | Remove-Item -Force -ErrorAction SilentlyContinue
    Write-Host "  清理完成，释放空间: $([math]::Round($size/1MB, 2)) MB" -ForegroundColor Green
} else {
    Write-Host "  未找到 .pyc 文件" -ForegroundColor Gray
}

# 3. 清理临时文件
Write-Host "`n[3/4] 清理临时文件..." -ForegroundColor Yellow
$tempFiles = @("*.tmp", "*.log", "*.bak", "*.swp")
$totalSize = 0
foreach ($pattern in $tempFiles) {
    $files = Get-ChildItem -Path $chatroomPath -Recurse -Filter $pattern -ErrorAction SilentlyContinue
    if ($files) {
        $size = ($files | Measure-Object -Property Length -Sum).Sum
        $totalSize += $size
        $files | Remove-Item -Force -ErrorAction SilentlyContinue
    }
}
if ($totalSize -gt 0) {
    Write-Host "  清理完成，释放空间: $([math]::Round($totalSize/1MB, 2)) MB" -ForegroundColor Green
} else {
    Write-Host "  未找到临时文件" -ForegroundColor Gray
}

# 4. 统计项目大小
Write-Host "`n[4/4] 统计项目大小..." -ForegroundColor Yellow
$allFiles = Get-ChildItem -Path $chatroomPath -Recurse -File -ErrorAction SilentlyContinue
$totalSize = ($allFiles | Measure-Object -Property Length -Sum).Sum
$fileCount = $allFiles.Count

Write-Host "`n项目统计:" -ForegroundColor Cyan
Write-Host "  总文件数: $fileCount" -ForegroundColor White
Write-Host "  总大小: $([math]::Round($totalSize/1MB, 2)) MB" -ForegroundColor White

# 统计表情包大小
$emojiPath = Join-Path $chatroomPath "resources\emojis\gifs"
if (Test-Path $emojiPath) {
    $emojiFiles = Get-ChildItem -Path $emojiPath -Filter "*.gif" -ErrorAction SilentlyContinue
    if ($emojiFiles) {
        $emojiSize = ($emojiFiles | Measure-Object -Property Length -Sum).Sum
        Write-Host "  表情包大小: $([math]::Round($emojiSize/1MB, 2)) MB ($($emojiFiles.Count) 个文件)" -ForegroundColor Yellow
        Write-Host "  提示: 如需进一步优化，可以考虑压缩 GIF 文件" -ForegroundColor Gray
    }
}

Write-Host "`n清理完成！" -ForegroundColor Green

