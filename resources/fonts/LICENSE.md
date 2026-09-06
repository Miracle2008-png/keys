# Bundled fonts

Keys ships two typefaces so that it looks the same on every machine, rather than
falling back to whatever the system happens to provide.

| Family | Files | Used for |
|---|---|---|
| **Inter** | `Inter-Regular.otf`, `Inter-SemiBold.otf`, `Inter-Italic.otf` | Interface chrome: menus, labels, panels |
| **JetBrains Mono** | `JetBrainsMono-Regular.ttf`, `-Medium.ttf`, `-Bold.ttf`, `-Italic.ttf` | Code, the terminal, keycaps, paths |

Both are licensed under the **SIL Open Font License, Version 1.1**, which permits
bundling and redistribution with an application. The licence requires that this
notice travel with the fonts, that they are not sold on their own, and that any
modified version be released under a different name. Keys ships them unmodified.

- Inter — Copyright (c) 2016 The Inter Project Authors
  <https://github.com/rsms/inter>
- JetBrains Mono — Copyright (c) 2020 The JetBrains Mono Project Authors
  <https://github.com/JetBrains/JetBrainsMono>

The full licence text is available at <https://openfontlicense.org>.

Only the weights Keys actually uses are bundled. Both families ship many more;
carrying them all would add megabytes to the installer for faces nothing asks
for. If a weight is added to the design, add the file here rather than
substituting a synthesised bold or italic, which is what Qt does otherwise and
which looks visibly wrong in a monospaced face.
