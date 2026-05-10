import type { MonitorSample } from "../engine/types";
import type { ReactElement } from "react";

interface Props {
  history: MonitorSample[];
  timeLimit: number;
  sampleIntervalSec: number;
  showBuildingLines: boolean;
}

function buildPath(
  history: MonitorSample[],
  selector: (m: MonitorSample) => number,
  timeLimit: number,
  pad: number,
  cw: number,
  ch: number,
  ymin: number,
  ymax: number,
): string {
  const span = Math.max(ymax - ymin, 1);
  return history
    .map((m, i) => {
      const x = pad + (m.t / Math.max(timeLimit, 1)) * cw;
      const vy = (selector(m) - ymin) / span;
      const y = pad + ch - vy * ch;
      return `${i === 0 ? "M" : "L"} ${x.toFixed(2)} ${y.toFixed(2)}`;
    })
    .join(" ");
}

export function LineChart({ history, timeLimit, sampleIntervalSec, showBuildingLines }: Props): ReactElement {
  const w = 640;
  const h = 200;
  const pad = 32;
  const cw = w - pad * 2;
  const ch = h - pad * 2;

  const selectors: Array<(m: MonitorSample) => number> = [(m) => m.networkHealth];
  if (showBuildingLines) {
    selectors.push((m) => m.adminHealth, (m) => m.csHealth, (m) => m.libHealth);
  }

  const allVals =
    history.length >= 1 ? history.flatMap((m) => selectors.map((fn) => fn(m))) : [95];
  let ymin = Math.min(0, ...allVals, 50);
  let ymax = Math.max(105, ...allVals, 50);
  const spanFloor = ymax - ymin;
  if (spanFloor < 25) ymax = ymin + 25;

  ymin = Math.min(ymin, 0);
  ymax = Math.max(ymax, 105);

  const yAt = (v: number) => {
    const span = Math.max(ymax - ymin, 1);
    return pad + ch - ((v - ymin) / span) * ch;
  };

  const y50 = yAt(50);

  return (
    <div className="w-full rounded-xl border border-white/10 bg-[var(--surface-1)] px-4 py-4 sm:px-5">
      <div className="mb-2">
        <h3 className="text-sm font-semibold text-white">Network health</h3>
        <p className="mt-0.5 max-w-none text-[11px] leading-relaxed text-[var(--muted)] sm:max-w-3xl">
          OmNeT <code className="text-white/70">BottleneckMonitor</code> — {sampleIntervalSec}s samples · debounced &lt;50% → bottleneck events (
          <code className="text-white/70">Infrastructure.cc</code>).
        </p>
      </div>
      <svg
        viewBox={`0 0 ${w} ${h}`}
        className="aspect-[16/5] w-full min-h-[10rem] max-h-[min(40vh,18rem)]"
        preserveAspectRatio="xMidYMid meet"
        role="img"
        aria-label="Network health curve"
      >
        <rect x={0} y={0} width={w} height={h} fill="#0a1018" rx={10} />

        <text x={pad - 8} y={yAt(ymax) + 4} textAnchor="end" fill="#5c6e82" fontSize="10">
          {`${Math.round(ymax)}`}
        </text>
        <text x={pad - 8} y={yAt(ymin) + 4} textAnchor="end" fill="#5c6e82" fontSize="10">
          {`${Math.round(ymin)}`}
        </text>

        <line x1={pad} x2={w - pad} y1={y50} y2={y50} stroke="rgba(248,113,113,0.22)" strokeDasharray="6 8" />
        <text x={w - pad} y={y50 - 6} textAnchor="end" fill="rgba(248,113,113,0.45)" fontSize="9">
          50% debounce baseline
        </text>

        {history.length >= 2 ? (
          <>
            <path d={buildPath(history, (m) => m.networkHealth, timeLimit, pad, cw, ch, ymin, ymax)} fill="none" stroke="#2dd4bf" strokeWidth={2.2} strokeLinejoin="round" strokeLinecap="round" />
            {showBuildingLines ? (
              <>
                <path d={buildPath(history, (m) => m.adminHealth, timeLimit, pad, cw, ch, ymin, ymax)} fill="none" stroke="rgba(148,163,184,0.6)" strokeWidth={1.4} opacity={0.9} strokeLinecap="round" />
                <path d={buildPath(history, (m) => m.csHealth, timeLimit, pad, cw, ch, ymin, ymax)} fill="none" stroke="rgba(232,121,249,0.68)" strokeWidth={1.4} opacity={0.9} strokeLinecap="round" />
                <path d={buildPath(history, (m) => m.libHealth, timeLimit, pad, cw, ch, ymin, ymax)} fill="none" stroke="rgba(252,211,77,0.58)" strokeWidth={1.4} opacity={0.9} strokeLinecap="round" />
              </>
            ) : null}
          </>
        ) : (
          <text x={w / 2} y={h / 2} textAnchor="middle" fill="#5c6e82" fontSize="13">
            Run · first OmNeT-style sample arrives at {sampleIntervalSec}s
          </text>
        )}

        <text x={pad} y={h - 10} fill="#5c6e82" fontSize="11">
          0s
        </text>
        <text x={w - pad} y={h - 10} textAnchor="end" fill="#5c6e82" fontSize="11">
          {Math.round(timeLimit)}s
        </text>
      </svg>
      <div className="mt-2 flex flex-wrap gap-3 text-[10px] text-[var(--muted)]">
        <span className="flex items-center gap-1">
          <span className="inline-block h-2 w-3 rounded-[2px] bg-teal-400" /> Weighted network
        </span>
        {showBuildingLines ? (
          <>
            <span className="flex items-center gap-1">
              <span className="inline-block h-2 w-3 rounded-[2px] bg-slate-400/70" /> Admin
            </span>
            <span className="flex items-center gap-1">
              <span className="inline-block h-2 w-3 rounded-[2px] bg-fuchsia-400/70" /> CS
            </span>
            <span className="flex items-center gap-1">
              <span className="inline-block h-2 w-3 rounded-[2px] bg-amber-300/70" /> Library
            </span>
          </>
        ) : (
          <span className="text-white/35">Optional per-building overlays off</span>
        )}
      </div>
    </div>
  );
}
