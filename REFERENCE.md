# Reference implementation notes

## Behavioral reference

Microsoft PowerToys Awake is used only as a high-level behavioral reference. This
repository does not include, load, redistribute, patch, or reverse-engineer any
PowerToys binaries or source files. The plugin independently implements the
same core user-facing behavior using documented Windows power-request APIs.

## Implemented behavior

- Off
- Keep awake indefinitely
- Keep awake for a duration
- Keep awake until a selected local date/time
- Optional keep-display-on state
- Quick presets: 30 min / 1 h / 2 h / 4 h
- Persist active mode and expiration across TrafficMonitor restarts
- TrafficMonitor display item, mouse actions, tooltip, settings window, and plugin commands

## Deliberate implementation difference

PowerToys Awake currently uses `SetThreadExecutionState`. This plugin instead uses a `PowerCreateRequest` handle with `PowerSetRequest` / `PowerClearRequest`.

This is deliberate: a TrafficMonitor plugin lives inside a host process, and a handle-owned power request gives the plugin explicit lifetime/cleanup control without depending on which TrafficMonitor thread happened to call the plugin.
