# TrafficMonitor Awake

TrafficMonitor 的 Windows 开源插件，用于临时阻止系统自动休眠，并可选保持显示器开启。项目使用纯 Win32 C++17，不包含常驻服务、外部运行时或 PowerToys 二进制文件。

## 功能

- 关闭、无限保持、定时保持、保持到指定时间
- 独立控制“保持屏幕开启”
- 状态写入 `TrafficMonitorAwake.ini`，重启后恢复未过期设置
- 单行和双行均使用自适应扁平胶囊图标；状态、错误和屏幕开启状态可直接识别
- 左键快速切换，右键提供预设菜单和设置窗口
- 支持 TrafficMonitor API 8、x64 和 Win32

## 安装

1. 从 GitHub Releases 下载与 TrafficMonitor 位数匹配的 DLL。
2. 将 `TrafficMonitorAwake.dll` 复制到 `TrafficMonitor.exe/plugins/`。
3. 重启 TrafficMonitor，并在插件管理和显示项设置中启用插件。

不要将 x64 DLL 放入 32 位 TrafficMonitor，反之亦然。插件不需要管理员权限；Windows 电源策略仍可能限制 Modern Standby 设备的行为。

## 从源码构建

环境要求：Windows 10/11、Visual Studio 2022 Desktop development with C++、CMake 3.20+。项目最低目标 API 为 Windows 7。

```powershell
./build.ps1 -Arch x64 -Config Release
./build.ps1 -Arch Win32 -Config Release
./build-all.ps1
```

构建结果位于 `dist/x64/` 和 `dist/Win32/`。验证导出符号：

```powershell
./verify-build.ps1 -Arch x64
./verify-build.ps1 -Arch Win32
```

图标预览工具位于 `tools/icon-preview/`，仅用于开发验证，不会随插件安装。

## 验证与故障排查

安装后运行 `powercfg /requests`，应看到 TrafficMonitor 的系统电源请求；开启保持屏幕开启后还应看到显示器请求。报告问题时请附插件版本、Windows/TrafficMonitor 版本、架构、复现步骤和相关输出。

## 项目结构

```text
src/                    插件实现、状态管理、设置窗口和 API 8 接口
tools/icon-preview/     GDI+ 图标预览工具
.github/workflows/      GitHub Actions 双架构构建
build*.ps1              本地构建脚本
verify-build.ps1        DLL 导出检查
REFERENCE.md            独立实现与 PowerToys Awake 的差异
```

## 参与贡献

请阅读 [CONTRIBUTING.md](CONTRIBUTING.md) 和 [AGENTS.md](AGENTS.md)。提交 PR 前至少完成双架构构建和导出检查；UI 改动请附浅色/深色背景截图。安全问题请遵循 [SECURITY.md](SECURITY.md)，不要直接公开利用细节。

## 许可证

本项目采用 [MIT License](LICENSE)。TrafficMonitor、Windows 和 PowerToys Awake 均属于各自权利方；本项目仅使用公开接口并独立实现插件功能。
