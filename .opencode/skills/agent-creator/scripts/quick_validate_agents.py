import os
from pathlib import Path


ALLOWED_MODES = {"primary", "subagent", "all"}


def parse_frontmatter(text: str):
    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return None

    # Find closing '---'
    end = None
    for i in range(1, min(len(lines), 400)):
        if lines[i].strip() == "---":
            end = i
            break
    if end is None:
        return None

    front = lines[1:end]
    data = {}

    # Minimal parser: capture top-level 'key: value' pairs; ignore nested YAML.
    for raw in front:
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        if raw.startswith(" ") or raw.startswith("\t"):
            continue
        if ":" not in raw:
            continue
        key, value = raw.split(":", 1)
        key = key.strip()
        value = value.strip()
        if key:
            data[key] = value

    return data


def main() -> int:
    project_root = Path.cwd().resolve()
    agents_dir = project_root / ".opencode" / "agents"

    if not agents_dir.exists():
        print(f"ERROR: missing directory: {agents_dir}")
        return 2

    agent_files = sorted([p for p in agents_dir.glob("*.md") if p.is_file()])
    if not agent_files:
        print(f"WARN: no agents found in {agents_dir}")
        return 0

    errors = 0
    warnings = 0

    for p in agent_files:
        try:
            text = p.read_text(encoding="utf-8")
        except Exception as e:
            print(f"ERROR: {p}: failed to read: {e}")
            errors += 1
            continue

        fm = parse_frontmatter(text)
        if fm is None:
            print(f"ERROR: {p}: missing/invalid YAML frontmatter")
            errors += 1
            continue

        desc = fm.get("description")
        mode = fm.get("mode")

        if not desc:
            print(f"ERROR: {p}: missing 'description' in frontmatter")
            errors += 1
        if not mode:
            print(f"ERROR: {p}: missing 'mode' in frontmatter")
            errors += 1
        elif mode not in ALLOWED_MODES:
            print(
                f"ERROR: {p}: invalid mode '{mode}' (allowed: {sorted(ALLOWED_MODES)})"
            )
            errors += 1

        # Heuristic warnings
        if fm.get("hidden") and mode != "subagent":
            print(f"WARN:  {p}: hidden is typically used with mode: subagent")
            warnings += 1

    if errors:
        print(f"\nFAILED: {errors} error(s), {warnings} warning(s)")
        return 1

    print(f"OK: {len(agent_files)} agent(s) validated ({warnings} warning(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
