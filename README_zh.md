# fbterm-mod

基于 [FbTerm](https://github.com/izmntuk/fbterm) 修改的 Linux 帧缓冲/vesa 终端模拟器。

## 新增特性

### 1. 基于字体度量的字符宽度（font-based-width）

传统终端对字符宽度采用硬编码的 Unicode 宽度表判断——字符要么是半角（宽 1），要么是全角（宽 2）。这种方式对等宽字体问题不大，但对非等宽字体或某些特殊字形（如 CJK 标点、特殊符号）会判断失准。

开启后，fbterm-mod 会通过 FreeType 获取每个字符字形的真实 advance 值（即字体实际渲染宽度），当该值超过字体标准宽度的 1.3 倍时，自动按全角处理：

```cpp
// wcwidth.cpp
s32 w = ambiguous_wide ? mk_wcwidth_cjk(ucs) : mk_wcwidth(ucs);
if (w == 1 && font_based_width) {
    s32 adv = Font::instance()->getAdvance(ucs);
    if (adv > (s32)(Font::instance()->width() * 1.3))
        return 2;  // 字体实际宽度较大，按全角显示
}
```

**适用场景：**
- 使用非等宽字体时，避免部分字符显示被截断
- CJK 扩展区字符在不同字体中宽度不一致时自动适配
- 特殊符号（数学符号、箭头等）宽度校正

### 2. 扩展区汉字 / 模糊宽度字符支持（ambiguous-wide）

Unicode 标准中有一类「东亚模糊宽度」（East Asian Ambiguous Width）字符，它们的宽度在不同上下文中可以是半角或全角，包括：

- 希腊字母、西里尔字母
- CJK 标点符号（如「」「」『』、。〝〞）
- 制表符号、框图符号
- 部分数学和货币符号
- Unicode 增补平面中的私有区字符（U+E000-U+F8FF, U+F0000-U+FFFFD, U+100000-U+10FFFD）

开启后，以上所有字符一律按全角处理，确保在中文环境中显示正常、对齐一致。

### 3. 可自定义的 16 色调色板

在 `~/.fbterm-modrc` 中通过 `color-0` 至 `color-15` 自定义终端 16 色，格式为十六进制 RRGGBB：

```
color-0=000000    # 黑色
color-1=AA0000    # 红色
color-2=00AA00    # 绿色
...
color-15=FFFFFF   # 白色
```

### 4. 其他改进

- **字体 hinting 固定为 normal**：避免某些 Infinality 配置下字体渲染异常
- **TERM 环境变量默认设为 `fbterm`**：不再伪装成 linux 终端，便于程序识别
- **Ctrl+Alt+F7~F12 切换编码**：避免与虚拟控制台切换 (Ctrl+Alt+F1~F6) 冲突
- **修复 Ctrl+Space 切换输入法时出现多余字符** 的问题

## 依赖

- freetype2
- fontconfig
- libx86（可选，vesa 支持）
- gpm（可选，鼠标支持）

## 编译

```bash
autoreconf -i
./configure
make
sudo make install
```

## 使用

```
fbterm-mod [选项] [--] [命令 [参数]]
```

### 命令行选项

| 选项 | 说明 |
|------|------|
| `-n, --font-names=TEXT` | 指定字体族名称，多个用逗号分隔 |
| `-s, --font-size=NUM` | 字体像素大小 |
| `--font-width=NUM` | 强制字体宽度 |
| `--font-height=NUM` | 强制字体高度 |
| `-f, --color-foreground=NUM` | 前景色 (0-7) |
| `-b, --color-background=NUM` | 背景色 (0-7) |
| `-a, --ambiguous-wide` | 模糊宽度字符按全角处理 |
| `-w, --font-based-width` | 基于字体度量决定字符宽度 |
| `-i, --input-method=TEXT` | 指定输入法程序 |
| `-e, --text-encodings=TEXT` | 额外文字编码 |
| `-r, --screen-rotate=NUM` | 屏幕旋转角度 (0-3) |
| `--cursor-shape=NUM` | 光标形状 (0=下划线, 1=方块) |
| `--cursor-interval=NUM` | 光标闪烁间隔（毫秒） |
| `-h, --help` | 显示帮助信息 |

### 配置文件

程序启动时读取 `~/.fbterm-modrc`，首次运行自动生成带注释的默认配置。推荐配置示例：

```
font-names=mono
font-size=12

# 启用新特性
ambiguous-wide=yes
font-based-width=yes

# 自定义配色
color-foreground=7
color-background=0

# 光标
cursor-shape=1
cursor-interval=500
```

## 许可证

GPLv2+
