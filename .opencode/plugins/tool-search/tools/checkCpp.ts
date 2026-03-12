import { tool } from "@opencode-ai/plugin"
import { TOOL_REGISTRY } from "../tool-registry"

export const checkCppTool = tool({
  description: TOOL_REGISTRY.checkCpp.description,
  args: {
    filePath: tool.schema.string().describe("Path to the C++ file to check")
  },
  async execute(args, context) {
    const { $ } = context
    const filePath = args.filePath
    
    try {
      const content = await $`cat ${filePath}`.text()
      const lines = content.split("\n")
      
      const issues = []
      let braceCount = 0
      let parenCount = 0
      let bracketCount = 0
      let inBlockComment = false
      
      for (let i = 0; i < lines.length; i++) {
        const line = lines[i]
        
        // Handle block comments
        if (line.includes("/*")) {
          if (!line.includes("*/")) {
            inBlockComment = true
          }
          continue
        }
        if (line.includes("*/")) {
          inBlockComment = false
          continue
        }
        if (inBlockComment) continue
        
        // Skip pure comment lines
        if (line.trim().startsWith("//")) continue
        
        // Count brackets
        for (let j = 0; j < line.length; j++) {
          const char = line[j]
          // Check for string literals to avoid counting brackets inside strings
          if (char === '"') {
            j++
            while (j < line.length && line[j] !== '"') {
              if (line[j] === '\\') j++
              j++
            }
            continue
          }
          if (char === "'") {
            j++
            while (j < line.length && line[j] !== "'") {
              if (line[j] === '\\') j++
              j++
            }
            continue
          }
          
          if (char === "{") braceCount++
          if (char === "}") braceCount--
          if (char === "(") parenCount++
          if (char === ")") parenCount--
          if (char === "[") bracketCount++
          if (char === "]") bracketCount--
        }
        
        // Check for TODO/FIXME
        if (line.includes("TODO") || line.includes("FIXME")) {
          issues.push({ line: i + 1, type: "note", message: "TODO/FIXME found", content: line.trim().slice(0, 80) })
        }
        
        // Check for hardcoded Windows paths
        if (/[A-Z]:\\|\\\\/g.test(line) && !line.includes("#include")) {
          issues.push({ line: i + 1, type: "warning", message: "Possible hardcoded Windows path", content: line.trim().slice(0, 80) })
        }
        
        // Check for common mistakes
        if (/\bsizeof\s*\(\s*\w+\s*\)\s*\/\s*\bsizeof\s*\(/g.test(line)) {
          issues.push({ line: i + 1, type: "info", message: "Consider using ARRAY_SIZE macro or std::size", content: line.trim().slice(0, 80) })
        }
        
        // Check for potential null pointer dereference
        if (/\*\w+\s*->/.test(line) && !line.includes("if ") && !line.includes("while ")) {
          issues.push({ line: i + 1, type: "warning", message: "Potential unchecked pointer dereference", content: line.trim().slice(0, 80) })
        }
        
        // Check for using namespace in header
        if (/using\s+namespace\s+/.test(line) && (filePath.endsWith(".h") || filePath.endsWith(".hpp"))) {
          issues.push({ line: i + 1, type: "warning", message: "using namespace in header file (consider removing)", content: line.trim().slice(0, 80) })
        }
      }
      
      // Check for unbalanced brackets
      if (braceCount !== 0) {
        issues.push({ line: lines.length, type: "error", message: `Unclosed braces: ${braceCount > 0 ? '+' : ''}${braceCount}` })
      }
      if (parenCount !== 0) {
        issues.push({ line: lines.length, type: "error", message: `Unclosed parentheses: ${parenCount > 0 ? '+' : ''}${parenCount}` })
      }
      if (bracketCount !== 0) {
        issues.push({ line: lines.length, type: "error", message: `Unclosed brackets: ${bracketCount > 0 ? '+' : ''}${bracketCount}` })
      }
      
      const errorCount = issues.filter(i => i.type === "error").length
      const warningCount = issues.filter(i => i.type === "warning").length
      const noteCount = issues.filter(i => i.type === "note").length
      
      return {
        filePath,
        totalLines: lines.length,
        issuesFound: issues.length,
        breakdown: { errors: errorCount, warnings: warningCount, notes: noteCount },
        issues: issues.slice(0, 20),
        status: errorCount > 0 ? "has_errors" : warningCount > 0 ? "has_warnings" : "clean",
        summary: errorCount > 0 
          ? `Found ${errorCount} errors, ${warningCount} warnings, ${noteCount} notes`
          : warningCount > 0
          ? `Found ${warningCount} warnings and ${noteCount} notes - no errors`
          : issues.length > 0
          ? `Found ${noteCount} notes - no errors or warnings`
          : "No issues detected"
      }
    } catch (error) {
      return {
        error: `Failed to check ${filePath}: ${error}`,
        filePath
      }
    }
  }
})

export default checkCppTool
