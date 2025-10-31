# RK3566 优化指南
# 针对 600MB 剩余空间限制的优化方案

## 已完成的优化

### 1. 代码优化
- ✅ 移除所有调试打印语句（减少输出和CPU占用）
- ✅ 实现 LRU 缓存策略（限制动画缓存大小为20个）
- ✅ 优化内存管理（及时释放未使用的动画对象）
- ✅ 延迟加载表情包（只在需要时加载）

### 2. 资源优化建议

#### 表情包优化
当前有 96 个 GIF 文件，建议：

1. **压缩 GIF 文件**
   ```powershell
   # 使用 ImageMagick 或在线工具压缩 GIF
   # 目标：每个 GIF < 50KB
   # 预计可节省 50-70% 空间
   ```

2. **减少表情包数量**
   - 保留最常用的 30-50 个表情
   - 删除不常用的表情文件
   - 更新 `emoji_map.json` 文件

3. **转换为静态图片**
   - 将不常用的 GIF 转换为 PNG/JPG
   - 可以节省更多空间（但失去动画效果）

#### 缓存清理
运行清理脚本：
```powershell
.\cleanup.ps1
```

### 3. 运行时优化

#### 内存限制
- 动画缓存已限制为最多 20 个
- 超出限制时自动清理最旧的缓存

#### 手动清理缓存
在聊天窗口中，可以调用：
```python
chat_window.message_display.clear_cache()  # 清理所有缓存
chat_window.message_display.cleanup_old_cache()  # 清理旧缓存
```

### 4. 部署建议

#### 最小化部署
1. 删除 `__pycache__` 文件夹（运行时自动生成）
2. 删除开发文档和 README（如不需要）
3. 压缩 Python 依赖（使用 pyinstaller --onefile）

#### 空间分配建议（600MB）
- 应用程序: ~50MB
- Python 运行时: ~100MB
- 依赖库: ~150MB
- 表情包资源: ~50MB（优化后）
- 缓存和临时文件: ~50MB
- 系统预留: ~200MB

### 5. 进一步优化（可选）

#### 使用压缩资源
- 将 GIF 文件打包为 ZIP
- 运行时解压到内存（需要更多内存）

#### 网络资源
- 表情包存储在外部服务器
- 按需下载（需要网络连接）

## 检查清单

- [ ] 运行 cleanup.ps1 清理缓存
- [ ] 压缩 GIF 文件（目标：减少 50%+）
- [ ] 删除不必要的表情文件
- [ ] 测试应用在 RK3566 上的运行情况
- [ ] 监控内存使用情况
- [ ] 确认功能正常

## 验证

部署后检查：
```powershell
# 检查项目大小
cd Chatroom
Get-ChildItem -Recurse -File | Measure-Object -Property Length -Sum | Select-Object @{Name="Size(MB)";Expression={[math]::Round($_.Sum/1MB,2)}}
```

目标：整个 Chatroom 文件夹 < 100MB（不含 Python 运行时）

