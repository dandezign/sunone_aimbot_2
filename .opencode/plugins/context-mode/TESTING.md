# Context Mode Native Plugin - Testing Guide

## Quick Start Verification

### 1. Check Build Output ✅

```bash
cd I:/CppProjects/sunone_aimbot_2/.opencode/context-mode-main
ls build/native-tools/
```

**Expected:** You should see compiled JavaScript files:
```
ctx_doctor.js
ctx_stats.js
ctx_upgrade.js
index.js
shared.js
```

### 2. Verify Plugin Exports ✅

```bash
node -e "import('./build/opencode-plugin.js').then(m => console.log('Plugin exports:', Object.keys(m)))"
```

**Expected output:**
```
Plugin exports: [ 'ContextModePlugin' ]
```

### 3. Run Unit Tests ✅

```bash
npm run test -- tests/core/manager.test.ts
```

**Expected:** All 5 tests passing
```
✓ tests/core/manager.test.ts (5 tests) 25ms
 Test Files  1 passed (1)
      Tests  5 passed (5)
```

---

## Manual Testing in OpenCode

### Step 1: Configure OpenCode

**Add to your project's `opencode.json`:**

```json
{
  "$schema": "https://opencode.ai/config.json",
  "plugin": ["I:/CppProjects/sunone_aimbot_2/.opencode/context-mode-main"]
}
```

**Or for global installation:**

```bash
npm install -g I:/CppProjects/sunone_aimbot_2/.opencode/context-mode-main
```

Then in `opencode.json`:

```json
{
  "plugin": ["context-mode"]
}
```

### Step 2: Restart OpenCode

```bash
# Exit OpenCode completely
# Then restart
opencode
```

### Step 3: Test Native Tools

#### Test `ctx_stats`

**Ask OpenCode:**
```
ctx stats
```

**Expected response:**
```
## Context Mode Statistics

**Session:** abc12345...
**Events Captured:** 0
**Compactions:** 0

**Context Savings:**
- Raw: 97.7 KB
- Processed: 2.0 KB
- Saved: 98.0%
```

**Test JSON format:**
```
ctx stats --format json
```

**Expected:**
```json
{
  "session": {
    "id": "abc12345...",
    "directory": "I:/your/project",
    "eventCount": 0,
    "compactCount": 0
  },
  "savings": {
    "rawBytes": 100000,
    "processedBytes": 2000,
    "ratio": "98.0%"
  }
}
```

#### Test `ctx_doctor`

**Ask OpenCode:**
```
ctx doctor
```

**Expected response:**
```
## Context Mode Diagnostics

**Session:** abc12345...
**Project:** I:/your/project

### Checks

- [x] Session state
- [x] OpenCode server (vX.X.X)

### Recommendations
- Use ctx_stats to view context savings
- Use ctx_upgrade to update to latest version
```

#### Test `ctx_upgrade`

**Ask OpenCode:**
```
ctx upgrade
```

**Expected:** npm install command runs and returns upgrade status

---

## Integration Testing

### Test 1: Session Isolation

**Goal:** Verify each OpenCode session has isolated state.

**Steps:**

1. **Open Terminal 1:**
   ```bash
   opencode
   ```
   
2. **In Terminal 1, ask:**
   ```
   ctx stats
   ```
   Note the session ID (e.g., `abc12345...`)

3. **Open Terminal 2 (new session):**
   ```bash
   opencode
   ```

4. **In Terminal 2, ask:**
   ```
   ctx stats
   ```
   
5. **Verify:** Different session ID (e.g., `xyz67890...`)

6. **In Terminal 1:**
   ```
   Run 5 shell commands
   ```

7. **In Terminal 2:**
   ```
   ctx stats
   ```
   
8. **Verify:** Terminal 2 shows 0 events (isolated from Terminal 1)

### Test 2: Event Capture

**Goal:** Verify events are captured per-session.

**Steps:**

1. Start OpenCode session
2. Run some commands:
   ```
   Read src/main.cpp
   Edit src/main.cpp
   Bash git status
   ```
3. Ask:
   ```
   ctx stats
   ```
4. **Expected:** eventCount > 0

### Test 3: Context Injection at Compaction

**Goal:** Verify session state is injected during compaction.

**Steps:**

1. Work in OpenCode for a while (create 10+ events)
2. Trigger compaction (usually automatic when context gets large)
3. After compaction, ask about previous work
4. **Expected:** OpenCode remembers context from before compaction

**Verification command:**
```bash
# Check session database
ls "%USERPROFILE%\.config\opencode\context-mode\sessions\"
```

**Expected:** `.db` files with session data

### Test 4: Session Lifecycle Events

**Goal:** Verify session.created and session.deleted fire correctly.

**Steps:**

1. Start OpenCode
2. Check logs for "Session created" message
3. Work for a bit
4. Exit OpenCode
5. Check session is cleaned up:
   ```bash
   # Session should be removed from Map
   # (verify via ctx stats showing fresh session on restart)
   ```

---

## Debugging

### Enable Verbose Logging

Add to `opencode.json`:

```json
{
  "env": {
    "CONTEXT_MODE_DEBUG": "1"
  }
}
```

### Check OpenCode Logs

```bash
# Windows
ls "%APPDATA%\opencode\logs\"

# Or in OpenCode session
ctx doctor
```

### Verify Database Files

```bash
# Check session databases exist
dir "%USERPROFILE%\.config\opencode\context-mode\sessions\*.db"
```

**Expected:** One or more `.db` files

### Inspect Database Contents

```bash
# Using sqlite3
sqlite3 "%USERPROFILE%\.config\opencode\context-mode\sessions\YOUR-SESSION.db"

# In sqlite:
SELECT COUNT(*) FROM session_events;
SELECT * FROM session_events LIMIT 5;
```

---

## Troubleshooting

### Issue: Plugin not loading

**Symptoms:** `ctx stats` command not recognized

**Fix:**
1. Verify plugin path in opencode.json
2. Check build output exists: `ls build/opencode-plugin.js`
3. Restart OpenCode completely
4. Check OpenCode logs for plugin errors

### Issue: Session not isolated

**Symptoms:** Multiple terminals share same session ID

**Fix:**
1. Verify `session.created` event fires
2. Check SessionManager.getOrCreate() is called with unique sessionID
3. Add debug logging to see sessionID values

### Issue: Context not injected

**Symptoms:** OpenCode forgets context after compaction

**Fix:**
1. Verify `experimental.session.compacting` hook fires
2. Check `output.context.push()` is called
3. Verify snapshot is built (check logs)

### Issue: TypeScript build fails

**Symptoms:** Build errors about types

**Fix:**
```bash
cd I:/CppProjects/sunone_aimbot_2/.opencode/context-mode-main
npm install @opencode-ai/plugin@latest
npm run build
```

---

## Success Criteria

✅ **All of these should be true:**

- [ ] `npm run build` succeeds with no errors
- [ ] `npm run test -- tests/core/manager.test.ts` shows 5/5 tests passing
- [ ] `ctx stats` command works in OpenCode
- [ ] `ctx doctor` command works in OpenCode
- [ ] Multiple OpenCode sessions have different session IDs
- [ ] Events are captured per-session (not shared)
- [ ] Session databases created in `%USERPROFILE%\.config\opencode\context-mode\sessions\`
- [ ] Context injection works at compaction time

---

## Next Steps After Verification

1. **Performance Testing:**
   - Measure context savings %
   - Compare native vs MCP tool execution time
   - Test with 100+ events per session

2. **Edge Case Testing:**
   - Test with 10+ concurrent sessions
   - Test orphan cleanup (kill OpenCode, restart)
   - Test with large files (>1MB)

3. **Integration Testing:**
   - Test with real development workflow
   - Verify MCP tools (ctx_execute) still work
   - Test session restore after crash

---

*Testing guide for Context Mode v1.0.22 Native OpenCode Plugin*  
*Created: 2026-03-15*
