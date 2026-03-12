import { tool } from "@opencode-ai/plugin"
import { TOOL_REGISTRY } from "../tool-registry"

export const findHeadersTool = tool({
  description: TOOL_REGISTRY.findHeaders.description,
  args: {
    filePath: tool.schema.string().describe("Path to the C++ source file")
  },
  async execute(args, context) {
    const { $ } = context
    const filePath = args.filePath
    
    try {
      // Use regex to find #include statements
      const content = await $`cat ${filePath}`.text()
      const includes = content.match(/#include\s+[<"]([^>"]+)[>"]/g) || []
      const headers = includes.map(inc => 
        inc.replace(/#include\s+[<"]/, "").replace(/[>"]$/, "")
      )
      
      // Also extract system vs local headers
      const systemHeaders = headers.filter(h => !h.includes("/") && !h.includes("\\"))
      const localHeaders = headers.filter(h => h.includes("/") || h.includes("\\"))
      
      // Analyze header types
      const standardHeaders = systemHeaders.filter(h => 
        /^(iostream|vector|string|map|set|unordered_map|unordered_set|memory|algorithm|functional|chrono|thread|mutex|future|filesystem|optional|variant|any)$/.test(h)
      )
      const customHeaders = systemHeaders.filter(h => !standardHeaders.includes(h))
      
      return {
        filePath,
        headersFound: headers.length,
        headers: headers.slice(0, 50),
        breakdown: {
          standardLibrary: standardHeaders.length,
          thirdParty: customHeaders.length,
          local: localHeaders.length
        },
        systemHeaders: systemHeaders.slice(0, 30),
        localHeaders: localHeaders.slice(0, 20),
        summary: `Found ${headers.length} header includes in ${filePath} (${standardHeaders.length} standard, ${customHeaders.length} third-party, ${localHeaders.length} local)`
      }
    } catch (error) {
      return {
        error: `Failed to analyze ${filePath}: ${error}`,
        filePath
      }
    }
  }
})

export default findHeadersTool
