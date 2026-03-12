# Agent Configuration Reference

Complete reference for all agent configuration options in OpenCode.

## Configuration File Locations

### Markdown Agents
- **Global**: `~/.config/opencode/agents/<agent-name>.md`
- **Project**: `.opencode/agents/<agent-name>.md`

### JSON Configuration
- **Global**: `~/.config/opencode/opencode.json`
- **Project**: `opencode.json` or `.opencode/opencode.json`

## YAML Frontmatter Reference

### Complete Example

```markdown
---
description: Performs security audits on code changes
mode: subagent
model: anthropic/claude-sonnet-4-20250514
temperature: 0.1
top_p: 0.95
steps: 15
tools:
  read: true
  write: false
  edit: false
  bash:
    "git log*": allow
    "git diff*": allow
    "*": deny
permission:
  edit: deny
  bash: ask
  webfetch: allow
task:
  "*": deny
  "security-*": allow
hidden: false
color: "#ff6b6b"
disable: false
---

You are a security auditor...
```

## Field Reference

### description
**Required** - Brief description of agent purpose

```yaml
description: Reviews code for security vulnerabilities
```

- Appears in tool documentation
- Used by LLM to decide when to invoke
- Keep under 100 characters
- Be specific about what the agent does

### mode
Agent availability mode

```yaml
mode: primary     # Switchable with Tab
mode: subagent    # Invoked with @mention
mode: all         # Both (default)
```

**Use cases:**
- `primary`: Main development agents (build, plan alternatives)
- `subagent`: Specialized task agents
- `all`: Universal utility agents

### model
Model selection

```yaml
model: provider/model-id
```

**Format:** `provider/model-id`

**Common providers:**
- `anthropic/` - Claude models
- `openai/` - GPT models
- `google/` - Gemini models
- `opencode/` - OpenCode models

**Examples:**
```yaml
model: anthropic/claude-opus-4-20251022      # Complex reasoning
model: anthropic/claude-sonnet-4-20250514    # Balanced
model: anthropic/claude-haiku-4-20250514     # Fast
model: openai/gpt-5.2                        # Strong reasoning
model: openai/gpt-5.1-codex                  # Coding
model: google/gemini-3-pro-preview           # Creative/UI
```

### temperature
Response creativity (0.0 - 1.0)

```yaml
temperature: 0.1   # Focused, deterministic
temperature: 0.3   # Balanced
temperature: 0.7   # Creative
```

**Recommendations:**
- **0.0-0.2**: Analysis, review, security auditing
- **0.3-0.5**: General coding, implementation
- **0.6-1.0**: Brainstorming, documentation, creative tasks

### top_p
Nucleus sampling alternative to temperature

```yaml
top_p: 0.9
```

Range: 0.0 - 1.0
Lower = more focused, Higher = more diverse

### steps
Maximum agentic iterations

```yaml
steps: 10
```

Agent forced to summarize after this many tool calls. Use for cost control.

### tools

Tool availability

```yaml
tools:
  read: true        # File reading (default: true)
  write: true       # File writing
  edit: true        # File editing
  bash: true        # Shell commands
  glob: true        # File search (default: true)
  grep: true        # Text search (default: true)
  question: true    # Interactive questioning with multiple choice
```

**MCP tools:**
```yaml
tools:
  "mcp_servername_toolname": true
  "mcp_*": false     # Disable all MCP
```

**Wildcard patterns:**
```yaml
tools:
  "mcp_context7_*": true    # Enable all context7 tools
  "*_search": true          # Enable all search tools
```

**MCP tools:**
```yaml
tools:
  "mcp_servername_toolname": true
  "mcp_*": false     # Disable all MCP
```

**Wildcard patterns:**
```yaml
tools:
  "mcp_context7_*": true    # Enable all context7 tools
  "*_search": true          # Enable all search tools
```

### permission
Fine-grained permissions

```yaml
permission:
  edit: ask        # ask | allow | deny
  bash:            # Command-specific
    "*": ask
    "git status": allow
    "git log*": allow
    "npm test": allow
    "rm *": deny
  webfetch: deny   # ask | allow | deny
  question: allow  # ask | allow | deny - Controls question tool usage
```

**Permission values:**
- `ask`: Prompt for approval
- `allow`: Execute without approval
- `deny`: Block operation

**Pattern matching:**
- `*`: Match any sequence
- `?`: Match single character
- Exact matches take precedence over wildcards

### task
Subagent invocation permissions

```yaml
permission:
  task:
    "*": deny              # Block all
    "helper-*": allow      # Allow helpers
    "reviewer": ask        # Ask for reviewer
    "explore": allow
```

Controls which subagents this agent can invoke via Task tool.

### hidden
Hide from @ autocomplete

```yaml
hidden: true    # Not shown in UI
```

Use for internal agents only invoked programmatically.

### color
UI appearance

```yaml
color: "#FF5733"        # Hex color
color: "primary"        # Theme colors
color: "secondary"
color: "accent"
color: "success"
color: "warning"
color: "error"
color: "info"
```

### disable
Disable the agent

```yaml
disable: true
```

### category (oh-my-opencode)
Model category reference

```yaml
category: ultrabrain         # Complex reasoning
category: visual-engineering # UI/UX
category: deep              # Planning
category: artistry          # Creative
category: quick             # Fast exploration
category: writing           # Documentation
```

Inherits model, temperature from category definition.

## JSON Configuration Format

### Complete Example

```json
{
  "$schema": "https://opencode.ai/config.json",
  "agent": {
    "security-auditor": {
      "description": "Performs security audits",
      "mode": "subagent",
      "model": "anthropic/claude-sonnet-4-20250514",
      "temperature": 0.1,
      "steps": 15,
      "tools": {
        "read": true,
        "write": false,
        "edit": false,
        "bash": {
          "git log*": "allow",
          "*": "deny"
        }
      },
      "permission": {
        "edit": "deny",
        "bash": "ask",
        "webfetch": "allow"
      },
      "hidden": false,
      "color": "#ff6b6b"
    }
  }
}
```

### Schema Structure

```json
{
  "agent": {
    "<agent-name>": {
      "description": string,
      "mode": "primary" | "subagent" | "all",
      "model": string,
      "temperature": number,
      "top_p": number,
      "steps": number,
      "tools": {
        "read": boolean,
        "write": boolean,
        "edit": boolean,
        "bash": boolean | object,
        "glob": boolean,
        "grep": boolean,
        "[pattern]": boolean
      },
      "permission": {
        "edit": "ask" | "allow" | "deny",
        "bash": "ask" | "allow" | "deny" | object,
        "webfetch": "ask" | "allow" | "deny",
        "task": object
      },
      "hidden": boolean,
      "color": string,
      "disable": boolean,
      "prompt": string
    }
  }
}
```

## Configuration Precedence

### Loading Order
1. Global config (`~/.config/opencode/opencode.json`)
2. Project config (`opencode.json`)
3. Global agents (`~/.config/opencode/agents/`)
4. Project agents (`.opencode/agents/`)

Later configs override earlier ones.

### Override Rules
- Markdown agents override JSON config for same name
- Project overrides global
- Agent-specific tools override global tool config
- Permission settings are merged

## Built-in Agent Overrides

Override built-in agents by creating agents with matching names:

```markdown
---
# ~/.config/opencode/agents/build.md
# This replaces the default build agent
---

Your custom build agent instructions...
```

**Overridable built-ins:**
- `build`
- `plan`
- `general`
- `explore`

## Validation

OpenCode validates agent configuration at startup:

**Invalid configs are silently skipped:**
- Missing `description`
- Invalid `mode` value
- Malformed YAML frontmatter
- Invalid tool names
- Invalid permission values

**Check for errors:**
- Review OpenCode startup logs
- Validate YAML syntax
- Verify model ID format

## Environment Variables

Reference environment variables in JSON config:

```json
{
  "agent": {
    "custom": {
      "model": "${CUSTOM_MODEL}"
    }
  }
}
```

Not supported in markdown frontmatter.
