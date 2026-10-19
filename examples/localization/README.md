# 可运行的多语言窗口

从 ElysiaApp 根目录运行：

```sh
xmake build -P . -y -j6 elysia-text-example
xmake run -P . elysia-text-example
```

前提是 ElysiaApp 已完成正常的 `xmake f -P . -y -m release --gui=y` 配置，
使用默认相邻的 ElysiaECS_Hpp。例子只有一个 `main.cpp` 和 `locales/`，不依赖 Fleet。
在窗口里选择 English 或 Français，然后按按钮。切换语言不会清除运行状态。

中文还需要包含中文字形的字体。已有 Fleet 工作区时可以这样运行：

```sh
xmake run -P . elysia-text-example examples/localization/locales --gui-font ../ElysiaFleetNextGen/data/fonts/NotoSansCJKsc-Regular.otf
```

也可以把最后的路径换成自己的中文字体文件。词典不提供字体；缺字不是翻译失败。
运行目录由 xmake 设为项目根目录。直接启动 exe 时，第一个参数必须明确传词典目录，
后续可传 `--gui-font FONT_FILE`、`--gui-hidden`、`--gui-frames 5` 等 GUI 参数。
例如在 Windows 项目根目录：

```powershell
.\build\windows\x64\release\elysia-text-example.exe .\examples\localization\locales
```

## 写界面只看这几行

```cpp
#include <elysia_text/i18n.hpp>

// 当前 App 的资源；这里只借用引用，不复制词典。
const auto& TR = *app.world().resources().get<elysia::text::I18nManager>();
ImGui::TextUnformatted(TR("demo.greeting", "Welcome aboard."));
ImGui::TextWrapped("%s", TR("demo.help", "Change language, then try the button."));
if (ImGui::Button(TR.t_id("demo.start", "Start"))) {
    // 原来的按钮操作。
}
```

同名键写入 `locales/zh-CN.yaml`：

```yaml
_name: "简体中文"
demo.greeting: "欢迎登舰。"
demo.help: "切换语言，再试试按钮。"
demo.start: "开始"
```

`TR` 是 `I18nManager` 的局部引用，对象的 `operator()` 查询它的当前语言。
也支持 RTScope 风格的 `TR("Start")`，这时英文原文同时作为键和回退文字，
对应 YAML 为 `Start: "开始"`。已有带命名空间的符号键可以继续使用，不必重命名。

`TR.t_id(...)` 只为现有 ImGui 控件生成带固定 `###id` 的字符串，不绘制控件。
普通文字用 `TR(...)`。全部绘制、布局和交互仍然直接调用 ImGui。

## 初始化只做一次

```cpp
app.add_plugin(elysia::text::Plugin{});
auto& i18n = *app.world().resources().get<elysia::text::I18nManager>();
i18n.load(locale_directory);
i18n.select("zh-CN");
```

独立工具也可以直接创建 `elysia::text::I18nManager i18n;`。
管理器自己持有词典和当前语言，App 插件只是帮多个面板和 DLL 共享这一份资源。
没有全局单例、隐式绑定、GUI 包装函数或新的控件框架。

用户点击语言选项时先记录请求，下一帧的 `prepare()` 再调用 `select()`。
示例中的 `State::language` 就是待应用的请求。这样一帧中的词条指针一直有效。
使用者链接 `elysia-text`；GUI 应用同时使用已有的 `elysia-gui`。

示例不需要 manifest、mod DLL 或 playset。先运行这一个窗口，再使用相同写法修改面板。
