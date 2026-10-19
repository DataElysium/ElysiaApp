# Elysia text module

Start with the [buildable localized window](../../examples/localization/README.md):
`xmake build -P . -y -j6 elysia-text-example`, then `xmake run -P . elysia-text-example`
from the ElysiaApp root. It includes English/French/Chinese dictionaries and a language selector.
Chinese requires a CJK font; the example guide shows the font option.

For ordinary ImGui text, the optional `<elysia_gui/text.hpp>` supplies drawing functions:

```cpp
elysia::gui::text(text, {"demo.greeting", "Welcome"});
elysia::gui::text_wrapped(text, {"demo.help", "Choose a language, then press Start."});
if (ImGui::Button(text.label({"demo.start", "Start"}))) {
    // Existing operation.
}
```

Link both `elysia-gui` and `elysia-text` for this adapter. It is not included by `gui.hpp`, so
existing GUI consumers do not acquire a text dependency. The adapter draws strings literally;
use existing ImGui formatting calls explicitly when inserting numbers or other arguments.

`elysia-text` is a renderer-independent localization library. `xmake sdk` installs its shared
library, `elysia_text/catalog.hpp` and yaml-cpp's license. YAML is statically linked into this
provider; consumer DLLs neither include yaml-cpp nor build another parser.

Install `elysia::text::Plugin{}` in the UI App, then use its `Catalog` resource on the UI thread.
Alternatively, create a `Catalog` directly in an ordinary tool: ECS is not required for lookup.
The owning process must keep the provider loaded until App resources are destroyed, as with
the GUI provider. Fleet links both providers normally; they are runtime bundle dependencies.

```cpp
app.add_plugin(elysia::text::Plugin{});
auto& text = *app.world().resources().get<elysia::text::Catalog>();
text.load(data_directory / "locales");
text.select("zh-CN");
// Inside the UI frame:
ImGui::TextUnformatted(text.get({"demo.greeting", "Welcome"}));
ImGui::Button(text.label({"demo.play", "Play"}));
```

A folder contains `en.yaml`, `zh-CN.yaml`, etc. It is optional. Files are flat string mappings:

```yaml
_name: "简体中文" # Optional native language name in the selector.
demo.greeting: "欢迎"
demo.play: "开始"
demo.briefing: |
  第一行。
  第二行。
```

Keys are stable symbols; values are display text. English can live in source fallbacks, so an
English YAML file may contain only `_name: English`. Later package dictionaries override earlier
keys in load/dependency order. Prefix mod-owned keys to avoid accidental collisions; intentional
translation packs may override host keys. A malformed package is rejected before live text changes.

`get` resolves selected locale, then English, then the supplied source fallback. `label` adds
`###id` for ImGui and caches the result. Repeated rows still need normal `PushID` scopes. Never
cache returned pointers across `load` or `select`; apply language requests between frames.
The catalog owns all loaded strings. Dictionaries do not retain memory from the requesting DLL.
This first module handles text lookup, not plural rules, shaping or game event serialization.

```sh
xmake build -P . -y -j6 elysia-text-tests
./build/linux/x86_64/release/elysia-text-tests
```
