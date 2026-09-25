#!/usr/bin/env python3
"""Analyze a capture from tools/pin-sniffer's RP2040 firmware.

The sniffer logs one row every time any watched GPIO changes, so this never
sees a "steady" sample — every row is an edge on at least one column. That
turns pin identification into a per-column stats problem:

  - a column that never changes is either a power/ground pin, an unused
    pin, or a control line that happens to sit idle the whole capture
  - the busiest column is the best strobe candidate (e.g. WR on an 8080-
    style parallel LCD bus toggles roughly once per byte written)
  - columns that (almost) only change in the same row as the strobe are
    riding the same bus transaction as it (data bits, or another control
    line sampled/asserted alongside it)
  - a column that changes far less often than the strobe, in bursts, is a
    good RS/DC (command vs. data) candidate — real displays send far more
    pixel bytes than command bytes
  - a column that's asserted for one long low/high stretch and doesn't
    toggle with the strobe is a good CS or RESET candidate

None of this replaces reading the numbers yourself — it's a ranked hint,
not a verdict.

Usage:
    python tools/pin_sniffer_analyze.py capture.csv
    python tools/pin_sniffer_analyze.py raw_serial_log.txt   # BEGIN/END markers are found automatically
"""
import csv
import sys
from pathlib import Path


def extract_csv_block(text: str) -> str:
    """Pull the CSV between a BEGIN_*/END_* marker pair if present, else return as-is.

    Handles both capture modes from the sniffer firmware: the change-triggered
    'a' capture (BEGIN_CAPTURE/END_CAPTURE, a real delta_us column) and the
    fixed-rate PIO 's' capture (BEGIN_PIO_CAPTURE/END_PIO_CAPTURE, a constant
    delta_ns column plus a leading sample_hz= line to skip).
    """
    for begin_marker, end_marker in (("BEGIN_PIO_CAPTURE", "END_PIO_CAPTURE"),
                                      ("BEGIN_CAPTURE", "END_CAPTURE")):
        begin = text.find(begin_marker)
        end = text.find(end_marker)
        if begin != -1 and end != -1:
            block = text[begin + len(begin_marker):end].strip()
            lines = block.splitlines()
            if lines and lines[0].startswith("sample_hz="):
                lines = lines[1:]
            return "\n".join(lines)
    return text.strip()


def load_rows(path: Path):
    text = path.read_text(errors="replace")
    block = extract_csv_block(text)
    reader = csv.reader(block.splitlines())
    header = next(reader)
    pin_cols = header[2:]  # skip idx, delta_us
    rows = [row for row in reader if len(row) == len(header)]
    return pin_cols, rows


def analyze(pin_cols, rows):
    n = len(rows)
    if n == 0:
        print("No data rows found.")
        return

    values = {name: [int(row[2 + i]) for row in rows] for i, name in enumerate(pin_cols)}
    edge_counts = {}
    high_counts = {}
    for name, vals in values.items():
        edges = sum(1 for i in range(1, n) if vals[i] != vals[i - 1])
        edge_counts[name] = edges
        high_counts[name] = sum(vals)

    busiest = max(edge_counts, key=edge_counts.get)
    busiest_vals = values[busiest]

    # For each column, what fraction of ITS edges land on a row where the
    # busiest column also changed (i.e. rides the same bus transaction)?
    sync_score = {}
    for name, vals in values.items():
        if name == busiest:
            continue
        own_edges = 0
        synced = 0
        for i in range(1, n):
            if vals[i] != vals[i - 1]:
                own_edges += 1
                if busiest_vals[i] != busiest_vals[i - 1]:
                    synced += 1
        sync_score[name] = (synced / own_edges) if own_edges else 0.0

    print(f"Rows: {n}  (each row is a change event, not a fixed-rate sample)\n")
    print(f"{'pin':<8}{'edges':>8}{'duty(high%)':>13}{'sync w/ busiest':>17}")
    print("-" * 46)
    for name in pin_cols:
        duty = 100.0 * high_counts[name] / n
        sync = "-" if name == busiest else f"{100.0 * sync_score[name]:.0f}%"
        marker = "  <- busiest (likely a strobe)" if name == busiest else ""
        print(f"{name:<8}{edge_counts[name]:>8}{duty:>12.1f}%{sync:>17}{marker}")

    print()
    print("Heuristic notes:")
    busiest_edges = edge_counts[busiest]
    for name in pin_cols:
        if name == busiest:
            continue
        edges = edge_counts[name]
        ratio = (edges / busiest_edges) if busiest_edges else 0.0
        sync = sync_score.get(name, 0)
        if edges == 0:
            level = "high" if high_counts[name] == n else "low"
            print(f"  {name}: never toggled, held {level} the whole capture "
                  f"-> static control line (CS asserted? RESET inactive?) or unused/NC")
        elif sync > 0.8 and ratio < 0.15:
            print(f"  {name}: toggles rarely ({edges} times vs {busiest_edges} for {busiest}), "
                  f"always alongside it -> good RS/DC (command vs. data) candidate")
        elif sync > 0.8 and ratio >= 0.5:
            print(f"  {name}: toggles almost every time {busiest} does "
                  f"-> probably another data bit, or a second strobe on the same bus")
        elif sync > 0.8:
            print(f"  {name}: toggles alongside {busiest} but less often "
                  f"-> plausibly a data bit (real image/text data won't flip every "
                  f"cycle the way random test data does) - check against known byte "
                  f"values if possible")
        elif sync < 0.3:
            print(f"  {name}: mostly changes independently of {busiest} "
                  f"-> maybe RESET, or an unrelated/async line")


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    path = Path(sys.argv[1])
    pin_cols, rows = load_rows(path)
    analyze(pin_cols, rows)


if __name__ == "__main__":
    main()
