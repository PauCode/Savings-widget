# Savings widget
This widget is created as a way to make it easy for people to save money, also it is for people that spend too much time on their computer and can't be bothered to use a phone.

## Project board

[Trello board](https://trello.com/b/EXYHdcAu/ga)

## Themes

Add a `.css` file to [Theme/](Theme/README.md). The app discovers themes at startup and lets you select one from the theme menu.

## Build and fallback

Run the default VS Code build task. The first build restores the pinned WebView2 SDK into an ignored local package cache. The WebView2 Runtime must be installed to use the web UI; if WebView2 initialization, page loading, or its browser process fails, the existing native UI starts instead.

## Development log

See [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md) for implementation notes and verification history.
