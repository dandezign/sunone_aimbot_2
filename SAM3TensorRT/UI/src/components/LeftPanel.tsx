import { useState } from 'react';

type SectionKey = 'Capture' | 'Labels' | 'Settings';

interface LeftPanelProps {
  activeSection: SectionKey;
  onSectionChange: (section: SectionKey) => void;
  onLog: (level: 'INFO' | 'SUCCESS' | 'WARNING', message: string) => void;
}

export const LeftPanel = ({ activeSection, onSectionChange, onLog }: LeftPanelProps): JSX.Element => {
  const [expandedSection, setExpandedSection] = useState<SectionKey | null>('Capture');

  // Capture section state
  const [captureMode, setCaptureMode] = useState('Window');
  const [captureInterval, setCaptureInterval] = useState(150);
  const [autoCapture, setAutoCapture] = useState(true);

  // Labels section state
  const [selectedLabel, setSelectedLabel] = useState('Person');
  const [labelPreset, setLabelPreset] = useState('Default');

  // Settings section state
  const [confidence, setConfidence] = useState(0.45);
  const [iouThreshold, setIouThreshold] = useState(0.35);
  const [livePreview, setLivePreview] = useState(true);
  const [snapToGrid, setSnapToGrid] = useState(false);

  const handleSectionClick = (section: SectionKey) => {
    if (expandedSection === section) {
      setExpandedSection(null);
    } else {
      setExpandedSection(section);
      onSectionChange(section);
      onLog('INFO', `Opened ${section} panel`);
    }
  };

  const menuItems = [
    {
      id: "capture",
      label: "Capture",
      sectionKey: "Capture" as SectionKey,
      icon: (
        <svg
          className="absolute w-full h-full top-0 left-0"
          viewBox="0 0 20 20"
          fill="none"
          xmlns="http://www.w3.org/2000/svg"
          aria-hidden="true"
        >
          <rect
            x="1.5"
            y="4.5"
            width="12"
            height="11"
            rx="1.5"
            stroke="#9f9fa9"
            strokeWidth="1.2"
          />
          <path
            d="M13.5 8.5L18.5 6V14L13.5 11.5V8.5Z"
            stroke="#9f9fa9"
            strokeWidth="1.2"
            strokeLinejoin="round"
          />
        </svg>
      ),
    },
    {
      id: "labels",
      label: "Labels",
      sectionKey: "Labels" as SectionKey,
      icon: (
        <svg
          className="absolute w-full h-full top-0 left-0"
          viewBox="0 0 20 20"
          fill="none"
          xmlns="http://www.w3.org/2000/svg"
          aria-hidden="true"
        >
          <path
            d="M2.5 2.5H10.086a1 1 0 0 1 .707.293l6.414 6.414a1 1 0 0 1 0 1.414l-5.586 5.586a1 1 0 0 1-1.414 0L3.793 9.793A1 1 0 0 1 3.5 9.086V3.5a1 1 0 0 1 1-1Z"
            stroke="#9f9fa9"
            strokeWidth="1.2"
            strokeLinejoin="round"
          />
          <circle cx="7" cy="7" r="1.2" fill="#9f9fa9" />
        </svg>
      ),
    },
    {
      id: "settings",
      label: "Settings",
      sectionKey: "Settings" as SectionKey,
      icon: (
        <svg
          className="absolute w-full h-full top-0 left-0"
          viewBox="0 0 20 20"
          fill="none"
          xmlns="http://www.w3.org/2000/svg"
          aria-hidden="true"
        >
          <circle cx="10" cy="10" r="2.5" stroke="#9f9fa9" strokeWidth="1.2" />
          <path
            d="M10 2v1.5M10 16.5V18M2 10h1.5M16.5 10H18M4.222 4.222l1.06 1.06M14.718 14.718l1.06 1.06M4.222 15.778l1.06-1.06M14.718 5.282l1.06-1.06"
            stroke="#9f9fa9"
            strokeWidth="1.2"
            strokeLinecap="round"
          />
        </svg>
      ),
    },
  ];

  const ChevronRight = ({ rotated = false }: { rotated?: boolean }) => (
    <svg
      className={`absolute w-[66.67%] h-[79.17%] top-[20.83%] left-[33.33%] transition-transform duration-200 ${rotated ? 'rotate-90' : ''}`}
      viewBox="0 0 10 13"
      fill="none"
      xmlns="http://www.w3.org/2000/svg"
      aria-hidden="true"
    >
      <path
        d="M2 1.5L7.5 6.5L2 11.5"
        stroke="#9f9fa9"
        strokeWidth="1.4"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    </svg>
  );

  return (
    <div className="flex flex-col w-64 h-[537.6px] items-start pb-[1.91e-05px] pt-0 px-0 relative bg-zinc-900 border-r-[0.8px] [border-right-style:solid] border-zinc-800">
      <div className="h-[52.8px] pt-4 pb-[0.8px] px-4 relative border-b-[0.8px] [border-bottom-style:solid] border-zinc-800 flex flex-col w-[255.2px] items-start">
        <div className="relative self-stretch w-full h-5">
          <h2 className="absolute top-0 left-0 [font-family:'Inter-SemiBold',Helvetica] font-semibold text-zinc-100 text-sm tracking-[0] leading-5 whitespace-nowrap">
            Editor Options
          </h2>
        </div>
      </div>

      <nav className="gap-1 pt-3 pb-0 px-3 flex flex-col w-[255.2px] items-start">
        {menuItems.map((item) => (
          <div key={item.id} className="w-full">
            <button
              className={`all-[unset] box-border flex items-center gap-3 px-3 py-0 rounded-lg relative self-stretch w-full h-10 cursor-pointer transition-colors ${
                expandedSection === item.sectionKey ? 'bg-zinc-800' : 'hover:bg-zinc-800'
              }`}
              aria-label={item.label}
              onClick={() => handleSectionClick(item.sectionKey)}
            >
              <div className="relative w-5 h-5 flex-shrink-0">{item.icon}</div>

              <div className="absolute top-2.5 left-11 w-[147px] h-5 flex">
                <span className="mt-[-0.2px] h-5 [font-family:'Inter-Medium',Helvetica] font-medium text-[#9f9fa9] text-sm tracking-[0] leading-5 whitespace-nowrap">
                  {item.label}
                </span>
              </div>

              <div className="relative w-4 h-4 ml-auto flex-shrink-0">
                <ChevronRight rotated={expandedSection === item.sectionKey} />
              </div>
            </button>

            {/* Expanded Section Content */}
            {expandedSection === item.sectionKey && (
              <div className="mt-2 bg-zinc-800 rounded-lg p-3">
                {item.sectionKey === 'Capture' && (
                  <>
                    <div className="mb-3">
                      <label className="block text-[12px] text-[#9f9fa9] mb-1.5">Capture Mode</label>
                      <select
                        className="w-full bg-zinc-900 text-[#d4d4d8] text-[12px] px-2 py-1.5 rounded border border-zinc-700 focus:border-emerald-500"
                        value={captureMode}
                        onChange={(e) => {
                          setCaptureMode(e.target.value);
                          onLog('INFO', `Capture mode changed to ${e.target.value}`);
                        }}
                      >
                        <option value="Window">Window</option>
                        <option value="Fullscreen">Fullscreen</option>
                        <option value="Region">Region</option>
                      </select>
                    </div>
                    <div className="mb-3">
                      <label className="block text-[12px] text-[#9f9fa9] mb-1.5">Interval: {captureInterval}ms</label>
                      <input
                        type="range"
                        min={50}
                        max={500}
                        step={10}
                        value={captureInterval}
                        onChange={(e) => {
                          const val = Number(e.target.value);
                          setCaptureInterval(val);
                          onLog('INFO', `Capture interval set to ${val}ms`);
                        }}
                        className="w-full accent-[#00bc7d] h-1 bg-zinc-900 rounded-full"
                      />
                    </div>
                    <button
                      className={`w-full text-[12px] px-2 py-1.5 rounded transition-colors font-medium ${
                        autoCapture ? 'bg-[#00bc7d] text-[#09090b]' : 'bg-zinc-900 text-[#9f9fa9] border border-zinc-700'
                      }`}
                      onClick={() => {
                        setAutoCapture(!autoCapture);
                        onLog('INFO', `Auto Capture ${!autoCapture ? 'enabled' : 'disabled'}`);
                      }}
                    >
                      Auto Capture: {autoCapture ? 'On' : 'Off'}
                    </button>
                  </>
                )}

                {item.sectionKey === 'Labels' && (
                  <>
                    <div className="mb-3">
                      <label className="block text-[12px] text-[#9f9fa9] mb-1.5">Label Class</label>
                      <select
                        className="w-full bg-zinc-900 text-[#d4d4d8] text-[12px] px-2 py-1.5 rounded border border-zinc-700 focus:border-emerald-500"
                        value={selectedLabel}
                        onChange={(e) => {
                          setSelectedLabel(e.target.value);
                          onLog('INFO', `Label class changed to ${e.target.value}`);
                        }}
                      >
                        <option value="Person">Person</option>
                        <option value="Vehicle">Vehicle</option>
                        <option value="Object">Object</option>
                      </select>
                    </div>
                    <div>
                      <label className="block text-[12px] text-[#9f9fa9] mb-1.5">Preset</label>
                      <select
                        className="w-full bg-zinc-900 text-[#d4d4d8] text-[12px] px-2 py-1.5 rounded border border-zinc-700 focus:border-emerald-500"
                        value={labelPreset}
                        onChange={(e) => {
                          setLabelPreset(e.target.value);
                          onLog('INFO', `Label preset changed to ${e.target.value}`);
                        }}
                      >
                        <option value="Default">Default</option>
                        <option value="Training">Training</option>
                        <option value="Debug">Debug</option>
                      </select>
                    </div>
                  </>
                )}

                {item.sectionKey === 'Settings' && (
                  <>
                    <div className="mb-3">
                      <label className="block text-[12px] text-[#9f9fa9] mb-1.5">Confidence: {confidence.toFixed(2)}</label>
                      <input
                        type="range"
                        min={0.1}
                        max={0.99}
                        step={0.01}
                        value={confidence}
                        onChange={(e) => {
                          const val = Number(e.target.value);
                          setConfidence(val);
                          onLog('INFO', `Confidence threshold set to ${val.toFixed(2)}`);
                        }}
                        className="w-full accent-[#00bc7d] h-1 bg-zinc-900 rounded-full"
                      />
                    </div>
                    <div className="mb-3">
                      <label className="block text-[12px] text-[#9f9fa9] mb-1.5">IoU Threshold: {iouThreshold.toFixed(2)}</label>
                      <input
                        type="range"
                        min={0.1}
                        max={0.95}
                        step={0.01}
                        value={iouThreshold}
                        onChange={(e) => {
                          const val = Number(e.target.value);
                          setIouThreshold(val);
                          onLog('INFO', `IoU threshold set to ${val.toFixed(2)}`);
                        }}
                        className="w-full accent-[#00bc7d] h-1 bg-zinc-900 rounded-full"
                      />
                    </div>
                    <button
                      className={`w-full text-[12px] px-2 py-1.5 rounded transition-colors font-medium mb-2 ${
                        livePreview ? 'bg-[#00bc7d] text-[#09090b]' : 'bg-zinc-900 text-[#9f9fa9] border border-zinc-700'
                      }`}
                      onClick={() => {
                        setLivePreview(!livePreview);
                        onLog('INFO', `Live Preview ${!livePreview ? 'enabled' : 'disabled'}`);
                      }}
                    >
                      Live Preview: {livePreview ? 'On' : 'Off'}
                    </button>
                    <button
                      className={`w-full text-[12px] px-2 py-1.5 rounded transition-colors font-medium ${
                        snapToGrid ? 'bg-[#00bc7d] text-[#09090b]' : 'bg-zinc-900 text-[#9f9fa9] border border-zinc-700'
                      }`}
                      onClick={() => {
                        setSnapToGrid(!snapToGrid);
                        onLog('INFO', `Snap To Grid ${!snapToGrid ? 'enabled' : 'disabled'}`);
                      }}
                    >
                      Snap To Grid: {snapToGrid ? 'On' : 'Off'}
                    </button>
                  </>
                )}
              </div>
            )}
          </div>
        ))}
      </nav>
    </div>
  );
};

export default LeftPanel;
