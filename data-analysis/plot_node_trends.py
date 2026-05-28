#!/usr/bin/env python3
"""
WEDS Node Trend Plotting Utility
===============================
This script finds the latest JSON state registry file in the data-analysis/data
directory, parses the historical trend data and active anomaly events for each node,
and generates premium, high-resolution multi-panel plots for analysis.

Usage:
  python plot_node_trends.py [-f path/to/file.json] [-o path/to/output_dir] [--show]
"""

import os
import sys
import glob
import json
import argparse
from datetime import datetime, timezone
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# Set clean styling defaults for matplotlib
plt.rcParams['font.family'] = 'sans-serif'
plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial', 'Helvetica']
plt.rcParams['axes.edgecolor'] = '#888888'
plt.rcParams['axes.linewidth'] = 0.8
plt.rcParams['xtick.color'] = '#333333'
plt.rcParams['ytick.color'] = '#333333'

def find_latest_json(data_dir):
    """
    Finds the most recent JSON registry state file in the given directory.
    Uses sorting by filename (since it includes an ISO timestamp) with a fallback to modification time.
    """
    json_files = glob.glob(os.path.join(data_dir, '*.json'))
    if not json_files:
        return None
    try:
        # Sort based on name to get the latest lexicographical file
        return max(json_files, key=os.path.basename)
    except Exception:
        return max(json_files, key=os.path.getmtime)

def plot_node_data(node, source_filename, output_dir, show_plots=False):
    """
    Plots trend metrics for a given node and saves/displays the figure.
    """
    node_id = node.get('node_id')
    trend = node.get('trend', [])
    events = node.get('events', [])
    
    # Filter and sort trend points by timestamp to prevent sorting issues or anomalies
    valid_trend = [p for p in trend if p.get('timestamp_s', 0) > 0]
    if not valid_trend:
        print(f"[-] Node {node_id}: No valid trend data points found.")
        return None
        
    valid_trend.sort(key=lambda x: x['timestamp_s'])
    
    # Convert timestamps to local datetime objects
    timestamps = [datetime.fromtimestamp(p['timestamp_s']) for p in valid_trend]
    
    # Extract metrics
    temp = [p.get('temperature', 0.0) for p in valid_trend]
    humidity = [p.get('humidity', 0.0) for p in valid_trend]
    pressure = [p.get('pressure', 0.0) / 100.0 for p in valid_trend]  # Pa to hPa
    gas = [p.get('gas_resistance', 0.0) / 1000.0 for p in valid_trend]  # Ohm to kOhm
    battery = [p.get('battery_level', 0.0) for p in valid_trend]
    anomaly_score = [p.get('anomaly_score', 0.0) for p in valid_trend]
    risk_score = [p.get('risk_score', 0.0) for p in valid_trend]
    
    # Color palette
    colors = {
        'temp': '#e63946',        # Coral / Red
        'hum': '#457b9d',         # Steel Blue
        'press': '#2a9d8f',       # Teal
        'gas': '#8338ec',         # Purple
        'battery': '#4caf50',     # Green
        'anomaly': '#f77f00',     # Orange
        'risk': '#d62828',        # Dark Red
        'event_shade': '#ffccd5'   # Soft alert pink
    }
    
    # Create subplots (3 rows, 2 columns)
    fig, axes = plt.subplots(3, 2, figsize=(15, 10), sharex=True)
    
    # Build title with metadata
    title_text = f"Sensor & Anomaly Metrics — Node {node_id}"
    subtitle_text = f"Source: {source_filename} | Timezone: Local System Time"
    fig.suptitle(title_text + "\n" + subtitle_text, fontsize=15, fontweight='bold', color='#1a1a1a', y=0.98)
    
    # Helper to overlay event intervals
    def overlay_events(ax):
        event_shaded = False
        for ev in events:
            start_ts = ev.get('start_timestamp_s')
            end_ts = ev.get('end_timestamp_s')
            if start_ts and end_ts:
                start_dt = datetime.fromtimestamp(start_ts)
                end_dt = datetime.fromtimestamp(end_ts)
                
                # Check for overlap with the trend timeline
                if timestamps[0] <= end_dt and start_dt <= timestamps[-1]:
                    label = "Active Anomaly Event" if not event_shaded else ""
                    ax.axvspan(start_dt, end_dt, color=colors['event_shade'], alpha=0.35, label=label)
                    event_shaded = True
        return event_shaded

    # Configure subplots: (row, col, data, title, color, ylabel)
    subplots_config = [
        (0, 0, temp, "Temperature", colors['temp'], "Temperature (°C)"),
        (0, 1, humidity, "Humidity", colors['hum'], "Humidity (% RH)"),
        (1, 0, pressure, "Pressure", colors['press'], "Pressure (hPa)"),
        (1, 1, gas, "Gas Resistance", colors['gas'], "Resistance (kΩ)"),
    ]
    
    # Plot standard sensors
    for r, c, data, title, color, ylabel in subplots_config:
        ax = axes[r, c]
        ax.plot(timestamps, data, color=color, linewidth=1.5, label=ylabel)
        ax.set_title(title, fontsize=11, fontweight='semibold', color='#333333')
        ax.set_ylabel(ylabel, fontsize=9, color='#555555')
        ax.grid(True, linestyle=':', alpha=0.6, color='#bbbbbb')
        ax.set_facecolor('#fdfdfd')
        
        has_ev = overlay_events(ax)
        if has_ev:
            ax.legend(loc='upper left', framealpha=0.9, fontsize=8)
            
    # Subplot 5: Anomaly & Risk Scores (bottom-left)
    ax_scores = axes[2, 0]
    ax_scores.plot(timestamps, anomaly_score, color=colors['anomaly'], linewidth=1.5, label='Anomaly Score')
    ax_scores.set_title("Anomaly & Risk Scores", fontsize=11, fontweight='semibold', color='#333333')
    ax_scores.set_ylabel("Anomaly Score", color=colors['anomaly'], fontsize=9)
    ax_scores.tick_params(axis='y', labelcolor=colors['anomaly'])
    ax_scores.grid(True, linestyle=':', alpha=0.6, color='#bbbbbb')
    ax_scores.set_facecolor('#fdfdfd')
    
    # Overlay Risk Score with secondary Y axis
    ax_risk = ax_scores.twinx()
    ax_risk.plot(timestamps, risk_score, color=colors['risk'], linewidth=1.5, linestyle='--', label='Risk Score')
    ax_risk.set_ylabel("Risk Score", color=colors['risk'], fontsize=9)
    ax_risk.tick_params(axis='y', labelcolor=colors['risk'])
    # Set y-limits for risk score to give it some padding
    ax_risk.set_ylim(-0.05, 1.05)
    
    # Combine legends
    lines1, labels1 = ax_scores.get_legend_handles_labels()
    lines2, labels2 = ax_risk.get_legend_handles_labels()
    has_ev = overlay_events(ax_scores)
    if has_ev:
        # Find index of event span label to put in unified legend
        for l, lab in zip(lines1, labels1):
            if lab == "Active Anomaly Event":
                break
        else:
            # Add manually if not added in lines1
            # Retrieve lines/labels again to capture axvspan
            lines1, labels1 = ax_scores.get_legend_handles_labels()
            
    ax_scores.legend(lines1 + lines2, labels1 + labels2, loc='upper left', framealpha=0.9, fontsize=8)

    # Subplot 6: Battery Level (bottom-right)
    ax_bat = axes[2, 1]
    ax_bat.plot(timestamps, battery, color=colors['battery'], linewidth=1.5, label='Battery Level')
    ax_bat.set_title("Battery Level", fontsize=11, fontweight='semibold', color='#333333')
    ax_bat.set_ylabel("Battery (%)", fontsize=9, color='#555555')
    ax_bat.set_ylim(0, 105)
    ax_bat.grid(True, linestyle=':', alpha=0.6, color='#bbbbbb')
    ax_bat.set_facecolor('#fdfdfd')
    
    overlay_events(ax_bat)
    ax_bat.legend(loc='lower left', framealpha=0.9, fontsize=8)

    # Determine dynamic X-axis format based on data duration
    time_diff = timestamps[-1] - timestamps[0]
    if time_diff.days < 1:
        date_fmt = mdates.DateFormatter('%H:%M')
    elif time_diff.days < 7:
        date_fmt = mdates.DateFormatter('%a %H:%M')
    else:
        date_fmt = mdates.DateFormatter('%Y-%m-%d %H:%M')

    # Apply x-axis updates to all axes
    for ax in axes.flat:
        ax.xaxis.set_major_formatter(date_fmt)
        ax.xaxis.set_major_locator(mdates.AutoDateLocator())
        plt.setp(ax.get_xticklabels(), rotation=20, ha='right', fontsize=9)
        
        # Style spines
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)
        ax.spines['left'].set_color('#888888')
        ax.spines['bottom'].set_color('#888888')
        
    # Standardize spines for the secondary risk axis as well
    ax_risk.spines['top'].set_visible(False)
    ax_risk.spines['left'].set_visible(False)
    ax_risk.spines['right'].set_color('#888888')
    ax_risk.spines['bottom'].set_color('#888888')

    plt.tight_layout()
    
    # Save output
    output_filename = f"node_{node_id}_trends.png"
    output_path = os.path.join(output_dir, output_filename)
    plt.savefig(output_path, dpi=150, bbox_inches='tight', facecolor='#f8f9fa')
    print(f"[+] Saved plot for Node {node_id} to: {output_path}")
    
    if show_plots:
        plt.show()
        
    plt.close()
    return output_path

def main():
    parser = argparse.ArgumentParser(description="Plot WEDS node metrics from registry state JSON.")
    parser.add_argument('-f', '--file', type=str, help="Path to specific JSON file (if omitted, uses the latest in data-analysis/data)")
    parser.add_argument('-o', '--output-dir', type=str, help="Directory to save output plots (default: data-analysis/plots)")
    parser.add_argument('--show', action='store_true', help="Interactively show the plots")
    
    args = parser.parse_args()
    
    # Determine directories
    script_dir = os.path.dirname(os.path.abspath(__file__))
    data_dir = os.path.join(script_dir, 'data')
    
    if args.output_dir:
        output_dir = os.path.abspath(args.output_dir)
    else:
        output_dir = os.path.join(script_dir, 'plots')
        
    # Find JSON file
    if args.file:
        json_path = os.path.abspath(args.file)
        if not os.path.exists(json_path):
            print(f"Error: Specified file not found: {json_path}")
            sys.exit(1)
    else:
        json_path = find_latest_json(data_dir)
        if not json_path:
            print(f"Error: No JSON registry files found in {data_dir}")
            sys.exit(1)
            
    print(f"[*] Loading registry state from: {json_path}")
    
    # Load JSON data
    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except Exception as e:
        print(f"Error reading JSON: {e}")
        sys.exit(1)
        
    nodes = data.get('nodes', [])
    if not nodes:
        print("Warning: No nodes found in the registry state file.")
        sys.exit(0)
        
    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)
    
    # Plot each node
    latest_file_name = os.path.basename(json_path)
    plotted_count = 0
    for node in nodes:
        node_id = node.get('node_id')
        if not node_id:
            continue
        print(f"[*] Processing Node {node_id}...")
        path = plot_node_data(node, latest_file_name, output_dir, show_plots=args.show)
        if path:
            plotted_count += 1
            
    print(f"[+] Finished! Generated {plotted_count} plots in {output_dir}")

if __name__ == '__main__':
    main()
