# Evidence for PR #135

Captures of feat/pc-export at c5d5930, the PR's head. This branch holds only these files and is never merged.

- title_macos.png: `build/export/macos/Murim24.app` from `make export`, launched with `open -n` on the development Mac (Apple Silicon) and captured with `screencapture -x -o -l<window id>`. It is the window at 1920 x 1144 points of the Retina capture, titled Murim24, on the title screen.
- icon_16x.png: `art/icon/icon.aseprite` at 16x, by `aseprite --batch art/icon/icon.aseprite --scale 16 --save-as icon_16x.png`.

The same app's `--verbose` log over 8 s had no line matching "error". With `Contents/Frameworks/libmurim.macos.template_release.universal.dylib` moved out of the bundle, it logged:

    ERROR: Can't open GDExtension dynamic library: 'res://murim.gdextension'.
    ERROR: Error loading extension: 'res://murim.gdextension'.
    SCRIPT ERROR: Parse Error: Could not find type "WorldAdapter" in the current scope.
