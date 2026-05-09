import type { ReactElement } from "react";

interface Props {
  logs: string[];
}

const MAX_LINES = 100;

export function ActivityLog({ logs }: Props): ReactElement {
  const tail = logs.slice(-MAX_LINES).reverse();

  return (
    <div>
      <p className="mb-2 text-[11px] uppercase tracking-wider text-[var(--muted)]">
        Activity ({MAX_LINES} most recent EV-style lines)
      </p>
      <div className="scrollbar-thin max-h-52 overflow-y-auto rounded-lg border border-white/[0.07] bg-[#090e14] px-2 py-2 font-mono text-[11px] leading-relaxed text-slate-400">
        {tail.map((line, i) => (
          <div key={`${i}-${line.slice(0, 20)}`} className="rounded px-1.5 py-0.5 hover:bg-white/[0.03]">
            {line.includes("overload") ? (
              <span className="text-amber-200/95">{line}</span>
            ) : line.includes("completed registration") ? (
              <span className="text-emerald-300/95">{line}</span>
            ) : line.includes("FAILED") ? (
              <span className="text-red-300/95">{line}</span>
            ) : (
              line
            )}
          </div>
        ))}
      </div>
    </div>
  );
}
