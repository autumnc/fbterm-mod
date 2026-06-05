# fbterm-mod

A fast framebuffer/vesa based terminal emulator for Linux, forked from [FbTerm](https://github.com/izmntuk/fbterm).

## Improvements over upstream

- **Configurable 16-color palette** — set custom colors via `color-0` through `color-15` in `~/.fbterm-modrc`
- **Font-based character width** (`font-based-width=yes`) — use actual font glyph metrics to determine character width, ideal for non-fixed-width fonts
- **Ambiguous-wide character support** (`ambiguous-wide=yes`) — treat CJK ambiguous-width characters as wide
- **Hardcoded font hinting** to normal level for consistent rendering
- **Default TERM set to `fbterm`** instead of `linux`, for better compatibility
- **Avoid accidental vconsole switching** via keyboard shortcuts
- **Fixed** unexpected character appearing when toggling input method with `Ctrl+Space`

## Dependencies

- freetype2
- fontconfig
- libx86 (optional, for VESA support)
- gpm (optional, for mouse support)

## Build

```bash
autoreconf -i
./configure
make
sudo make install
```

## Usage

```
fbterm-mod [options] [--] [command [arguments]]
```

See `man fbterm-mod` and comments in `~/.fbterm-modrc` for configuration details.

## License

GPLv2+
