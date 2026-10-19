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
#include <elysia_gui/text.hpp>

// words 是已加载的 elysia::text::Catalog。
elysia::gui::text(words, {"demo.greeting", "Welcome aboard."});
elysia::gui::text_wrapped(words, {"demo.help", "Change language, then try the button."});
if (ImGui::Button(words.label({"demo.start", "Start"}))) {
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

两个字符串分别是固定的词条名和缺少翻译时的英文。绘图函数直接显示查到的文字，
其中的 `%` 不会当作格式指令。`label()` 用于现有的 ImGui 按钮、窗口和其他控件，
自动保留固定 ID；不需要给每种控件再造一个翻译包装。

## 初始化只做一次

```cpp
app.add_plugin(elysia::text::Plugin{});
auto& words = *app.world().resources().get<elysia::text::Catalog>();
words.load(locale_directory);
words.select("zh-CN");
```

独立工具也可以直接创建 `elysia::text::Catalog words;`，不必安装 ECS 插件。
插件只是帮 App 保存同一份词典，方便多个面板共享。`elysia-text` 负责读词典和查文字；
可选的 `elysia_gui/text.hpp` 负责调用 ImGui 显示它，不改仿真或渲染后端。
使用这份 GUI 头文件的目标需要同时链接 `elysia-gui` 和 `elysia-text`。

用户点击语言选项时先记录请求，下一帧的 `prepare()` 再调用 `select()`。
示例中的 `State::language` 就是这个待应用的请求。这样一帧中的词条指针一直有效。

示例不需要 manifest、mod DLL 或 playset。先理解这一个窗口，再把同样的调用放进面板。
