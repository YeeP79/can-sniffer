#!/usr/bin/env python3
"""
Moon Patrol iBooster DAQ — Log Analysis Script

Loads CSV log files from the SD card, parses CAN frames and pressure data,
prints summary statistics, and generates plots for protocol analysis.

Usage:
    # Activate venv first:
    source venv/bin/activate

    python analysis/analyze.py <logfile.csv>
    python analysis/analyze.py analysis/sample_data/run_001.csv
"""

import sys
import os
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend for saving plots


PLOTS_DIR = Path(__file__).parent / "plots"


def load_log(filepath):
    """Load a CSV log file, skipping comment lines (descriptions)."""
    # Skip lines starting with '#' (run descriptions)
    lines = []
    header = None
    with open(filepath, 'r') as f:
        for line in f:
            if line.startswith('#'):
                continue
            if header is None:
                header = line.strip()
                continue
            lines.append(line.strip())

    if header is None:
        print("ERROR: No header found in log file")
        sys.exit(1)

    columns = header.split(',')

    # Parse data rows
    rows = []
    for line in lines:
        if not line:
            continue
        parts = line.split(',')
        rows.append(parts)

    df = pd.DataFrame(rows, columns=columns)

    # Convert types
    df['timestamp_ms'] = pd.to_numeric(df['timestamp_ms'], errors='coerce')

    return df


def split_can_pressure(df):
    """Split log into CAN frames and pressure readings."""
    can_df = df[df['source'].str.startswith('CAN')].copy()
    psi_df = df[df['source'] == 'PSI'].copy()

    # Parse pressure values
    if not psi_df.empty:
        psi_df['psi_front'] = pd.to_numeric(psi_df['psi_front'], errors='coerce')
        psi_df['psi_rear'] = pd.to_numeric(psi_df['psi_rear'], errors='coerce')

    # Parse CAN data bytes to integers
    data_cols = ['d0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7']
    for col in data_cols:
        if col in can_df.columns:
            can_df[col] = can_df[col].apply(
                lambda x: int(x, 16) if isinstance(x, str) and x.strip() else None
            )

    return can_df, psi_df


def print_summary(can_df, psi_df, filepath):
    """Print a summary of the log file."""
    print(f"\n{'='*60}")
    print(f"  Log Analysis: {os.path.basename(filepath)}")
    print(f"{'='*60}")

    total = len(can_df) + len(psi_df)
    if total == 0:
        print("  No data found in log file.")
        return

    t_min = min(
        can_df['timestamp_ms'].min() if not can_df.empty else float('inf'),
        psi_df['timestamp_ms'].min() if not psi_df.empty else float('inf'),
    )
    t_max = max(
        can_df['timestamp_ms'].max() if not can_df.empty else 0,
        psi_df['timestamp_ms'].max() if not psi_df.empty else 0,
    )
    duration_s = (t_max - t_min) / 1000.0

    print(f"  Total samples:  {total}")
    print(f"  Duration:       {duration_s:.1f} s")
    print()

    # CAN summary
    if not can_df.empty:
        for bus in sorted(can_df['source'].unique()):
            bus_df = can_df[can_df['source'] == bus]
            print(f"  --- {bus} ---")
            ids = bus_df['can_id'].value_counts().sort_index()
            print(f"  Unique IDs: {len(ids)}")
            print(f"  {'ID':<10} {'Count':>8} {'Freq (Hz)':>10}")
            for can_id, count in ids.items():
                freq = count / duration_s if duration_s > 0 else 0
                print(f"  {can_id:<10} {count:>8} {freq:>10.1f}")
            print()

    # Pressure summary
    if not psi_df.empty:
        print("  --- Pressure ---")
        for col, label in [('psi_front', 'Front'), ('psi_rear', 'Rear')]:
            vals = psi_df[col].dropna()
            if not vals.empty:
                print(f"  {label}: min={vals.min():.1f}  max={vals.max():.1f}  "
                      f"avg={vals.mean():.1f}  PSI")
        print()


def plot_pressure(psi_df, run_name):
    """Plot pressure vs time."""
    if psi_df.empty:
        print("  No pressure data to plot.")
        return

    t = (psi_df['timestamp_ms'] - psi_df['timestamp_ms'].iloc[0]) / 1000.0

    fig, ax = plt.subplots(figsize=(12, 5))
    if psi_df['psi_front'].notna().any():
        ax.plot(t, psi_df['psi_front'], label='Front', linewidth=0.8)
    if psi_df['psi_rear'].notna().any():
        ax.plot(t, psi_df['psi_rear'], label='Rear', linewidth=0.8)

    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Pressure (PSI)')
    ax.set_title(f'{run_name} — Brake Pressure')
    ax.legend()
    ax.grid(True, alpha=0.3)

    outpath = PLOTS_DIR / f"{run_name}_pressure.png"
    fig.savefig(outpath, dpi=150, bbox_inches='tight')
    plt.close(fig)
    print(f"  Saved: {outpath}")


def plot_can_bytes(can_df, run_name):
    """For each unique CAN ID, plot each data byte vs time."""
    if can_df.empty:
        print("  No CAN data to plot.")
        return

    data_cols = ['d0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7']

    for bus in sorted(can_df['source'].unique()):
        bus_df = can_df[can_df['source'] == bus]
        unique_ids = sorted(bus_df['can_id'].unique())

        for can_id in unique_ids:
            id_df = bus_df[bus_df['can_id'] == can_id].copy()
            t = (id_df['timestamp_ms'] - id_df['timestamp_ms'].iloc[0]) / 1000.0

            # Find which bytes actually change
            changing = []
            for col in data_cols:
                vals = id_df[col].dropna()
                if len(vals) > 0 and vals.nunique() > 1:
                    changing.append(col)

            if not changing:
                continue  # Skip IDs with static data

            n_plots = len(changing)
            fig, axes = plt.subplots(n_plots, 1, figsize=(12, 2.5 * n_plots),
                                     sharex=True)
            if n_plots == 1:
                axes = [axes]

            for ax, col in zip(axes, changing):
                ax.plot(t, id_df[col], linewidth=0.5, marker='.', markersize=1)
                ax.set_ylabel(col.upper())
                ax.grid(True, alpha=0.3)
                ax.set_ylim(-5, 260)

            axes[-1].set_xlabel('Time (s)')
            safe_id = can_id.replace('0x', '')
            fig.suptitle(f'{run_name} — {bus} ID {can_id} — Changing Bytes',
                         fontsize=12)
            fig.tight_layout()

            outpath = PLOTS_DIR / f"{run_name}_{bus}_{safe_id}_bytes.png"
            fig.savefig(outpath, dpi=150, bbox_inches='tight')
            plt.close(fig)
            print(f"  Saved: {outpath}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python analyze.py <logfile.csv>")
        print("Example: python analyze.py run_001.csv")
        sys.exit(1)

    filepath = sys.argv[1]
    if not os.path.exists(filepath):
        print(f"ERROR: File not found: {filepath}")
        sys.exit(1)

    run_name = Path(filepath).stem

    # Ensure plots directory exists
    PLOTS_DIR.mkdir(parents=True, exist_ok=True)

    # Load and parse
    print(f"Loading {filepath}...")
    df = load_log(filepath)
    can_df, psi_df = split_can_pressure(df)

    # Summary
    print_summary(can_df, psi_df, filepath)

    # Plots
    print("Generating plots...")
    plot_pressure(psi_df, run_name)
    plot_can_bytes(can_df, run_name)

    print(f"\nDone. Plots saved to {PLOTS_DIR}/")


if __name__ == '__main__':
    main()
