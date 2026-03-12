import { tool } from "@opencode-ai/plugin"
import { TOOL_REGISTRY } from "../tool-registry"

type SearchResult = {
  file: string
  line: number
  type: "class_definition" | "function_definition" | "implementation" | "declaration" | "function_call" | "usage"
  preview: string
}

export const searchSymbolsTool = tool({
  description: TOOL_REGISTRY.searchSymbols.description,
  args: {
    symbol: tool.schema.string().describe("The symbol name to search for"),
    type: tool.schema.string().optional().describe("Filter: 'class', 'function', 'variable', or 'all'")
  },
  async execute(args, context) {
    const { $ } = context
    const { symbol, type = "all" } = args
    const results: SearchResult[] = []

    try {
      let grepPattern = `\\b(class|struct)\\s+${symbol}\\b|\\b${symbol}\\s*\\(`

      if (type === "class") {
        grepPattern = `\\bclass\\s+${symbol}\\b|\\bstruct\\s+${symbol}\\b`
      } else if (type === "function") {
        grepPattern = `\\b${symbol}\\s*\\(`
      } else if (type === "variable") {
        grepPattern = `\\b${symbol}\\b`
      }

      const output = await $`grep -rn -E "${grepPattern}" --include="*.cpp" --include="*.h" --include="*.hpp" --include="*.c" . 2>/dev/null || true`.text()

      if (output) {
        const lines = output.split("\n").filter(line => line.trim())
        const seen = new Set<string>()

        for (const line of lines) {
          const match = line.match(/^(.+):(\d+):(.*)$/)
          if (!match) {
            continue
          }

          const [, file, lineNum, content] = match
          const trimmedContent = content.trim()
          const key = `${file}:${lineNum}`

          if (seen.has(key)) {
            continue
          }
          seen.add(key)

          let resultType: SearchResult["type"] = "usage"
          if (new RegExp(`\\b(class|struct)\\s+${symbol}\\b`).test(trimmedContent)) {
            resultType = "class_definition"
          } else if (new RegExp(`\\b${symbol}\\s*\\([^)]*\\)\\s*(const)?\\s*\\{`).test(trimmedContent)) {
            resultType = "function_definition"
          } else if (new RegExp(`\\b${symbol}\\s*\\(`).test(trimmedContent) && !trimmedContent.includes("=")) {
            resultType = "function_call"
          } else if (trimmedContent.includes("::") && trimmedContent.includes(symbol)) {
            resultType = "implementation"
          }

          results.push({
            file: file.replace(/^\.\/?/, "").replace(/^\//, ""),
            line: Number.parseInt(lineNum, 10),
            type: resultType,
            preview: trimmedContent.slice(0, 120)
          })
        }
      }

      if (type === "all" || type === "function" || type === "class") {
        const declOutput = await $`grep -rn "${symbol}" --include="*.h" --include="*.hpp" . 2>/dev/null | grep -E "(virtual|static|void|int|bool|class|struct|enum|template)" | head -20 || true`.text()
        if (declOutput) {
          const declLines = declOutput.split("\n").filter(line => line.trim())
          for (const line of declLines) {
            const match = line.match(/^(.+):(\d+):(.*)$/)
            if (!match) {
              continue
            }

            const [, file, lineNum, content] = match
            const normalizedFile = file.replace(/^\.\/?/, "")
            const parsedLine = Number.parseInt(lineNum, 10)
            const alreadyFound = results.some(result => result.file === normalizedFile && result.line === parsedLine)
            if (alreadyFound) {
              continue
            }

            results.push({
              file: normalizedFile,
              line: parsedLine,
              type: "declaration",
              preview: content.trim().slice(0, 120)
            })
          }
        }
      }

      const priority: Record<SearchResult["type"], number> = {
        class_definition: 0,
        function_definition: 1,
        implementation: 2,
        declaration: 3,
        function_call: 4,
        usage: 5
      }

      results.sort((a, b) => priority[a.type] - priority[b.type])

      const byFile: Record<string, SearchResult[]> = {}
      for (const result of results) {
        if (!byFile[result.file]) {
          byFile[result.file] = []
        }
        byFile[result.file].push(result)
      }

      const summarySuffix = results.find(result => result.type === "class_definition")
        ? " (class definition found)"
        : results.find(result => result.type === "function_definition")
          ? " (function definition found)"
          : results.find(result => result.type === "declaration")
            ? " (declaration found)"
            : ""

      return {
        symbol,
        searchType: type,
        totalFound: results.length,
        byFile: Object.entries(byFile).slice(0, 10).map(([file, items]) => ({
          file,
          occurrences: items.length,
          items: items.slice(0, 5)
        })),
        results: results.slice(0, 20),
        summary: results.length > 0
          ? `Found ${results.length} occurrences of "${symbol}"${summarySuffix}`
          : `No occurrences of "${symbol}" found`
      }
    } catch (error) {
      return {
        error: `Failed to search for ${symbol}: ${error}`,
        symbol
      }
    }
  }
})

export default searchSymbolsTool
