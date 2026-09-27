# Custom Themes

Add a folder to this directory containing a `.css` file with the same name as the folder (for example `Theme/ocean/ocean.css`). The app discovers valid theme folders when it starts and lists them in the theme menu.

Themes can override the variables in `default/default.css` and any UI selectors. Keep the stylesheet self-contained; local files and remote imports are not loaded by the app.