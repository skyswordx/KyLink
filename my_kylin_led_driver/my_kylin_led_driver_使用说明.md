# my_kylin_led_driver 使用说明

## 模块简介
`my_kylin_led_driver` 是为 Firefly RK3588 平台定制的内核模块，通过字符设备 `/dev/kylin_led` 控制设备树中 `kylin:orange:skyswordx` 用户指示灯，并在操作期间暂时关闭 `firefly:blue:power` 电源指示灯以避免亮度冲突。

## 环境准备
- 已编译生成 `external/my_kylin_led_driver/my_kylin_led_driver.ko`
- 开发板运行内核版本需与构建时一致（示例：`5.10.198-skyswordx`）
- 拥有 root 权限或具备加载内核模块的能力

## 部署步骤
1. 将 `my_kylin_led_driver.ko` 拷贝到开发板，例如：
   ```bash
   scp external/my_kylin_led_driver/my_kylin_led_driver.ko root@BOARD_IP:/root/
   ```
2. 登录开发板终端，加载模块：
   ```bash
   sudo insmod ./my_kylin_led_driver.ko
   dmesg | tail -n 20
   ```
   若加载成功，日志会提示设备注册完成。若提示 “unable to locate LED …”，请检查 `dmesg` 输出的 LED 名称列表及设备树标签是否一致。
3. 查看字符设备是否存在：
   ```bash
   ls -l /dev/kylin_led
   ```
4. 卸载模块：
   ```bash
   sudo rmmod my_kylin_led_driver
   ```

## 终端快速测试
| 指令 | 功能 |
|------|------|
| `echo 1 | sudo tee /dev/kylin_led` | 点亮用户 LED，电源 LED 保持关闭 |
| `echo 0 | sudo tee /dev/kylin_led` | 关闭用户 LED，并恢复电源 LED |
| `echo 5 | sudo tee /dev/kylin_led` | 闪烁用户 LED 5 次，闪烁完成后恢复电源 LED |

> 注意：输入值小于等于 0 会关闭 LED；大于 1 的数值会作为闪烁次数（上限 100），每次闪烁 200 ms 亮 / 200 ms 灭。

## C++ 程序示例
```cpp
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <string>

class KylinLed {
public:
    explicit KylinLed(const char* path = "/dev/kylin_led") {
        fd_ = open(path, O_WRONLY);
        if (fd_ < 0) {
            throw std::runtime_error("无法打开 /dev/kylin_led");
        }
    }

    ~KylinLed() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    void setSolid() { writeValue("1\n"); }
    void setOff() { writeValue("0\n"); }

    void blink(int count) {
        if (count < 0) count = 0;
        if (count > 100) count = 100;
        writeValue(std::to_string(count) + "\n");
    }

private:
    void writeValue(const std::string& value) {
        if (write(fd_, value.c_str(), value.size()) != static_cast<ssize_t>(value.size())) {
            throw std::runtime_error("写入 /dev/kylin_led 失败");
        }
    }

    int fd_;
};

int main() {
    try {
        KylinLed led;
        led.setSolid();
        sleep(2);
        led.blink(3);
        led.setOff();
    } catch (const std::exception& e) {
        // 在此处理错误，例如打印日志
    }
    return 0;
}
```

编译运行：
```bash
g++ led_test.cpp -o led_test
sudo ./led_test
```
确保在运行前模块已加载并且 `/dev/kylin_led` 可用。

## 常见问题
- **加载失败，提示 No such device：**
  - 使用 `dmesg` 查看驱动列出的 LED 名称，核对设备树及系统中真正的 LED 名称。
  - 确认设备树已刷新到设备（重新打包 `boot.img` 并刷机）。
- **写入失败：**
  - 确认以 root 权限访问 `/dev/kylin_led`。
  - 确认模块仍然加载且未被卸载。

## 应用集成建议
- 可在系统启动脚本（如 `/etc/rc.local`）中添加 `insmod`，自动加载驱动。
- 若需要包管理，可将模块纳入 DKMS 或固件镜像中，随系统升级。
- 在多线程应用中，自行管理对 `/dev/kylin_led` 的访问，避免频繁打开/关闭造成资源浪费。
