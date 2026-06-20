# fbterm-mod

基于 [FbTerm](https://github.com/izmntuk/fbterm) 的增强版 Linux 帧缓冲终端模拟器，专注于 ARM 嵌入式设备的性能优化。

## 新增特性

### 渲染增强

- **粗体/斜体渲染** — 合成加粗（相邻像素贡献 75%）和斜体倾斜，支持独立配置粗体和斜体字体
- **下划线/删除线** — 支持 SGR 4/24/9/29 转义序列及 terminfo 扩展
- **真彩色 (24-bit)** — SGR 38;2/48;2 直接颜色序列，256 色调色板
- **Hex 颜色** — `#RRGGBB` 格式的 `color-foreground`/`color-background` 配置
- **Sixel 图形** — 完整的 DEC sixel 位图图形支持（DCS 序列解析、HLS/RGB 色彩空间、每单元像素图、BGRA 帧缓冲渲染、VT340 调色板）
- **CJK 扩展 B-F** — 通过动态字体回退支持超大字符集
- **Shift/Ctrl+方向键** — 标准 xterm 扩展转义序列

### 性能优化

- **ARM NEON SIMD** — `vld4`/`vst1q`/`vbsl` sixel 渲染（2-3x 加速），`vdup`/`vst1q` 帧缓冲填充，通过 `--enable-neon` 配置启用
- **双缓冲 + VSYNC** — 系统内存后备缓冲，一次性 memcpy 交换消除画面撕裂；`FBIO_WAITFORVSYNC` 垂直同步支持
- **内存访问优化** — 4 像素批量处理、预计算 alpha 混合差值、内联 RenderColor RGB 消除指针解引用、`__builtin_prefetch` 预取
- **ARM 交叉编译** — 支持 armhf (ARMv7 hard-float) 构建

### 字体与显示

- **可配置多字体** — `font-names`、`font-names-bold`、`font-names-italic` 独立设置
- **字体宽度/高度覆盖** — `font-width`、`font-height` 选项
- **字体感知字符宽度** — 根据实际字体字形度量计算字符宽度
- **可配置调色板** — `color-0` 到 `color-15` 自定义 16 色调色板
- **旋转显示** — 90°/180°/270° 屏幕旋转支持

### 输入与兼容性

- **Shift+方向键** — 标准 xterm 扩展转义序列 `\e[1;2A/B/C/D`
- **Ctrl+方向键** — 标准 xterm 扩展转义序列 `\e[1;5A/B/C/D`
- **INPUT METHOD 输入法** — 中文输入法支持

### 兼容性

- 默认 TERM 设置为 `fbterm`
- 避免控制台切换快捷键干扰
- 修复输入法 `Ctrl+Space` 切换时出现意外字符

## 依赖

- freetype2
- fontconfig
- libx86（可选，VESA 支持）
- gpm（可选，鼠标支持）

## 编译

### 本地编译

```bash
autoreconf -fi
./configure
make
sudo make install
```

### ARM 交叉编译 (armhf)

```bash
autoreconf -fi
./configure --host=arm-linux-gnueabihf --enable-neon
make
```

交叉编译时需要设置正确的 pkg-config 路径：

```bash
PKG_CONFIG_PATH=/path/to/sysroot/usr/lib/pkgconfig \
PKG_CONFIG_SYSROOT_DIR=/path/to/sysroot \
./configure --host=arm-linux-gnueabihf --enable-neon
```

### 配置选项

| 选项 | 说明 |
|------|------|
| `--enable-neon` | 启用 ARM NEON SIMD 优化（默认自动检测） |
| `--disable-neon` | 禁用 NEON 优化 |
| `--disable-gpm` | 禁用 gpm 鼠标支持 |
| `--disable-vesa` | 禁用 VESA 显卡支持 |
| `--disable-epoll` | 不使用 epoll 系统调用 |
| `--disable-signalfd` | 不使用 signalfd 系统调用 |

## 使用

```
fbterm-mod [选项] [--] [命令 [参数]]
```

配置文件位于 `~/.fbtermrc`，常见选项示例：

```
# 字体设置
font-names=DejaVu Sans Mono
font-names-bold=DejaVu Sans Mono Bold
font-names-italic=DejaVu Sans Mono Oblique
font-size=14

# 颜色设置
color-foreground=#D3D7CF
color-background=black

# CJK 支持
ambiguous-wide=yes
font-based-width=yes
```

详见 `man fbterm-mod` 和 `~/.fbtermrc` 注释。

## License

GPLv2+
