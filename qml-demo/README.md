# QML Demo

This folder contains a standalone Qt Quick/QML desktop demo. It is separated from the main Qt Widgets application and can be built independently.

## Build

```bash
cmake -S qml-demo -B build-qml-demo
cmake --build build-qml-demo
```

The demo uses Qt Quick Controls and shows a compact dashboard layout with metric cards, a status panel, and a placeholder chart area.
