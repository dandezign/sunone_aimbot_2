import { useState } from 'react';
import LeftPanel from './LeftPanel';

type LogLevel = 'INFO' | 'SUCCESS' | 'WARNING';

interface LogEntry {
  time: string;
  level: LogLevel;
  message: string;
}

export function EditorUI() {
  const [zoom, setZoom] = useState(25);
  const [showLog, setShowLog] = useState(true);
  const [logs, setLogs] = useState<LogEntry[]>([
    { time: '[14:32:01]', level: 'INFO', message: 'Editor initialized successfully' },
    { time: '[14:32:03]', level: 'SUCCESS', message: 'Canvas loaded (1920x1080)' },
    { time: '[14:32:05]', level: 'INFO', message: 'Loading assets...' },
    { time: '[14:32:07]', level: 'SUCCESS', message: '12 layers imported' },
    { time: '[14:32:09]', level: 'WARNING', message: 'Some fonts are missing, using fallback' },
  ]);

  const addLog = (level: LogLevel, message: string) => {
    const time = new Date().toLocaleTimeString('en-GB', { hour12: false });
    setLogs(prev => {
      const next = [...prev, { time: `[${time}]`, level, message }];
      return next.slice(-8);
    });
  };

  const handleZoomIn = () => {
    setZoom(v => Math.min(200, v + 5));
    addLog('INFO', `Zoom increased to ${Math.min(200, zoom + 5)}%`);
  };

  const handleZoomOut = () => {
    setZoom(v => Math.max(10, v - 5));
    addLog('INFO', `Zoom decreased to ${Math.max(10, zoom - 5)}%`);
  };

  const handleZoomFit = () => {
    setZoom(25);
    addLog('INFO', 'Zoom reset to fit');
  };

  return (
    <div className="bg-white content-stretch flex flex-col items-start relative size-full" style={{ width: '1014px', height: '730px' }}>
      <div className="bg-[#09090b] content-stretch flex flex-col h-[729.6px] items-start relative shrink-0 w-full">
        {/* Main Container */}
        <div className="flex-[537.6_0_0] min-h-px min-w-px relative w-[1014.4px]">
          <div className="flex-[1_0_0] flex items-start overflow-clip relative h-full w-full">
            
            {/* Left Panel Component */}
            <LeftPanel
              activeSection="Capture"
              onSectionChange={() => {}}
              onLog={addLog}
            />

            {/* Main Panel - Canvas Area */}
            <div className="bg-[#09090b] flex-[1_0_0] h-[537.6px] min-h-px min-w-px relative">
              <div className="bg-clip-padding border-0 border-[transparent] border-solid relative size-full">
                
                {/* Toolbar */}
                <div className="absolute bg-[#18181b] border-[#27272a] border-b-[0.8px] border-solid content-stretch flex h-[48px] items-center justify-between left-0 pb-[0.8px] px-[16px] top-0 w-[758.4px]">
                  <div className="flex items-center gap-[8px]">
                    <div className="h-[20px] relative shrink-0 w-[44.39px]">
                      <div className="bg-clip-padding border-0 border-[transparent] border-solid relative size-full">
                        <p className="absolute font-medium leading-[20px] left-0 not-italic text-[#f4f4f5] text-[14px] top-[-0.2px] whitespace-nowrap">
                          Canvas
                        </p>
                      </div>
                    </div>
                    <div className="h-[15.99px] relative shrink-0 w-[66.54px]">
                      <div className="bg-clip-padding border-0 border-[transparent] border-solid relative size-full">
                        <p className="absolute font-normal leading-[16px] left-0 not-italic text-[#71717b] text-[12px] top-[-0.2px] whitespace-nowrap">
                          1920 × 1080
                        </p>
                      </div>
                    </div>
                  </div>
                  
                  <div className="h-[32px] relative shrink-0 w-[176px]">
                    <div className="bg-clip-padding border-0 border-[transparent] border-solid relative size-full">
                      
                      {/* Zoom Out Button */}
                      <button 
                        className="absolute content-stretch flex flex-col items-center justify-center left-0 pt-[8px] px-[8px] rounded-[4px] size-[32px] top-0 hover:bg-[#27272a] transition-colors cursor-pointer"
                        onClick={handleZoomOut}
                        title="Zoom Out"
                      >
                        <div className="h-[16px] overflow-clip relative shrink-0 w-full">
                          <img alt="" className="block max-w-none size-full" src="/assets/icon-zoom-out.svg" />
                        </div>
                      </button>

                      {/* Zoom Percentage Text */}
                      <div className="absolute h-[20px] left-[36px] top-[6px] w-[64px]">
                        <div className="bg-clip-padding border-0 border-[transparent] border-solid relative size-full">
                          <p className="absolute font-normal leading-[20px] left-[32.21px] not-italic text-[#9f9fa9] text-[14px] text-center top-[-0.2px] whitespace-nowrap translate-x-[-50%]">
                            {zoom}%
                          </p>
                        </div>
                      </div>

                      {/* Zoom In Button */}
                      <button 
                        className="absolute content-stretch flex flex-col items-center justify-center left-[104px] pt-[8px] px-[8px] rounded-[4px] size-[32px] top-0 hover:bg-[#27272a] transition-colors cursor-pointer"
                        onClick={handleZoomIn}
                        title="Zoom In"
                      >
                        <div className="h-[16px] overflow-clip relative shrink-0 w-full">
                          <img alt="" className="block max-w-none size-full" src="/assets/icon-zoom-in.svg" />
                        </div>
                      </button>

                      {/* Fit Button */}
                      <button 
                        className="absolute content-stretch flex flex-col items-center justify-center left-[144px] pt-[8px] px-[8px] rounded-[4px] size-[32px] top-0 hover:bg-[#27272a] transition-colors cursor-pointer"
                        onClick={handleZoomFit}
                        title="Fit to Screen"
                      >
                        <div className="h-[16px] overflow-clip relative shrink-0 w-full">
                          <img alt="" className="block max-w-none size-full" src="/assets/icon-fit.svg" />
                        </div>
                      </button>

                    </div>
                  </div>
                </div>

                {/* Canvas Area - This will be the main canvas for image/video editing */}
                <div className="absolute bg-[#09090b] content-stretch flex flex-col h-[489.6px] items-start left-0 overflow-clip pr-[15.2px] pt-[48px] top-0 w-[758.4px]">
                  <div className="h-[704px] relative shrink-0 w-full">
                    
                    {/* 
                      CANVAS PLACEHOLDER
                      This area will contain the main canvas element for:
                      - Image/video display
                      - Annotation overlays
                      - Detection boxes
                      - Segmentation masks
                      - User interactions (drawing, selection, etc.)
                      
                      TODO: Integrate with backend canvas rendering system
                    */}
                    
                    {/* Empty canvas container - ready for integration */}
                    <div 
                      className="absolute bg-[#09090b] left-[51.6px] shadow-[0px_20px_25px_0px_rgba(0,0,0,0.1),0px_8px_10px_0px_rgba(0,0,0,0.1)] size-[640px] top-[32px]"
                      style={{ transform: `scale(${zoom / 100})`, transformOrigin: 'top left' }}
                    >
                      {/* 
                        CANVAS CONTENT AREA
                        This is where the actual canvas/image will be rendered
                        
                        Future implementation:
                        - <canvas> element for real-time rendering
                        - Image/video element for media display
                        - Overlay layers for annotations
                      */}
                    </div>

                  </div>
                </div>

              </div>
            </div>

          </div>
        </div>

        {/* Log Panel */}
        <div className={`bg-[#09090b] border-[#27272a] border-solid border-t-[0.8px] relative shrink-0 w-[1014.4px] transition-all duration-200 ${showLog ? 'h-[192px]' : 'h-[40px]'}`}>
          <div className="bg-clip-padding border-0 border-[transparent] border-solid content-stretch flex flex-col items-start pt-[0.8px] relative size-full">
            
            {/* Log Header */}
            <div className="bg-[#18181b] border-[#27272a] border-b-[0.8px] border-solid h-[40px] relative shrink-0 w-[1014.4px]">
              <div className="bg-clip-padding border-0 border-[transparent] border-solid content-stretch flex items-center justify-between pb-[0.8px] px-[16px] relative size-full">
                
                {/* Log Title */}
                <div className="h-[20px] relative shrink-0 w-[162.8px]">
                  <div className="bg-clip-padding border-0 border-[transparent] border-solid content-stretch flex gap-[8px] items-center relative size-full">
                    <div className="relative shrink-0 size-[16px]">
                      <img alt="" className="absolute block max-w-none size-full" src="/assets/icon-log.svg" />
                    </div>
                    <div className="h-[20px] relative shrink-0 w-[84px]">
                      <div className="bg-clip-padding border-0 border-[transparent] border-solid relative size-full">
                        <p className="absolute font-medium leading-[20px] left-0 not-italic text-[#f4f4f5] text-[14px] top-[-0.2px] whitespace-nowrap">
                          Log Console
                        </p>
                      </div>
                    </div>
                    <div className="h-[15.99px] relative shrink-0 w-[52.59px]">
                      <div className="bg-clip-padding border-0 border-[transparent] border-solid relative size-full">
                        <p className="absolute font-normal leading-[16px] left-0 not-italic text-[#71717b] text-[12px] top-[-0.2px] whitespace-nowrap">
                          ({logs.length} entries)
                        </p>
                      </div>
                    </div>
                  </div>
                </div>

                {/* Close Button */}
                <button 
                  className="relative rounded-[4px] shrink-0 size-[24px] hover:bg-[#27272a] transition-colors cursor-pointer"
                  onClick={() => setShowLog(!showLog)}
                  title={showLog ? 'Collapse Log' : 'Expand Log'}
                >
                  <img alt="" className="absolute inset-1/4 block max-w-none size-full" src="/assets/icon-close.svg" />
                </button>

              </div>
            </div>

            {/* Log Entries */}
            {showLog && (
              <div className="flex-[151.2_0_0] min-h-px min-w-px relative w-[1014.4px]">
                <div className="bg-clip-padding border-0 border-[transparent] border-solid overflow-clip relative rounded-[inherit] size-full">
                  
                  {logs.map((entry, index) => (
                    <div 
                      key={`${entry.time}-${entry.level}-${index}`}
                      className="flex gap-[12px] items-center h-[32px] px-[12px] border-b border-[#27272a] last:border-0"
                      style={{ minHeight: '32px' }}
                    >
                      <div className="shrink-0 w-[75px]">
                        <p className="font-mono text-[12px] text-[#52525c] whitespace-nowrap">
                          {entry.time}
                        </p>
                      </div>
                      <div className="shrink-0 w-[70px]">
                        <p className={`font-mono font-bold text-[12px] uppercase ${
                          entry.level === 'SUCCESS' ? 'text-[#05df72]' :
                          entry.level === 'WARNING' ? 'text-[#fdc700]' :
                          'text-[#51a2ff]'
                        }`}>
                          {entry.level}
                        </p>
                      </div>
                      <div className="flex-1 min-w-0">
                        <p className="font-mono text-[12px] text-[#d4d4d8] truncate">
                          {entry.message}
                        </p>
                      </div>
                    </div>
                  ))}

                </div>
              </div>
            )}

          </div>
        </div>

      </div>
    </div>
  );
}

export default EditorUI;
