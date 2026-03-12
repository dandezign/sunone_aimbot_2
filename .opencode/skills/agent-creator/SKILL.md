---
name: agent-creator
description: Use when creating OpenCode agents - provides agent architecture, primary/subagent patterns, YAML frontmatter configuration, tool permissions, model selection, and prompt engineering for specialized AI assistants.
---

# OpenCode Agent Creator

Create specialized AI agents with custom prompts, tool permissions, and model configurations to extend OpenCode's capabilities.

## Philosophy: Specialized Intelligence

Agents in OpenCode are **specialized AI assistants** with distinct roles, permissions, and capabilities. Rather than one agent doing everything, you create focused agents that excel at specific tasks.

**Before creating an agent, ask:**
- What specific task or workflow should this agent specialize in?
- Should this be a primary agent (user-facing) or subagent (task-specific)?
- What tools does this agent need vs. what should be restricted?
- Which model is best suited for this agent's cognitive requirements?
- How will users invoke this agent (Tab switching, @ mention, or automatic delegation)?

**Core principles:**
1. **Specialization over generalization**: Agents should excel at one thing
2. **Least privilege**: Grant only necessary tools to minimize risk
3. **Clear identity**: Agent name and description should communicate purpose
4. **Appropriate model**: Match model capabilities to agent requirements
5. **Composable workflows**: Design agents to work together through delegation

## Agent Types

### Primary Agents
Main assistants users interact with directly. Switch between them using **Tab** key.

**Characteristics:**
- Full session control
- Visible in agent switcher
- Can invoke subagents via Task tool
- Typically have broader tool access

**Built-in primary agents:**
- **build**: Default agent with full tool access
- **plan**: Restricted agent for analysis (file edits ask permission)

### Subagents
Specialized assistants invoked via **@mention** or automatically by primary agents.

**Characteristics:**
- Task-specific focus
- Invoked with `@agent-name` syntax
- Can be hidden from autocomplete (internal use)
- Delegated by primary agents for specialized work

**Built-in subagents:**
- **general**: Multi-step research and execution
- **explore**: Fast read-only codebase exploration

### System Agents
Hidden agents that run automatically for system operations.

**Built-in system agents:**
- **compaction**: Compacts long context
- **title**: Generates session titles
- **summary**: Creates session summaries

## Agent Configuration

### Markdown Format (Recommended)

Create agents as markdown files with YAML frontmatter:

**Location:**
- Global: `~/.config/opencode/agents/`
- Project: `.opencode/agents/`

**Structure:**
```markdown
---
description: Brief description of what the agent does
mode: subagent
model: anthropic/claude-sonnet-4-20250514
temperature: 0.3
tools:
  write: false
  edit: false
  bash: false
permission:
  edit: deny
  bash:
    "*": ask
    "git status": allow
---

You are a specialized agent focused on [specific task].

Your responsibilities:
- Responsibility 1
- Responsibility 2

Guidelines:
1. Guideline one
2. Guideline two

When working:
- Always do X before Y
- Never do Z without checking W
```

**Filename becomes agent name**: `code-reviewer.md` → `@code-reviewer`

### JSON Format

Configure agents in `opencode.json`:

```json
{
  "$schema": "https://opencode.ai/config.json",
  "agent": {
    "code-reviewer": {
      "description": "Reviews code for best practices",
      "mode": "subagent",
      "model": "anthropic/claude-sonnet-4-20250514",
      "temperature": 0.1,
      "prompt": "You are a code reviewer...",
      "tools": {
        "write": false,
        "edit": false,
        "bash": false
      },
      "permission": {
        "edit": "deny",
        "bash": {
          "*": "ask",
          "git status": "allow"
        }
      }
    }
  }
}
```

## Configuration Options

### Required Fields

| Field | Description | Example |
|-------|-------------|---------|
| `description` | What the agent does | `"Reviews code for security issues"` |

### Mode

```yaml
mode: primary    # User-facing, switchable with Tab
mode: subagent   # Task-specific, invoked with @mention
mode: all        # Both primary and subagent
```

Default: `all`

### Model Selection

```yaml
model: anthropic/claude-sonnet-4-20250514
model: openai/gpt-5.2
model: google/gemini-3-pro-preview
```

Format: `provider/model-id`

**Model selection guidelines:**
- **Complex reasoning**: Claude Opus, GPT-5.2
- **Fast exploration**: Claude Haiku, Grok
- **UI/Creative work**: Gemini 3 Pro
- **Balanced**: Claude Sonnet

### Temperature

Control creativity vs. determinism:

```yaml
temperature: 0.1   # Very focused (analysis, review)
temperature: 0.3   # Balanced (general coding)
temperature: 0.7   # Creative (brainstorming, docs)
```

Range: 0.0 to 1.0

### Max Steps

Limit agentic iterations:

```yaml
steps: 10    # Force response after 10 tool calls
```

Useful for cost control or preventing runaway agents.

### Tools

Enable/disable specific tools:

```yaml
tools:
  read: true       # Allow file reads (default: true)
  write: true      # Allow file writes
  edit: true       # Allow file edits
  bash: true       # Allow shell commands
  glob: true       # Allow file search (default: true)
  grep: true       # Allow text search (default: true)
  list: true       # Allow directory listing (default: true)
  question: true   # Allow interactive questioning
  skill: true      # Allow loading skills
  webfetch: true   # Allow fetching web content
  todowrite: true  # Allow todo list management
  todoread: true   # Allow reading todo lists
```

Use wildcards for MCP tools:

```yaml
tools:
  "mcp_server_*": false    # Disable all MCP server tools
```

### Permissions

Fine-grained control over dangerous operations:

```yaml
permission:
  edit: ask          # ask | allow | deny
  bash:              # Glob patterns supported
    "*": ask         # Default: ask for all
    "git status": allow
    "git log*": allow
    "npm test": allow
  webfetch: deny     # Block web fetching
```

**Permission levels:**
- `ask`: Prompt for approval each time
- `allow`: Execute without approval
- `deny`: Block the operation entirely

### Task Permissions

Control which subagents can invoke other subagents:

```yaml
permission:
  task:
    "*": deny              # Block all by default
    "helper-*": allow      # Allow helpers
    "reviewer": ask        # Ask for reviewer
```

### Hidden

Hide from @ autocomplete (internal agents):

```yaml
hidden: true   # Only programmatic invocation
```

### Color

Customize appearance in UI:

```yaml
color: "#ff6b6b"        # Hex color
color: "accent"         # Theme color (primary, secondary, accent, success, warning, error, info)
```

### Top P

Alternative to temperature for diversity control:

```yaml
top_p: 0.9    # Nucleus sampling
```

### Additional Options

Provider-specific parameters passed through:

```yaml
reasoningEffort: high      # OpenAI reasoning models
textVerbosity: low         # Output verbosity
```

## Agent Categories (oh-my-opencode)

When using oh-my-opencode, agents can reference categories for model inheritance:

```yaml
category: ultrabrain         # Complex reasoning
category: visual-engineering # UI/UX work
category: deep              # Strategic planning
category: quick             # Fast exploration
category: writing           # Documentation
```

**Category inheritance:**
- Category provides default model, temperature, etc.
- Agent-specific settings override category defaults

## Best Practices

### Naming Conventions

**Good names:**
- `security-auditor`
- `performance-analyzer`
- `docs-writer`
- `api-tester`

**Bad names:**
- `agent1`
- `helper`
- `my-agent`

### Tool Permissions

**Least privilege principle:**

```yaml
# Review agent - should not modify code
tools:
  write: false
  edit: false
  bash: false

# Debug agent - needs to run tests
tools:
  write: false
  edit: false
  bash:
    "npm test": allow
    "cargo test": allow

# Implementation agent - full access
tools:
  write: true
  edit: true
  bash: true
```

### Interactive Questioning

The `question` tool enables agents to present users with multiple choice options, reducing cognitive load and ensuring clear responses.

**Configuration:**
```yaml
tools:
  question: true   # Enable interactive questioning
```

**When to use multiple choice questions:**
- Clarifying vague user input
- Presenting configuration options
- Gathering preferences (tech stack, architecture, etc.)
- Confirming decisions before proceeding

**Question format:**
```
question([
  {
    header: "Short Label",
    question: "What would you like to do?",
    multiSelect: false,  # true allows multiple selections
    options: [
      { label: "Option 1", description: "What this option does" },
      { label: "Option 2", description: "Another choice" },
      { label: "Option 3 (Recommended)", description: "Best default" }
    ]
  }
])
```

**Key features:**
- **Auto "Other" option**: When `multiSelect: false`, users get a "Type your own answer" option automatically
- **Multiple questions**: Pass an array to ask multiple questions in one interaction
- **Descriptions**: Each option should have a clear description explaining the choice
- **Recommended options**: Mark the best default with "(Recommended)" in the label

**Example — clarifying "fast":**
```
User says: "It should be fast"

question: "Fast how?"
options: [
  { label: "Sub-second response", description: "Prioritize low latency" },
  { label: "Handles large datasets", description: "Optimize for throughput" },
  { label: "Quick to build", description: "Fastest implementation" }
]
```

**Example — gathering requirements:**
```
question([
  {
    header: "Priority",
    question: "What's most important for this feature?",
    options: [
      { label: "Speed (Recommended)", description: "Optimize for performance" },
      { label: "Security", description: "Prioritize security measures" },
      { label: "Simplicity", description: "Keep it minimal" }
    ]
  },
  {
    header: "Scope",
    question: "What should be included?",
    multiSelect: true,
    options: [
      { label: "Core functionality", description: "Essential features only" },
      { label: "Error handling", description: "Comprehensive error management" },
      { label: "Documentation", description: "Inline docs and README" }
    ]
  }
])
```

**Anti-patterns to avoid:**
- Generic categories ("Technical", "Business", "Other")
- Too many options (2-4 is ideal)
- Leading questions that presume an answer
- Forcing users to type when they could select

### Prompt Engineering

**Structure agent prompts with:**

1. **Identity**: Who the agent is
2. **Purpose**: What they do
3. **Guidelines**: How they work
4. **Constraints**: What they should not do
5. **Examples**: Sample interactions (if helpful)

**Example structure:**
```markdown
You are a [specialized role] focused on [specific domain].

Your purpose is to:
- Task 1
- Task 2

Guidelines:
1. Always do X
2. Check Y before Z
3. Prioritize A over B

Constraints:
- Never do X without permission
- Always validate Y
- Document Z decisions
```

### Agent Workflows

Design agents to work together:

1. **Planner** (@planner): Creates implementation plan
2. **Implementer** (@implement): Executes the plan
3. **Reviewer** (@reviewer): Reviews the code
4. **Tester** (@tester): Validates with tests

**Example workflow:**
```
User: "Add user authentication"

1. @planner creates auth implementation plan
2. @implement builds the auth system
3. @reviewer checks for security issues
4. @tester validates the implementation
```

## Validation (Required)

When you create/update agents, validate BEFORE telling the user it is ready.

Validation checklist:
- Frontmatter parses and includes `description` and `mode`.
- Agent file lives in `.opencode/agents/` (NOT `.opencode/agent/`).
- If the agent depends on tools, confirm those tools exist:
  - Built-ins: `read`, `edit`, `bash`, `question`, `task`, `skill`, etc.
  - Plugin tools: ensure a plugin provides them (eg `.opencode/plugins/*.ts`).
- If the agent should be internal-only, set `hidden: true` and ensure the parent
  agent only invokes it via `task`.
- If the agent requires interactive selection, use the `question` tool (not
  A/B/C plain text).

Repo script validation (preferred):
- Run: `python .opencode/skills/agent-creator/scripts/quick_validate_agents.py`

## Anti-Patterns to Avoid

❌ **Over-privileged agents**: Giving all tools when not needed
```yaml
# WRONG - review agent doesn't need write
tools:
  write: true
  edit: true
```

❌ **Vague descriptions**: Unclear agent purpose
```yaml
# WRONG
---
description: "Helps with stuff"
```

❌ **Kitchen-sink agents**: Doing too many things
```yaml
# WRONG - agent that reviews, implements, tests, deploys
```

❌ **Missing constraints**: No guidance on what NOT to do
```markdown
# WRONG - only positive guidance
You are a helpful assistant.
```

❌ **Inappropriate models**: Using heavy models for simple tasks
```yaml
# WRONG - using Opus for file exploration
model: anthropic/claude-opus-4-20251022
```

## Variation Guidance

Agent designs should vary based on purpose:

- **Analysis agents** (review, audit): Low temperature, read-only tools, detailed prompts
- **Creative agents** (docs, UI): Higher temperature, more permissive
- **Implementation agents** (build, fix): Full tool access, focused prompts
- **Exploration agents** (search, research): Fast models, read-only, broad prompts
- **Guardian agents** (security, compliance): Strict permissions, verification focus

Avoid creating agents that duplicate built-in functionality or add unnecessary complexity.

## Remember

OpenCode agents enable powerful specialization through focused intelligence. Create agents that excel at specific tasks, grant appropriate permissions, and design them to work together in composable workflows.

**Start simple**: Create one focused agent. Then expand your agent ecosystem as needs grow.

See `references/` for detailed configuration options, example agents, and reusable templates.
