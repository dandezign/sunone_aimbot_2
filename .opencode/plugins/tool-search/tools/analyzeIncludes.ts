import { tool } from "@opencode-ai/plugin"
import { TOOL_REGISTRY } from "../tool-registry"

export const analyzeIncludesTool = tool({
  description: TOOL_REGISTRY.analyzeIncludes.description,
  args: {
    entryPoint: tool.schema.string().optional().describe("Starting file or directory")
  },
  async execute(args, context) {
    const { $ } = context
    const { entryPoint = "." } = args
    
    const includeGraph = new Map<string, Set<string>>()
    const circularDeps: string[][] = []
    const headerStats = {
      systemHeaders: new Set<string>(),
      localHeaders: new Set<string>(),
      totalIncludes: 0
    }
    
    try {
      // Find all C++ files
      const files = await $`find ${entryPoint} -type f \\( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \\) 2>/dev/null | head -100`.text()
      const fileList = files.split("\n").filter(f => f.trim())
      
      // Build include graph
      for (const file of fileList) {
        try {
          const content = await $`cat "${file}" 2>/dev/null || echo ""`.text()
          const includes = content.match(/#include\s+[<"]([^>"]+)[>"]/g) || []
          const headers = includes.map(inc => 
            inc.replace(/#include\s+[<"]/, "").replace(/[>"]$/, "")
          )
          
          includeGraph.set(file.trim(), new Set(headers))
          
          // Categorize headers
          for (const h of headers) {
            headerStats.totalIncludes++
            if (h.includes("/") || h.includes("\\")) {
              headerStats.localHeaders.add(h)
            } else {
              headerStats.systemHeaders.add(h)
            }
          }
        } catch {
          // Skip files that can't be read
        }
      }
      
      // Detect circular dependencies (simplified BFS)
      function findCycles() {
        const visited = new Set<string>()
        const recursionStack = new Set<string>()
        const cycles: string[][] = []
        
        function dfs(node: string, path: string[] = []) {
          if (recursionStack.has(node)) {
            // Found cycle
            const cycleStart = path.indexOf(node)
            if (cycleStart !== -1) {
              const cycle = path.slice(cycleStart).concat([node])
              cycles.push(cycle)
            }
            return
          }
          
          if (visited.has(node)) return
          
          visited.add(node)
          recursionStack.add(node)
          
          const deps = includeGraph.get(node) || new Set()
          for (const dep of deps) {
            // Try to resolve the header to a file
            const depFile = [...includeGraph.keys()].find(k => 
              k.endsWith(dep) || k.endsWith("/" + dep) || k.endsWith("\\" + dep)
            )
            if (depFile) {
              dfs(depFile, [...path, node])
            }
          }
          
          recursionStack.delete(node)
        }
        
        for (const file of includeGraph.keys()) {
          if (!visited.has(file)) {
            dfs(file)
          }
        }
        
        return cycles
      }
      
      const cycles = findCycles()
      
      // Find potentially unused includes (files that include but don't use)
      const unusedIncludes: Array<{ file: string, include: string }> = []
      for (const [file, includes] of includeGraph) {
        try {
          const content = await $`cat "${file}"`.text()
          for (const inc of includes) {
            const baseName = inc.split("/").pop()?.split("\\").pop()?.replace(/\.[^.]+$/, "")
            if (baseName && !content.includes(baseName)) {
              // Simple heuristic - if header name doesn't appear in file content
              unusedIncludes.push({ file, include: inc })
            }
          }
        } catch {}
      }
      
      // Calculate depth statistics
      const depthMap = new Map<string, number>()
      function getDepth(file: string, visited = new Set<string>()): number {
        if (visited.has(file)) return 0
        if (depthMap.has(file)) return depthMap.get(file)!
        
        visited.add(file)
        const includes = includeGraph.get(file) || new Set()
        let maxDepth = 0
        
        for (const inc of includes) {
          const incFile = [...includeGraph.keys()].find(k => k.endsWith(inc) || k.endsWith("/" + inc))
          if (incFile) {
            maxDepth = Math.max(maxDepth, getDepth(incFile, new Set(visited)) + 1)
          }
        }
        
        depthMap.set(file, maxDepth)
        return maxDepth
      }
      
      for (const file of includeGraph.keys()) {
        getDepth(file)
      }
      
      const maxDepth = Math.max(...depthMap.values(), 0)
      const avgDepth = depthMap.size > 0 
        ? [...depthMap.values()].reduce((a, b) => a + b, 0) / depthMap.size 
        : 0
      
      // Most included headers
      const includeCount = new Map<string, number>()
      for (const includes of includeGraph.values()) {
        for (const inc of includes) {
          includeCount.set(inc, (includeCount.get(inc) || 0) + 1)
        }
      }
      const mostIncluded = [...includeCount.entries()]
        .sort((a, b) => b[1] - a[1])
        .slice(0, 10)
      
      return {
        totalFiles: includeGraph.size,
        totalIncludes: headerStats.totalIncludes,
        headerBreakdown: {
          system: headerStats.systemHeaders.size,
          local: headerStats.localHeaders.size
        },
        circularDependencies: cycles.slice(0, 5).map(cycle => cycle.map(f => f.split("/").pop() || f)),
        hasCircularDeps: cycles.length > 0,
        circularDepCount: cycles.length,
        includeDepth: {
          max: maxDepth,
          average: Math.round(avgDepth * 10) / 10
        },
        mostIncludedHeaders: mostIncluded.slice(0, 5),
        unusedIncludeSuggestions: unusedIncludes.slice(0, 5),
        summary: cycles.length > 0
          ? `Found ${cycles.length} circular dependencies and analyzed ${includeGraph.size} files`
          : `No circular dependencies detected in ${includeGraph.size} files`
      }
    } catch (error) {
      return {
        error: `Failed to analyze includes: ${error}`,
        entryPoint
      }
    }
  }
})

export default analyzeIncludesTool
