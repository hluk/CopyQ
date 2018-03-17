This directory contains source files for building CopyQ Flatpak package.

# Build

Install `flatpak-builder`.

Option `--user` can be omitted.

```bash
flatpak install --user flathub org.freedesktop.Platform//1.6 org.freedesktop.Sdk//1.6
flatpak remote-add --user --if-not-exists kdeapps --from https://distribute.kde.org/kdeapps.flatpakrepo
flatpak install --user flathub org.kde.Platform//5.9
flatpak install --user flathub org.kde.Sdk//5.9
```

Build the app.

```bash
mkdir copyq repo
flatpak-builder --ccache --force-clean --repo=repo --subject="Build of copyq" copyq com.github.hluk.copyq.json
```

Install the app.

```bash
flatpak remote-add --user copyq repo --no-gpg-verify
flatpak install --user --reinstall copyq com.github.hluk.copyq
flatpak run com.github.hluk.copyq
```
