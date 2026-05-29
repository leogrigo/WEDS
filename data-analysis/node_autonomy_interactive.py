#!/usr/bin/env python3
"""
WEDS Node Autonomy Interactive Calculator
=========================================
This script provides a premium, interactive desktop GUI built with Tkinter 
and Matplotlib to simulate and visualize the autonomy lifetime of a WEDS node.

It allows real-time tuning of battery capacity, sleep duration, preheatings, 
and voltage step-down mode, immediately recalculating and plotting all affected variables.
"""

import sys
import tkinter as tk
from tkinter import ttk
import numpy as np
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# ==============================================================================
# CONSTANTS (from data-analysis/duty cycle.txt)
# ==============================================================================
AVG_CURRENT_NO_STEPDOWN = 106.46    # mA
AVG_CURRENT_WITH_STEPDOWN = 96.66   # mA
SINGLE_PREHEATING_TIME = 2.04       # seconds
BASE_ACTIVE_TIME = 7.75             # seconds (derived: 38.35s - 15 * 2.04s)
MINUTES_IN_SEASON = 133920.0        # 3 months, 31 days each

# ==============================================================================
# COLOR PALETTE (Premium Light Theme)
# ==============================================================================
BG_COLOR = "#f3f4f6"        # Main window background
PANEL_COLOR = "#ffffff"     # Main dashboard panel background
SIDEBAR_COLOR = "#ffffff"   # Sidebar background
FG_COLOR = "#1f2937"        # Dark slate gray text
FG_MUTED = "#6b7280"        # Muted gray text
ACCENT_BLUE = "#3b82f6"     # Blue (Theme Accent)
ACCENT_RED = "#ef4444"      # Red (Active state)
ACCENT_GREEN = "#10b981"    # Green (Sleep state/Autonomy)
ACCENT_YELLOW = "#f59e0b"   # Amber/Yellow (Target benchmarks)
BORDER_COLOR = "#e5e7eb"    # Light border line

class AutonomyDashboard(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("WEDS Node Autonomy Simulator")
        self.geometry("1100x700")
        self.configure(bg=BG_COLOR)
        
        # Minimum window size to maintain clean layout
        self.minsize(1000, 650)
        
        # Variables
        self.battery_capacity = tk.DoubleVar(value=3000.0)
        self.preheatings = tk.IntVar(value=15)
        self.sleep_duration = tk.DoubleVar(value=42.4261)
        self.stepdown_active = tk.BooleanVar(value=False)
        self.plot_x_axis = tk.StringVar(value="Sleep Duration")
        
        # Configure custom modern style
        self.setup_styles()
        
        # Build UI layout
        self.build_ui()
        
        # Initialize calculations and plots
        self.recalculate()

    def setup_styles(self):
        self.style = ttk.Style()
        self.style.theme_use("clam")
        
        # Common configuration
        self.style.configure(".", background=BG_COLOR, foreground=FG_COLOR, font=("Segoe UI", 10))
        
        # Frame styles
        self.style.configure("TFrame", background=BG_COLOR)
        self.style.configure("Sidebar.TFrame", background=SIDEBAR_COLOR, bordercolor=BORDER_COLOR)
        self.style.configure("Panel.TFrame", background=PANEL_COLOR)
        
        # Label styles
        self.style.configure("TLabel", background=BG_COLOR, foreground=FG_COLOR)
        self.style.configure("Sidebar.TLabel", background=SIDEBAR_COLOR, foreground=FG_COLOR)
        self.style.configure("Panel.TLabel", background=PANEL_COLOR, foreground=FG_COLOR)
        self.style.configure("Title.TLabel", font=("Segoe UI", 14, "bold"), foreground=ACCENT_BLUE, background=SIDEBAR_COLOR)
        self.style.configure("Section.TLabel", font=("Segoe UI", 11, "bold"), foreground=ACCENT_BLUE, background=SIDEBAR_COLOR)
        
        # Value highlight labels
        self.style.configure("Val.TLabel", font=("Segoe UI", 10, "bold"), foreground=FG_COLOR, background=SIDEBAR_COLOR)
        self.style.configure("ValHighlight.TLabel", font=("Segoe UI", 12, "bold"), foreground=ACCENT_GREEN, background=SIDEBAR_COLOR)
        
        # Control styles
        self.style.configure("TRadiobutton", background=SIDEBAR_COLOR, foreground=FG_COLOR, font=("Segoe UI", 9))
        self.style.map("TRadiobutton", 
                       background=[("selected", SIDEBAR_COLOR), ("active", SIDEBAR_COLOR)],
                       foreground=[("selected", ACCENT_BLUE), ("active", FG_COLOR)])
        
        # Scale/Slider styles
        self.style.configure("TScale", background=SIDEBAR_COLOR, troughcolor=BG_COLOR)

    def build_ui(self):
        # ----------------------------------------------------------------------
        # Sidebar Frame (Inputs & Metrics)
        # ----------------------------------------------------------------------
        sidebar = ttk.Frame(self, style="Sidebar.TFrame", width=340)
        sidebar.pack(side=tk.LEFT, fill=tk.Y, padx=0, pady=0)
        sidebar.pack_propagate(False)
        
        # Sidebar title
        title_lbl = ttk.Label(sidebar, text="Node Autonomy Simulator", style="Title.TLabel")
        title_lbl.pack(anchor=tk.W, padx=20, pady=(20, 5))
        
        subtitle_lbl = ttk.Label(sidebar, text="Interactive lifetime data analyzer", font=("Segoe UI", 9, "italic"), foreground=FG_MUTED, style="Sidebar.TLabel")
        subtitle_lbl.pack(anchor=tk.W, padx=20, pady=(0, 20))
        
        # Separator line
        sep1 = ttk.Separator(sidebar, orient="horizontal")
        sep1.pack(fill=tk.X, padx=20, pady=(0, 15))
        
        # --- System Parameters Section ---
        param_title = ttk.Label(sidebar, text="System Parameters", style="Section.TLabel")
        param_title.pack(anchor=tk.W, padx=20, pady=(0, 10))
        
        # 1. Voltage Step-Down Toggle
        stepdown_frame = ttk.Frame(sidebar, style="Sidebar.TFrame")
        stepdown_frame.pack(fill=tk.X, padx=20, pady=5)
        
        sd_lbl = ttk.Label(stepdown_frame, text="Voltage Step-Down Regulator:", font=("Segoe UI", 9, "bold"), style="Sidebar.TLabel")
        sd_lbl.pack(anchor=tk.W, pady=(0, 5))
        
        r_off = ttk.Radiobutton(stepdown_frame, text="OFF (106.46 mA Avg active current)", variable=self.stepdown_active, value=False, command=self.recalculate)
        r_off.pack(anchor=tk.W, padx=5, pady=2)
        r_on = ttk.Radiobutton(stepdown_frame, text="ON (96.66 mA Avg active current)", variable=self.stepdown_active, value=True, command=self.recalculate)
        r_on.pack(anchor=tk.W, padx=5, pady=2)
        
        # 2. Battery Capacity Slider
        self.slider_bat = self.create_slider_group(
            sidebar, 
            label="Battery Capacity (mAh):", 
            variable=self.battery_capacity, 
            from_=1000, to=10000, 
            resolution=100,
            val_format="{:,.0f} mAh"
        )
        
        # 3. Preheatings Slider
        self.slider_pre = self.create_slider_group(
            sidebar, 
            label="Number of Preheatings:", 
            variable=self.preheatings, 
            from_=0, to=50, 
            resolution=1,
            val_format="{:d}"
        )
        
        # 4. Sleep Duration Slider
        self.slider_sleep = self.create_slider_group(
            sidebar, 
            label="Sleep Duration per cycle (min):", 
            variable=self.sleep_duration, 
            from_=1.0, to=180.0, 
            resolution=0.5,
            val_format="{:.2f} min"
        )
        
        # Separator line
        sep2 = ttk.Separator(sidebar, orient="horizontal")
        sep2.pack(fill=tk.X, padx=20, pady=15)
        
        # --- Recalculated Metrics Section ---
        metrics_title = ttk.Label(sidebar, text="Recalculated Metrics", style="Section.TLabel")
        metrics_title.pack(anchor=tk.W, padx=20, pady=(0, 10))
        
        self.metrics_frame = ttk.Frame(sidebar, style="Sidebar.TFrame")
        self.metrics_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=0)
        
        # We will dynamically update these labels in self.recalculate()
        self.lbl_val_avg_current = self.create_metric_row("Active Avg Current:")
        self.lbl_val_dc_time = self.create_metric_row("Duty Cycle Active Time:")
        self.lbl_val_dc_capacity = self.create_metric_row("Battery Life (Duty Cycles):")
        self.lbl_val_total_active = self.create_metric_row("Total Active Time:")
        self.lbl_val_total_sleep = self.create_metric_row("Total Sleep Time:")
        self.lbl_val_required_sleep = self.create_metric_row("Sleep for 1-Season Target:", highlight=True)
        self.lbl_val_autonomy = self.create_metric_row("Total Autonomy Lifetime:", highlight=True)
        self.lbl_val_seasons = self.create_metric_row("Autonomy in Seasons:")
        
        # Footer
        footer_lbl = ttk.Label(sidebar, text="Constants based on WEDS experiments", font=("Segoe UI", 8, "italic"), foreground=FG_MUTED, style="Sidebar.TLabel")
        footer_lbl.pack(side=tk.BOTTOM, anchor=tk.CENTER, pady=15)

        # ----------------------------------------------------------------------
        # Right Plot Area
        # ----------------------------------------------------------------------
        right_panel = ttk.Frame(self, style="Panel.TFrame")
        right_panel.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=15, pady=15)
        
        # Plot controller frame (top of right panel)
        plot_ctrl = ttk.Frame(right_panel, style="Panel.TFrame")
        plot_ctrl.pack(fill=tk.X, padx=10, pady=(0, 10))
        
        ctrl_lbl = ttk.Label(plot_ctrl, text="Plot Sweep Relationship vs:", style="Panel.TLabel", font=("Segoe UI", 10, "bold"))
        ctrl_lbl.pack(side=tk.LEFT, padx=(5, 10))
        
        for option in ["Sleep Duration", "Battery Capacity", "Preheatings"]:
            rb = ttk.Radiobutton(plot_ctrl, text=option, variable=self.plot_x_axis, value=option, command=self.recalculate, style="PanelRadio.TRadiobutton")
            # We configure style manually to fit the main panel
            rb.configure(style="TRadiobutton")
            rb.pack(side=tk.LEFT, padx=10)
            
        # Matplotlib Figure Setup
        plt.style.use("default")  # Use light theme defaults
        self.fig, (self.ax_pie, self.ax_curve) = plt.subplots(1, 2, figsize=(8, 4.5), facecolor=PANEL_COLOR)
        
        self.canvas = FigureCanvasTkAgg(self.fig, master=right_panel)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
    def create_slider_group(self, parent, label, variable, from_, to, resolution, val_format):
        frame = ttk.Frame(parent, style="Sidebar.TFrame")
        frame.pack(fill=tk.X, padx=20, pady=6)
        
        lbl_frame = ttk.Frame(frame, style="Sidebar.TFrame")
        lbl_frame.pack(fill=tk.X)
        
        lbl = ttk.Label(lbl_frame, text=label, font=("Segoe UI", 9), style="Sidebar.TLabel")
        lbl.pack(side=tk.LEFT)
        
        val_frame = ttk.Frame(lbl_frame, style="Sidebar.TFrame")
        val_frame.pack(side=tk.RIGHT)
        
        entry = ttk.Entry(val_frame, width=7, font=("Segoe UI", 9, "bold"), justify=tk.RIGHT)
        entry.pack(side=tk.LEFT)
        
        # Parse unit
        unit = ""
        if "mAh" in val_format:
            unit = " mAh"
        elif "min" in val_format:
            unit = " min"
            
        if unit:
            unit_lbl = ttk.Label(val_frame, text=unit, font=("Segoe UI", 9), style="Sidebar.TLabel")
            unit_lbl.pack(side=tk.LEFT, padx=(2, 0))
            
        def update_entry_from_val(val_float):
            if resolution >= 1:
                text = f"{int(val_float)}"
            else:
                text = f"{val_float:.2f}"
            entry.delete(0, tk.END)
            entry.insert(0, text)
            
        # Callback wrapper to update entry text and recalculate
        def on_slider_move(val):
            val_float = float(val)
            if self.focus_get() != entry:
                update_entry_from_val(val_float)
            self.recalculate()
            
        scale = ttk.Scale(
            frame, 
            from_=from_, to=to, 
            variable=variable, 
            command=on_slider_move,
            style="TScale"
        )
        scale.pack(fill=tk.X, pady=(2, 0))
        
        def on_entry_change(*args):
            raw_val = entry.get().strip()
            if not raw_val:
                return
            try:
                val = float(raw_val)
                val_clamped = max(from_, min(to, val))
                if abs(variable.get() - val_clamped) > 0.001:
                    variable.set(val_clamped)
                    scale.set(val_clamped)
                    self.recalculate()
            except ValueError:
                pass
                
        def on_entry_commit(*args):
            raw_val = entry.get().strip()
            try:
                val = float(raw_val)
                val_clamped = max(from_, min(to, val))
                variable.set(val_clamped)
                scale.set(val_clamped)
                self.recalculate()
                update_entry_from_val(val_clamped)
            except ValueError:
                update_entry_from_val(variable.get())
                
        entry.bind("<KeyRelease>", on_entry_change)
        entry.bind("<Return>", on_entry_commit)
        entry.bind("<FocusOut>", on_entry_commit)
        
        # Set initial text
        update_entry_from_val(variable.get())
            
        return scale

    def create_metric_row(self, label_text, highlight=False):
        row = ttk.Frame(self.metrics_frame, style="Sidebar.TFrame")
        row.pack(fill=tk.X, pady=4)
        
        lbl = ttk.Label(row, text=label_text, style="Sidebar.TLabel", font=("Segoe UI", 9))
        lbl.pack(side=tk.LEFT)
        
        style_name = "ValHighlight.TLabel" if highlight else "Val.TLabel"
        val = ttk.Label(row, text="", style=style_name)
        val.pack(side=tk.RIGHT)
        return val

    def recalculate(self, *args):
        # 1. Retrieve current inputs
        c_bat = self.battery_capacity.get()
        n_pre = self.preheatings.get()
        t_sleep = self.sleep_duration.get()
        is_stepdown = self.stepdown_active.get()
        
        i_avg = AVG_CURRENT_WITH_STEPDOWN if is_stepdown else AVG_CURRENT_NO_STEPDOWN
        
        # 2. Perform math calculations
        t_dc = BASE_ACTIVE_TIME + n_pre * SINGLE_PREHEATING_TIME
        
        # Battery capacity in mA-seconds: C_bat * 3600
        # Battery duration in cycles: (C_bat * 3600) / (I_avg * T_dc)
        n_dc = (c_bat * 3600.0) / (i_avg * t_dc)
        
        # Total active time and sleep time over the battery capacity
        t_active_total_min = (n_dc * t_dc) / 60.0
        t_sleep_total_min = n_dc * t_sleep
        
        # Total autonomy lifetime
        t_lifetime_min = t_active_total_min + t_sleep_total_min
        t_lifetime_days = t_lifetime_min / 1440.0
        t_lifetime_seasons = t_lifetime_min / MINUTES_IN_SEASON
        
        # Required sleep duration per cycle to achieve 1 season (133,920 minutes) of autonomy
        # Target: T_active_total_min + N_dc * T_sleep_req = 133920.0
        # T_sleep_req = (133920.0 - T_active_total_min) / N_dc
        t_sleep_required_for_season = (MINUTES_IN_SEASON - t_active_total_min) / n_dc
        
        # 3. Update Text Labels in Sidebar
        self.lbl_val_avg_current.configure(text=f"{i_avg:.2f} mA")
        self.lbl_val_dc_time.configure(text=f"{t_dc:.2f} s")
        self.lbl_val_dc_capacity.configure(text=f"{n_dc:,.2f} cycles")
        self.lbl_val_total_active.configure(text=f"{t_active_total_min:,.1f} min ({t_active_total_min/60:.2f} h)")
        self.lbl_val_total_sleep.configure(text=f"{t_sleep_total_min:,.0f} min ({t_sleep_total_min/1440:.1f} d)")
        
        if t_sleep_required_for_season > 0:
            self.lbl_val_required_sleep.configure(text=f"{t_sleep_required_for_season:.2f} min", style="Val.TLabel")
        else:
            self.lbl_val_required_sleep.configure(text="Achieved!", style="ValHighlight.TLabel")
            
        self.lbl_val_autonomy.configure(text=f"{t_lifetime_days:.2f} days")
        self.lbl_val_seasons.configure(text=f"{t_lifetime_seasons:.3f} seasons")
        
        # 4. Refresh Plots
        self.update_plots(t_dc, t_sleep, c_bat, i_avg, n_dc, t_active_total_min, t_lifetime_days)

    def update_plots(self, t_dc, t_sleep, c_bat, i_avg, n_dc, t_active_total_min, current_lifetime_days):
        self.ax_pie.clear()
        self.ax_curve.clear()
        
        # ----------------------------------------------------------------------
        # Pie Chart: Active vs Sleep ratio in a single cycle
        # ----------------------------------------------------------------------
        labels = ["Active", "Sleep"]
        sizes = [t_dc, t_sleep * 60.0]
        colors = [ACCENT_RED, ACCENT_BLUE]
        
        # Donut Chart with pctdistance=0.78 to move numbers inside the colored ring
        wedges, texts, autotexts = self.ax_pie.pie(
            sizes, 
            labels=labels, 
            autopct="%1.2f%%",
            pctdistance=0.78, 
            startangle=90, 
            colors=colors,
            textprops=dict(color=FG_COLOR, fontsize=8),
            wedgeprops=dict(width=0.4, edgecolor=PANEL_COLOR) # width=0.4 creates the donut hole
        )
        
        # Make the percentage labels bold white for high legibility inside the colored ring
        for autotext in autotexts:
            autotext.set_color("#ffffff")
            autotext.set_weight("bold")
            autotext.set_fontsize(8.5)
            
        self.ax_pie.set_title("Duty Cycle Time Ratio", color=FG_COLOR, fontsize=10, fontweight="bold", pad=8)
        
        # ----------------------------------------------------------------------
        # Curve Plot: Parameter Sweep
        # ----------------------------------------------------------------------
        sweep_mode = self.plot_x_axis.get()
        
        if sweep_mode == "Sleep Duration":
            # X: Sleep Duration from 1 to 180 min
            x_vals = np.linspace(1.0, 180.0, 200)
            # Battery capacity and preheatings are held constant
            # Lifetime = Active time + N_dc * Sleep
            y_vals = (t_active_total_min + n_dc * x_vals) / 1440.0
            
            x_label = "Sleep Duration per cycle (minutes)"
            y_label = "Autonomy Lifetime (days)"
            title = "Autonomy vs. Sleep Duration"
            current_x = t_sleep
            current_y = current_lifetime_days
            
        elif sweep_mode == "Battery Capacity":
            # X: Battery Capacity from 500 to 10000 mAh
            x_vals = np.linspace(500, 10000, 200)
            # Preheatings and sleep duration are held constant
            # N_dc_vals = (x_vals * 3600) / (I_avg * T_dc)
            # Lifetime = N_dc_vals * (T_dc / 60 + Sleep)
            n_dc_vals = (x_vals * 3600.0) / (i_avg * t_dc)
            y_vals = (n_dc_vals * (t_dc / 60.0 + t_sleep)) / 1440.0
            
            x_label = "Battery Capacity (mAh)"
            y_label = "Autonomy Lifetime (days)"
            title = "Autonomy vs. Battery Capacity"
            current_x = c_bat
            current_y = current_lifetime_days
            
        else: # "Preheatings"
            # X: Preheatings from 0 to 50
            x_vals = np.arange(0, 51)
            # Battery capacity and sleep duration are held constant
            t_dc_vals = BASE_ACTIVE_TIME + x_vals * SINGLE_PREHEATING_TIME
            n_dc_vals = (c_bat * 3600.0) / (i_avg * t_dc_vals)
            y_vals = (n_dc_vals * (t_dc_vals / 60.0 + t_sleep)) / 1440.0
            
            x_label = "Number of Preheatings"
            y_label = "Autonomy Lifetime (days)"
            title = "Autonomy vs. Preheatings"
            current_x = self.preheatings.get()
            current_y = current_lifetime_days
            
        # Plot curve
        self.ax_curve.plot(x_vals, y_vals, color=ACCENT_GREEN, linewidth=2, label="Sweep Curve")
        
        # Plot current operating point
        self.ax_curve.plot(current_x, current_y, "o", color=ACCENT_RED, markersize=7, label="Current Point")
        
        # Add horizontal dashed line indicating 1 Season (133,920 mins = 93 days)
        season_days = MINUTES_IN_SEASON / 1440.0
        self.ax_curve.axhline(season_days, color=ACCENT_YELLOW, linestyle="--", linewidth=1, alpha=0.7, label="1 Season Target (93d)")
        
        # Labeling and styling axes
        self.ax_curve.set_title(title, color=FG_COLOR, fontsize=10, fontweight="bold", pad=8)
        self.ax_curve.set_xlabel(x_label, color=FG_MUTED, fontsize=8)
        self.ax_curve.set_ylabel(y_label, color=FG_MUTED, fontsize=8)
        self.ax_curve.grid(True, linestyle=":", alpha=0.5, color=BORDER_COLOR)
        self.ax_curve.tick_params(colors=FG_MUTED, labelsize=8)
        
        # Show legend
        self.ax_curve.legend(loc="best", frameon=True, facecolor=PANEL_COLOR, edgecolor=BORDER_COLOR, fontsize=8, labelcolor=FG_COLOR)
        
        # Set panel backgrounds
        self.ax_pie.set_facecolor(PANEL_COLOR)
        self.ax_curve.set_facecolor(PANEL_COLOR)
        
        # Style spines
        for ax in [self.ax_pie, self.ax_curve]:
            for spine in ax.spines.values():
                spine.set_color(BORDER_COLOR)
                
        self.fig.tight_layout()
        self.canvas.draw()

# ==============================================================================
# ENTRY POINT
# ==============================================================================
if __name__ == "__main__":
    app = AutonomyDashboard()
    app.mainloop()
