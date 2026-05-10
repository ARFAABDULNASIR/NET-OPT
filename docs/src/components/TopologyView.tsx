import type { SimSnapshot } from "../engine/types";
import type { ReactElement } from "react";

interface Props {
  snapshot: SimSnapshot;
}

/** Compact campus summary — NED-aligned AP counts via runtime list. */
export function TopologyView({ snapshot }: Props): ReactElement {
  const { aps, scenario } = snapshot;

  const summary = (
    bid: string,
    label: string,
    sub: string,
  ): { label: string; sub: string; clients: number; cap: number; hot: boolean; bn: boolean } => {
    const subset = aps.filter((a) => a.building === bid);
    const clients = subset.reduce((s, ap) => s + ap.connectedClients, 0);
    const cap = subset.reduce((s, ap) => s + ap.maxClients, 0);
    const hot = subset.some((ap) => ap.isBottleneck || ap.connectedClients >= ap.maxClients * 0.85);
    const bn = subset.some((ap) => ap.isBottleneck);
    return { label, sub, clients, cap, hot, bn };
  };

  const rows = [
    summary("Admin", "Admin block", "Ch 1 · 4 APs"),
    summary("CS", "CS block", "Ch 6 · 5 APs"),
    summary("Library", "Library", "Ch 11 · 3 APs"),
  ];

  return (
    <div className="w-full rounded-xl border border-white/10 bg-[var(--surface-1)] px-4 py-4 sm:px-5">
      <h3 className="text-sm font-semibold text-white">Campus load</h3>
      <p className="mt-0.5 text-[11px] text-[var(--muted)]">
        {scenario.registering ? "Aggregate Wi‑Fi clients vs AP capacity · matches `NetOpt.ned` fan-out" : "Background traffic · `NormalDay` omnetpp.ini"}
      </p>

      <div className="mt-4 space-y-2">
        {rows.map((r) => (
          <div key={r.label} className="flex items-center gap-3 rounded-lg border border-white/5 bg-black/20 px-3 py-2.5">
            <div className="min-w-0 flex-1">
              <p className="truncate text-[13px] font-medium text-white">{r.label}</p>
              <p className="text-[10px] text-[var(--muted)]">{r.sub}</p>
            </div>
            <div className="shrink-0 text-right font-mono text-[12px] text-white/85">
              {r.clients}/{r.cap}
              <span className="text-[var(--muted)]"> clients </span>
            </div>
            <div className={`h-9 w-[3px] shrink-0 rounded-full ${r.bn ? "bg-red-400 shadow-[0_0_12px_rgba(248,113,113,.5)]" : r.hot ? "bg-amber-400" : "bg-emerald-500/60"}`} title={r.bn ? "Bottleneck gate" : r.hot ? "High load" : "Nominal"} />
          </div>
        ))}
      </div>
    </div>
  );
}
