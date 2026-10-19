# Elysia text module

`elysia-text` provides `elysia::text::I18nManager` in `<elysia_text/i18n.hpp>`.
It owns the current language and loaded dictionaries. It contains no GUI widgets or renderer.

```cpp
#include <elysia_text/i18n.hpp>

elysia::text::I18nManager i18n;
i18n.load(locale_directory);
i18n.select("zh-CN");

// Inside a panel: TR is a reference, not a global function or singleton.
const auto& TR = i18n;
ImGui::TextUnformatted(TR("Welcome"));
ImGui::TextWrapped("%s", TR("demo.help", "Choose a language, then press Start."));
if (ImGui::Button(TR.t_id("Start"))) {
    // Existing operation.
}
```

`TR("Welcome")` uses the English text as its lookup key and fallback, like RTScope.
`TR("demo.help", "Choose a language, then press Start.")` uses an explicit stable key and
English fallback. Both query the same dictionary and selected language. The named
`i18n.translate(key, fallback)` method is equivalent to the call operator.

`TR.t_id(key, fallback)` returns the translation followed by `###key` to keep an ImGui
control's ID stable when its caption changes. Repeated rows still need their normal `PushID`
scopes. Use ordinary ImGui functions for all drawing. Pass translated text through
`TextUnformatted` or a `%s` argument when it is not meant to be a printf format string.

## Dictionaries

A folder contains `en.yaml`, `zh-CN.yaml`, etc. Files are flat string mappings:

```yaml
_name: "简体中文"
Welcome: "欢迎"
Start: "开始"
demo.help: "请选择语言，再按开始。"
demo.briefing: |
  第一行。
  第二行。
```

The filename is the language ID; `_name` is its name in the language selector. New languages
require only another file, not an enum or C++ change. English can live in source fallbacks,
so `en.yaml` may contain only `_name: English`. Resolution is selected language, then English,
then the source fallback (or key when omitted).

Later package dictionaries override earlier keys in load/dependency order. Prefix mod-owned
symbolic keys to avoid accidental collisions; intentional translation packs may override host
keys. Invalid files leave the complete live manager unchanged. A missing folder is optional.
Loaded strings are copied and owned by the manager, not borrowed from YAML or a requesting DLL.

## App resource and DLLs

Initialize once in the UI App:

```cpp
app.add_plugin(elysia::text::Plugin{});
auto& i18n = *app.world().resources().get<elysia::text::I18nManager>();
i18n.load(locale_directory);
i18n.select("zh-CN");
```

Panels and DLL tools get a reference to that resource and name it `TR` locally. Independent
Apps have independent managers; rebuilding an App discards its old dictionaries. There is no
process-wide or per-DLL singleton, implicit active-manager binding, or thread-local manager.
An ordinary tool can also construct an `I18nManager` directly, without ECS.

Use the resource on the UI thread. Apply `load` and `select` between frames; returned string
pointers must not be kept across either operation. Unresolved fallback pointers are borrowed
from the caller. The shared provider must remain loaded until the manager and its consumers
have been destroyed. YAML stays private to the provider; consumers include no parser headers.

`xmake sdk` installs the library, public header and yaml-cpp's license. UI consumers link
`elysia-text`; ImGui applications also use the existing GUI provider. No drawing adapter is
needed. This module does not implement plural rules, shaping or event serialization.

## Run the example

From the ElysiaApp root:

```sh
xmake build -P . -y -j6 elysia-text-example elysia-text-tests
xmake run -P . elysia-text-example
xmake run -P . elysia-text-tests
```

See the [window example](../../examples/localization/README.md) for Chinese font selection.
