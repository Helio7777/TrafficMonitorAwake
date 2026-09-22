# Contributing

感谢你为 TrafficMonitor Awake 提交改进。提交前请先阅读 `README.md` 和
`AGENTS.md`，确认改动符合 Windows、TrafficMonitor API 8 和 C++17 约束。

## 开发环境

- Windows 10/11（目标 API 最低为 Windows 7）
- Visual Studio 2022 Desktop development with C++
- CMake 3.20 或更高版本

## 提交改动

1. 从默认分支创建主题分支，例如 `fix/icon-scaling`。
2. 保持改动聚焦；不要提交 `build-*`、`dist` 或本地 DLL。
3. 使用四空格缩进、宽字符 Win32 API，并保持 `/W4` 无警告。
4. 至少运行：

   ```powershell
   ./build-all.ps1
   ./verify-build.ps1 -Arch x64
   ./verify-build.ps1 -Arch Win32
   ```

5. 涉及电源状态时，手动验证关闭、无限、定时、到期和保持屏幕开启模式，
   并检查 `powercfg /requests`。

## Pull Request

PR 描述应说明用户可见变化、测试架构和手动验证结果。UI 或图标改动请附
浅色/深色背景截图；行为变化请说明兼容的 TrafficMonitor 版本和 Windows
最低版本。提交信息使用简短的祈使句，例如 `Improve adaptive status icon`。
