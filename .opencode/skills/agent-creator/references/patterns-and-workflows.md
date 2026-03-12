# Agent Patterns and Workflows

Common patterns for designing effective agent systems and multi-agent workflows.

## Single Agent Patterns

### Specialist Pattern

One agent excels at one specific task.

**Use when:**
- Task is well-defined and focused
- Requires specific expertise
- Used repeatedly in same way

**Example:**
```yaml
description: Reviews API endpoints for REST compliance
mode: subagent
```

### Guardian Pattern

Agent acts as a gatekeeper or enforcer.

**Use when:**
- Enforcing standards
- Security reviews
- Compliance checks

**Example:**
```yaml
description: Enforces security policies on code changes
mode: subagent
tools:
  write: false
  edit: false
```

### Assistant Pattern

Agent helps with a specific aspect of work.

**Use when:**
- Augmenting developer capabilities
- Providing specific expertise
- Handling routine tasks

**Example:**
```yaml
description: Generates unit tests for functions
tools:
  write: true
```

## Multi-Agent Patterns

### Pipeline Pattern

Agents pass work sequentially from one to the next.

```
User Request
    ↓
@planner → Creates plan
    ↓
@implementer → Executes plan
    ↓
@reviewer → Reviews implementation
    ↓
@tester → Validates with tests
    ↓
Complete
```

**Configuration:**
```yaml
# planner.md
mode: subagent
tools:
  write: false
  edit: false

# implementer.md
mode: subagent
tools:
  write: true
  edit: true

# reviewer.md
mode: subagent
tools:
  write: false
  edit: false

# tester.md
mode: subagent
tools:
  write: true
  bash:
    "npm test": allow
```

**Use when:**
- Complex, multi-stage tasks
- Need for separation of concerns
- Quality gates required

### Hub-and-Spoke Pattern

Primary agent delegates to multiple specialists.

```
     @orchestrator (primary)
    /        |         \
   ↓         ↓          ↓
@security  @performance  @docs
   \         |         /
    ↓        ↓        ↓
      Coordinated Result
```

**Configuration:**
```yaml
# orchestrator.md
mode: primary
tools:
  write: true
  edit: true
permission:
  task:
    "*": allow    # Can call all subagents

# security.md
mode: subagent
tools:
  write: false   # Analysis only

# performance.md
mode: subagent
tools:
  write: false   # Analysis only

# docs.md
mode: subagent
tools:
  write: true    # Can write docs
```

**Use when:**
- Multiple aspects to address
- Need parallel analysis
- Complex coordination required

### Assembly Pattern

Multiple agents contribute parts to a whole.

```
User: "Build a feature"

@backend-dev → Implements API
@frontend-dev → Implements UI
@database-dev → Schema changes
       ↓
  Combined Result
```

**Configuration:**
```yaml
# backend-dev.md
description: Implements backend APIs

# frontend-dev.md
description: Implements frontend components

# database-dev.md
description: Designs and implements database changes
```

**Use when:**
- Clear component separation
- Different expertise needed
- Can work in parallel

### Consultation Pattern

Primary agent consults specialists for advice.

```
@build (primary)
    ↓
"I need to implement auth"
    ↓
@security-consultant → "Use JWT, avoid sessions"
    ↓
@build implements with advice
```

**Configuration:**
```yaml
# build.md - already exists

# security-consultant.md
mode: subagent
tools:
  write: false   # Advisory only
```

**Use when:**
- Need expert advice
- Decision making
- Best practices guidance

### Escalation Pattern

Agent escalates when encountering issues.

```
@junior-dev → Attempts fix
    ↓ (if stuck)
@senior-dev → Provides guidance
    ↓ (if complex)
@architect → Designs solution
```

**Configuration:**
```yaml
# junior-dev.md
mode: subagent
temperature: 0.4

# senior-dev.md
mode: subagent
temperature: 0.2
model: anthropic/claude-opus-4-20251022

# architect.md
mode: subagent
temperature: 0.1
model: anthropic/claude-opus-4-20251022
```

**Use when:**
- Tiered support
- Complex problem solving
- Learning/mentoring

## Workflow Patterns

### Review Gate Workflow

Every change must pass review before proceeding.

```
Code Change
    ↓
@implementer makes changes
    ↓
@reviewer must approve
    ↓ (if approved)
Merge/Deploy
    ↓ (if rejected)
Back to implementer
```

**Implementation:**
```yaml
# Use stop hook or tool.execute.before
# to enforce review requirement
```

### Canary Workflow

Gradual rollout with monitoring.

```
@implementer → Makes change
    ↓
@deployer → Deploys to canary
    ↓
@monitor → Watches metrics
    ↓ (if good)
Full rollout
    ↓ (if bad)
Rollback
```

### TDD Workflow

Test-driven development with agents.

```
Feature Request
    ↓
@tester → Writes failing tests
    ↓
@implementer → Makes tests pass
    ↓
@refactorer → Improves code
    ↓
@tester → Validates
```

### Documentation-First Workflow

Document before implementing.

```
Feature Request
    ↓
@architect → Designs solution
    ↓
@docs-writer → Documents API
    ↓
@implementer → Builds to spec
    ↓
@reviewer → Validates against docs
```

## Agent Delegation Strategies

### Automatic Delegation

Primary agent automatically uses Task tool.

**Prompt instruction:**
```markdown
When you need [specific task], delegate to @specialist.

Example:
"For security reviews, use @security-auditor"
```

### Manual Delegation

User explicitly mentions subagent.

**Usage:**
```
@security-auditor check this PR for vulnerabilities
```

### Conditional Delegation

Delegate based on content analysis.

**Prompt instruction:**
```markdown
Analyze the request:
- If it involves [condition A], use @agent-a
- If it involves [condition B], use @agent-b
- Otherwise handle yourself
```

### Parallel Delegation

Delegate to multiple agents simultaneously.

```
User: "Review this code"

Primary agent:
→ @security-auditor (security)
→ @performance-analyzer (performance)
→ @maintainability-reviewer (code quality)

Combine results
```

## Permission Patterns

### Read-Only Analysis

Agent can only read, never modify.

```yaml
tools:
  write: false
  edit: false
  bash: false
permission:
  edit: deny
  bash: deny
```

### Guarded Modification

Agent can modify with approval.

```yaml
tools:
  write: true
  edit: true
permission:
  edit: ask
  bash: ask
```

### Safe Commands Only

Agent can run specific safe commands.

```yaml
permission:
  bash:
    "git status": allow
    "git log*": allow
    "npm test": allow
    "*": deny
```

### Full Access

Agent has complete control (use sparingly).

```yaml
tools:
  write: true
  edit: true
  bash: true
permission:
  edit: allow
  bash: allow
```

## State Management Patterns

### Stateless Agents

Agents don't track state between invocations.

**Use when:**
- Each request is independent
- No context needed from previous runs
- Simple, focused tasks

### Session State

Agents track state within a session.

**Implementation:**
```typescript
// In plugin
const sessions = new Map<string, AgentState>()
```

**Use when:**
- Multi-step processes
- Context accumulation
- Workflow tracking

### Persistent State

Agents save state to disk.

**Implementation:**
```typescript
// Save to file
await writeFile(statePath, JSON.stringify(state))

// Load from file
const state = JSON.parse(await readFile(statePath))
```

**Use when:**
- Cross-session persistence
- Long-running workflows
- Audit trails

## Error Handling Patterns

### Retry Pattern

Agent retries failed operations.

```markdown
If an operation fails:
1. Analyze the error
2. Fix the issue
3. Retry up to 3 times
4. Escalate if still failing
```

### Fallback Pattern

Agent has fallback options.

```markdown
Primary approach: [Method A]
If that fails: [Method B]
If that fails: [Method C]
```

### Escalation Pattern

Agent escalates when stuck.

```markdown
If you encounter:
- Security issue → Escalate to @security
- Performance problem → Escalate to @performance
- Architecture concern → Escalate to @architect
```

## Communication Patterns

### Structured Output

Agent produces consistent, parseable output.

```markdown
Always format findings as:

## Summary
[One sentence summary]

## Details
- [Point 1]
- [Point 2]

## Action Items
1. [Action]
2. [Action]
```

### Conversation Style

Agent adapts communication style.

```markdown
Adjust your tone based on context:
- Technical details → Precise, detailed
- High-level overview → Concise, conceptual
- Error situations → Clear, actionable
```

### Context Preservation

Agent maintains conversation context.

```markdown
Remember:
- User preferences from earlier
- Decisions made
- Constraints discussed

Reference previous context when relevant.
```

## Anti-Patterns

❌ **Circular Delegation**
```
@a → delegates to @b
@b → delegates back to @a
```

❌ **Over-Delegation**
```
Primary agent delegates every small task
Result: Slow, fragmented workflow
```

❌ **Tool Mismatch**
```yaml
# Analysis agent with write access
description: Code reviewer
tools:
  write: true  # Wrong! Should be false
```

❌ **Vague Boundaries**
```markdown
# Unclear what this agent does
description: "Helps with code"
```

❌ **Permission Escalation**
```yaml
# Subagent has more permissions than primary
mode: subagent
tools:
  bash: allow   # Primary only has ask
```

## Best Practices Summary

✅ **Clear boundaries**: Each agent has specific focus  
✅ **Appropriate permissions**: Least privilege principle  
✅ **Complementary skills**: Agents fill different roles  
✅ **Clear handoffs**: Defined transition points  
✅ **Error handling**: Graceful failure and recovery  
✅ **Documentation**: Clear descriptions and guidelines  
✅ **Testing**: Validate agent behavior  
✅ **Monitoring**: Track agent usage and effectiveness  

## Workflow Design Checklist

When designing multi-agent workflows:

- [ ] Clear goal definition
- [ ] Agent role definitions
- [ ] Handoff points identified
- [ ] Tool permissions appropriate
- [ ] Error handling planned
- [ ] Fallback strategies defined
- [ ] User interaction model clear
- [ ] Performance considerations
- [ ] Debugging support
- [ ] Documentation complete
