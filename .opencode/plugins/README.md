# OpenCode Plugins

Plugins for OpenCode.

## Context Mode

The native OpenCode plugin for Context Mode is located in `context-mode/`.

### Quick Start

```json
{
  "plugin": ["context-mode"]
}
```

### Testing

```bash
cd context-mode
npm run test
```

### Structure

- `context-mode/` - Complete Context Mode npm package
  - `build/opencode-plugin.js` - Native OpenCode plugin entry point
  - `src/native-tools/` - Native ctx_stats, ctx_doctor, ctx_upgrade
  - `src/session/manager.ts` - Per-session state management
  - `tests/` - Test suite
