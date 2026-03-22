"""
AI Decision Tree PDF Generator for Planet Conquest
Generates a visual flowchart PDF showing the complete AI decision-making pipeline
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
from matplotlib.backends.backend_pdf import PdfPages
import numpy as np

# Color scheme
COLORS = {
    'input': '#4A90E2',      # Blue
    'process': '#50C878',    # Green
    'decision': '#FFD700',   # Gold/Yellow
    'modifier': '#FF8C42',   # Orange
    'action': '#E74C3C',     # Red
    'background': '#F5F5F5', # Light gray
    'text': '#2C3E50'        # Dark gray
}

def create_box(ax, x, y, width, height, text, color, fontsize=9, bold=False):
    """Create a colored box with text"""
    box = FancyBboxPatch(
        (x - width/2, y - height/2), width, height,
        boxstyle="round,pad=0.05", 
        facecolor=color, 
        edgecolor='black', 
        linewidth=2,
        zorder=2
    )
    ax.add_patch(box)
    
    weight = 'bold' if bold else 'normal'
    ax.text(x, y, text, ha='center', va='center', 
            fontsize=fontsize, weight=weight, color='white' if color != COLORS['decision'] else 'black',
            zorder=3, wrap=True)
    
    return box

def create_arrow(ax, x1, y1, x2, y2, label='', color='black', style='->'):
    """Create an arrow between two points"""
    arrow = FancyArrowPatch(
        (x1, y1), (x2, y2),
        arrowstyle=style, 
        color=color, 
        linewidth=2,
        mutation_scale=20,
        zorder=1
    )
    ax.add_patch(arrow)
    
    if label:
        mid_x, mid_y = (x1 + x2) / 2, (y1 + y2) / 2
        ax.text(mid_x, mid_y, label, ha='center', va='bottom', 
                fontsize=7, style='italic', bbox=dict(boxstyle='round,pad=0.3', 
                facecolor='white', edgecolor='none', alpha=0.8))

def page1_overview(pdf):
    """Page 1: High-level overview of the 8-stage pipeline"""
    fig, ax = plt.subplots(figsize=(11, 8.5))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 12)
    ax.axis('off')
    fig.patch.set_facecolor(COLORS['background'])
    
    # Title
    ax.text(5, 11.5, 'AI Decision-Making Pipeline', 
            ha='center', fontsize=24, weight='bold', color=COLORS['text'])
    ax.text(5, 11, 'Planet Conquest - Complete Flow', 
            ha='center', fontsize=14, style='italic', color=COLORS['text'])
    
    # Pipeline stages (vertical flow)
    stages = [
        (10.0, 'Gather Pressures\n(18 Environmental Sensors)', COLORS['input']),
        (9.0, 'Calculate Utilities\n(8 Action Scores)', COLORS['process']),
        (8.0, 'Apply Strategic Focus\n(Commitment Boost)', COLORS['modifier']),
        (7.0, 'Normalize Priorities\n(Convert to %)', COLORS['process']),
        (6.0, 'Update Strategic Focus\n(Commit/Decay)', COLORS['decision']),
        (5.0, 'Allocate Vehicles\n(Distribute by Priority)', COLORS['process']),
        (4.0, 'Execute Actions\n(Spawn/Assign)', COLORS['action']),
        (3.0, 'Detect & Recover Stuck\n(Reboot Deadlocks)', COLORS['modifier'])
    ]
    
    prev_y = None
    for y, text, color in stages:
        create_box(ax, 5, y, 4, 0.6, text, color, fontsize=11, bold=True)
        if prev_y is not None:
            create_arrow(ax, 5, prev_y - 0.3, 5, y + 0.3)
        prev_y = y
    
    # Loop back arrow
    create_arrow(ax, 5.5, 2.7, 7, 2.7, color=COLORS['modifier'], style='->')
    create_arrow(ax, 7, 2.7, 7, 10.3, color=COLORS['modifier'], style='->')
    create_arrow(ax, 7, 10.3, 5.5, 10.3, 'Every 5 seconds', color=COLORS['modifier'], style='->')
    
    # Key features boxes
    features_y = 1.5
    create_box(ax, 2, features_y, 3, 0.5, 'Economic Reasoning\nFactories > Turrets', COLORS['process'], fontsize=9)
    create_box(ax, 5, features_y, 3, 0.5, 'Strategic Commitment\nWon\'t Flip-Flop', COLORS['decision'], fontsize=9)
    create_box(ax, 8, features_y, 3, 0.5, 'Threat Prediction\n5-Second Lookahead', COLORS['input'], fontsize=9)
    
    # Legend
    legend_y = 0.5
    legend_items = [
        (1, legend_y, COLORS['input'], 'Input'),
        (2.5, legend_y, COLORS['process'], 'Process'),
        (4, legend_y, COLORS['decision'], 'Decision'),
        (5.5, legend_y, COLORS['modifier'], 'Modifier'),
        (7, legend_y, COLORS['action'], 'Action')
    ]
    
    for x, y, color, label in legend_items:
        create_box(ax, x, y, 1, 0.3, label, color, fontsize=8)
    
    pdf.savefig(fig, bbox_inches='tight')
    plt.close()

def page2_pressures(pdf):
    """Page 2: Environmental Pressures (18 sensors)"""
    fig, ax = plt.subplots(figsize=(11, 8.5))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 12)
    ax.axis('off')
    fig.patch.set_facecolor(COLORS['background'])
    
    # Title
    ax.text(5, 11.5, 'Stage 1: Gather Environmental Pressures', 
            ha='center', fontsize=20, weight='bold', color=COLORS['text'])
    ax.text(5, 11, '18 Real-Time Sensors • Updated Every 5 Seconds', 
            ha='center', fontsize=12, style='italic', color=COLORS['text'])
    
    # Grouped pressures
    groups = [
        ('Economic Sensors', 2, 9.5, [
            'ProximalResources (unclaimed within 3000u)',
            'OwnedResources (count)',
            'Money (current balance)'
        ]),
        ('Military Sensors', 5, 9.5, [
            'VehiclesOwned (count)',
            'VehiclesInCombat (active fighters)',
            'ProximalEnemyVehicles (within 3000u)'
        ]),
        ('Territory Sensors', 8, 9.5, [
            'CitiesOwned (count)',
            'FactoriesOwned (count)',
            'TurretsOwned (count)'
        ]),
        ('Threat Sensors', 2, 6.5, [
            'ProximalEnemyCities (within 6000u)',
            'IncomingThreats (5-sec prediction)',
            'CitiesUnderActiveAttack (HP lost)'
        ]),
        ('Status Sensors', 5, 6.5, [
            'LowestCityHealthPercent (weakest city)',
            'StuckVehicles (position tracking)',
            'TotalIncome (per 5-sec tick)'
        ]),
        ('Strategic Sensors', 8, 6.5, [
            'ControlPercentage (territory %)',
            'EnemyStrength (relative power)',
            'ExpansionOpportunities (available cities)'
        ])
    ]
    
    y_offset = 0
    for title, x, y, items in groups:
        # Group title
        ax.text(x, y + 0.5, title, ha='center', fontsize=11, weight='bold', color=COLORS['text'])
        
        # Items
        for i, item in enumerate(items):
            item_y = y - (i * 0.4)
            create_box(ax, x, item_y, 2.5, 0.35, item, COLORS['input'], fontsize=8)
    
    # Special callouts
    create_box(ax, 2, 4.5, 2.5, 0.6, 
               'Incoming Threats:\nVelocity-based\n5-sec lookahead', 
               COLORS['modifier'], fontsize=8, bold=True)
    
    create_box(ax, 5, 4.5, 2.5, 0.6, 
               'Stuck Vehicles:\nExcludes active\ncaptures/attacks', 
               COLORS['modifier'], fontsize=8, bold=True)
    
    create_box(ax, 8, 4.5, 2.5, 0.6, 
               'Cities Under Attack:\nHP delta tracking\nbetween updates', 
               COLORS['modifier'], fontsize=8, bold=True)
    
    # Flow to next stage
    create_box(ax, 5, 2, 4, 0.6, 
               '18 Pressure Values → Calculate Utilities', 
               COLORS['process'], fontsize=12, bold=True)
    
    create_arrow(ax, 5, 2.5, 5, 1.4)
    
    # Key insight
    ax.text(5, 0.5, 
            'Key: Pressures are normalized observations (0.0-1.0 or counts). They feed into utility calculations.',
            ha='center', fontsize=9, style='italic', 
            bbox=dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor=COLORS['input'], linewidth=2))
    
    pdf.savefig(fig, bbox_inches='tight')
    plt.close()

def page3_utilities(pdf):
    """Page 3: Utility Calculations (8 actions)"""
    fig, ax = plt.subplots(figsize=(11, 8.5))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 12)
    ax.axis('off')
    fig.patch.set_facecolor(COLORS['background'])
    
    # Title
    ax.text(5, 11.5, 'Stage 2: Calculate Utility Scores', 
            ha='center', fontsize=20, weight='bold', color=COLORS['text'])
    ax.text(5, 11, '8 Actions Evaluated • Personality-Modified', 
            ha='center', fontsize=12, style='italic', color=COLORS['text'])
    
    # Actions with formulas (simplified)
    actions_left = [
        ('Capture Resources', 10.2, 
         'Base: 0.7 + Greed×0.3\n+Proximity, +Safety, +Money\nSaturates at 10 resources'),
        ('Capture Enemy Resources', 9.0,
         'Base: 0.5 + Agg×0.3 + Greed×0.2\n+Conflict willingness'),
        ('Build Vehicles', 7.8,
         'Base: 0.5\n+Safety bonus, $1k-$3k threshold\nScales with money & threats'),
        ('Build Turrets', 6.6,
         'Base: 0.2 + Def×0.3\nREQUIRES threats, $2.5k-$6k\nSituational defense')
    ]
    
    actions_right = [
        ('Build Factories', 10.2,
         'Base: 0.5 + Greed×0.4\nIncome calc, ROI 2.5min\nCap at 15 factories'),
        ('Capture Cities', 9.0,
         'Assess all cities\nConfidence×0.7 + Value×0.3\nValue = Weakness × Resources'),
        ('Attack Vehicles', 7.8,
         'Base: Agg×0.7 + Def×0.3\n+Superiority awareness\nTargets nearby enemies'),
        ('Defend Cities', 6.6,
         'Base: 0.6 + Def×0.4\n+Incoming threats×1.5\n+Active attack urgency')
    ]
    
    x_left, x_right = 2.5, 7.5
    
    for action, y, formula in actions_left:
        create_box(ax, x_left, y, 2, 0.5, action, COLORS['process'], fontsize=9, bold=True)
        ax.text(x_left, y - 0.6, formula, ha='center', va='top', 
                fontsize=7, family='monospace',
                bbox=dict(boxstyle='round,pad=0.3', facecolor='white', edgecolor='gray'))
    
    for action, y, formula in actions_right:
        create_box(ax, x_right, y, 2, 0.5, action, COLORS['process'], fontsize=9, bold=True)
        ax.text(x_right, y - 0.6, formula, ha='center', va='top', 
                fontsize=7, family='monospace',
                bbox=dict(boxstyle='round,pad=0.3', facecolor='white', edgecolor='gray'))
    
    # Personality modifiers box
    create_box(ax, 5, 4, 5, 1.2, 
               'Personality Modifiers Applied:\n\n' +
               'Overextender (Agg×Greed>0.6): 0.7× weak cities, 1.2× resources\n' +
               'Fortress (Agg×Def>0.6): 1.3× strong cities, 1.2× defense\n' +
               'Turtle (Greed×Def>0.6): 1.2× factories, 0.8× distant targets',
               COLORS['modifier'], fontsize=9, bold=True)
    
    # Flow to next stage
    create_arrow(ax, 5, 3.4, 5, 2.6)
    create_box(ax, 5, 2, 4, 0.6, 
               '8 Base Utilities × Personality → Focus Boost', 
               COLORS['decision'], fontsize=11, bold=True)
    
    # Key insight
    ax.text(5, 0.5, 
            'Key: Each utility is calculated from pressures + personality traits, then modified by archetypes.',
            ha='center', fontsize=9, style='italic', 
            bbox=dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor=COLORS['process'], linewidth=2))
    
    pdf.savefig(fig, bbox_inches='tight')
    plt.close()

def page4_strategic_focus(pdf):
    """Page 4: Strategic Focus System"""
    fig, ax = plt.subplots(figsize=(11, 8.5))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 12)
    ax.axis('off')
    fig.patch.set_facecolor(COLORS['background'])
    
    # Title
    ax.text(5, 11.5, 'Stage 3 & 5: Strategic Focus System', 
            ha='center', fontsize=20, weight='bold', color=COLORS['text'])
    ax.text(5, 11, 'Commitment & Momentum • Prevents Flip-Flopping', 
            ha='center', fontsize=12, style='italic', color=COLORS['text'])
    
    # Stage 3: Apply boost
    create_box(ax, 2.5, 9.5, 3, 0.6, 
               'Stage 3: Apply Focus Boost', 
               COLORS['modifier'], fontsize=11, bold=True)
    
    ax.text(2.5, 8.7, 
            'IF CurrentStrategicFocus exists:\n' +
            '  Utility[Focus] *= (1.0 + FocusStrength×0.5)\n' +
            '  Max boost: +50% when fully committed',
            ha='center', fontsize=8, family='monospace',
            bbox=dict(boxstyle='round,pad=0.4', facecolor='white', edgecolor='gray'))
    
    # Stage 5: Update focus
    create_box(ax, 7.5, 9.5, 3, 0.6, 
               'Stage 5: Update Strategic Focus', 
               COLORS['decision'], fontsize=11, bold=True)
    
    ax.text(7.5, 8.7, 
            'After normalization:\n' +
            'IF Best action >35% (Agg) or >25% (Others):\n' +
            '  Set CurrentStrategicFocus\n' +
            '  FocusStrength = 0.7\n' +
            'ELSE:\n' +
            '  FocusStrength *= (1.0 - DecayRate)',
            ha='center', fontsize=8, family='monospace',
            bbox=dict(boxstyle='round,pad=0.4', facecolor='white', edgecolor='gray'))
    
    # Decision tree
    create_box(ax, 5, 6, 6, 0.6, 
               'Decision: Should we commit to this action?', 
               COLORS['decision'], fontsize=10, bold=True)
    
    # Yes branch
    create_arrow(ax, 3, 5.7, 2, 4.8)
    create_box(ax, 2, 4.5, 2.5, 0.5, 
               'YES: Winning Decisively', 
               COLORS['process'], fontsize=9)
    ax.text(2, 3.9, 
            '• >35% if Aggression high\n• >25% otherwise\n• Set focus strength 0.7',
            ha='center', fontsize=7)
    
    # No branch
    create_arrow(ax, 7, 5.7, 8, 4.8)
    create_box(ax, 8, 4.5, 2.5, 0.5, 
               'NO: Close Competition', 
               COLORS['modifier'], fontsize=9)
    ax.text(8, 3.9, 
            '• Decay existing focus\n• Rate: 15% (low AttSpan)\n        to 3% (high AttSpan)',
            ha='center', fontsize=7)
    
    # Special hold condition
    create_box(ax, 5, 2.5, 5, 0.8, 
               'Special Hold: City Offensive', 
               COLORS['action'], fontsize=10, bold=True)
    ax.text(5, 1.8, 
            'IF attacking city AND (Money > $2000 OR under attack ourselves):\n' +
            '  Don\'t abandon offensive (maintain focus)',
            ha='center', fontsize=8, family='monospace',
            bbox=dict(boxstyle='round,pad=0.4', facecolor='white', edgecolor='gray'))
    
    # Key insight
    ax.text(5, 0.5, 
            'Key: High AttentionSpan = stubborn (3% decay), Low AttentionSpan = distractible (15% decay).',
            ha='center', fontsize=9, style='italic', 
            bbox=dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor=COLORS['modifier'], linewidth=2))
    
    pdf.savefig(fig, bbox_inches='tight')
    plt.close()

def page5_city_assessment(pdf):
    """Page 5: City Assessment & Targeting"""
    fig, ax = plt.subplots(figsize=(11, 8.5))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 12)
    ax.axis('off')
    fig.patch.set_facecolor(COLORS['background'])
    
    # Title
    ax.text(5, 11.5, 'City Assessment & Smart Targeting', 
            ha='center', fontsize=20, weight='bold', color=COLORS['text'])
    ax.text(5, 11, 'Strategic Value = Weakness × Resources', 
            ha='center', fontsize=12, style='italic', color=COLORS['text'])
    
    # Assessment calculation flow
    y = 10
    create_box(ax, 5, y, 4, 0.5, 
               'For Each Enemy City:', 
               COLORS['input'], fontsize=11, bold=True)
    
    # Strength calculation
    y -= 1
    create_arrow(ax, 5, y + 0.75, 5, y + 0.25)
    create_box(ax, 5, y, 5, 0.5, 
               'Calculate Effective Strength', 
               COLORS['process'], fontsize=10, bold=True)
    
    ax.text(5, y - 0.5, 
            'EffectiveStrength = CityHP + TurretHP + TurretThreat + DefenderThreat\n' +
            '  TurretThreat = NumTurrets × 300 (DPS over 45 sec)\n' +
            '  DefenderThreat = NumDefenders × 200 (DPS over 30 sec)',
            ha='center', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round,pad=0.4', facecolor='white', edgecolor='gray'))
    
    # Vehicles needed
    y -= 1.5
    create_arrow(ax, 5, y + 0.75, 5, y + 0.25)
    create_box(ax, 5, y, 5, 0.5, 
               'Calculate Required Force', 
               COLORS['process'], fontsize=10, bold=True)
    
    ax.text(5, y - 0.5, 
            'VehiclesNeeded = EffectiveStrength / 150 (vehicle DPS × 22.5 sec)\n' +
            'ConfidenceRatio = VehiclesAvailable / VehiclesNeeded',
            ha='center', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round,pad=0.4', facecolor='white', edgecolor='gray'))
    
    # Resource counting
    y -= 1.5
    create_arrow(ax, 5, y + 0.75, 5, y + 0.25)
    create_box(ax, 5, y, 5, 0.5, 
               'Count Nearby Resources', 
               COLORS['process'], fontsize=10, bold=True)
    
    ax.text(5, y - 0.5, 
            'NearbyResources = Count within 3000 units of city',
            ha='center', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round,pad=0.4', facecolor='white', edgecolor='gray'))
    
    # Strategic value
    y -= 1.3
    create_arrow(ax, 5, y + 0.75, 5, y + 0.25)
    create_box(ax, 5, y, 5, 0.5, 
               'Calculate Strategic Value', 
               COLORS['decision'], fontsize=10, bold=True)
    
    ax.text(5, y - 0.6, 
            'WeaknessFactor = 1.0 / (1.0 + EffectiveStrength / 1000.0)\n' +
            'ResourceFactor = 1.0 + (NearbyResources × 0.2)\n' +
            'StrategicValue = WeaknessFactor × ResourceFactor',
            ha='center', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round,pad=0.4', facecolor='white', edgecolor='gray'))
    
    # Final scoring
    y -= 1.5
    create_arrow(ax, 5, y + 0.75, 5, y + 0.25)
    create_box(ax, 5, y, 5, 0.6, 
               'Select Best Target', 
               COLORS['action'], fontsize=11, bold=True)
    
    ax.text(5, y - 0.6, 
            'Score = (ConfidenceRatio × 0.7) + (StrategicValue × 0.3)\n\n' +
            'ConfidenceRatio scaling:\n' +
            '  <0.7: Score = 0 (too risky)\n' +
            '  0.7-1.0: Linear 0→1 (Aggressive AIs gamble here)\n' +
            '  1.0-1.5: Linear 1→1 (safe range)\n' +
            '  >1.5: Score = 1 (overkill)',
            ha='center', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round,pad=0.4', facecolor='white', edgecolor='gray'))
    
    # Key insight
    ax.text(5, 0.5, 
            'Key: Weak cities with many resources score higher. AI considers both winnable fights AND strategic payoff.',
            ha='center', fontsize=9, style='italic', 
            bbox=dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor=COLORS['decision'], linewidth=2))
    
    pdf.savefig(fig, bbox_inches='tight')
    plt.close()

def page6_execution(pdf):
    """Page 6: Execution & Stuck Vehicle Recovery"""
    fig, ax = plt.subplots(figsize=(11, 8.5))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 12)
    ax.axis('off')
    fig.patch.set_facecolor(COLORS['background'])
    
    # Title
    ax.text(5, 11.5, 'Stage 7 & 8: Execution & Recovery', 
            ha='center', fontsize=20, weight='bold', color=COLORS['text'])
    ax.text(5, 11, 'Action Execution • Stuck Vehicle Handling', 
            ha='center', fontsize=12, style='italic', color=COLORS['text'])
    
    # Execution (Stage 7)
    create_box(ax, 2.5, 9.5, 3, 0.6, 
               'Stage 7: Execute Actions', 
               COLORS['action'], fontsize=11, bold=True)
    
    actions_text = (
        'Based on normalized priorities:\n\n' +
        '1. Spawn buildings on cities\n' +
        '   • Vehicles, Turrets, Factories\n' +
        '   • Check money & limits\n\n' +
        '2. Assign vehicles to tasks\n' +
        '   • Capture resources\n' +
        '   • Attack cities/vehicles\n' +
        '   • Defend cities\n\n' +
        '3. Set bHasAssignment flag'
    )
    
    ax.text(2.5, 7.5, actions_text, ha='center', va='top', fontsize=8,
            bbox=dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor='gray'))
    
    # Stuck Detection (Stage 8)
    create_box(ax, 7.5, 9.5, 3, 0.6, 
               'Stage 8: Detect Stuck Vehicles', 
               COLORS['modifier'], fontsize=11, bold=True)
    
    detection_text = (
        'Position tracking between updates:\n\n' +
        'FOR each vehicle with assignment:\n' +
        '  SKIP if TargetResource ≠ null\n' +
        '    (actively capturing)\n' +
        '  SKIP if CurrentTarget valid\n' +
        '    (actively attacking)\n\n' +
        '  IF moved < 100 units:\n' +
        '    Mark as stuck'
    )
    
    ax.text(7.5, 7.5, detection_text, ha='center', va='top', fontsize=8, family='monospace',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor='gray'))
    
    # Recovery flow
    create_box(ax, 5, 5, 5, 0.6, 
               'Stuck Vehicle Recovery', 
               COLORS['action'], fontsize=11, bold=True)
    
    create_arrow(ax, 5, 4.7, 5, 4.1)
    
    create_box(ax, 5, 3.7, 3, 0.5, 
               'Find Nearest Friendly City', 
               COLORS['process'], fontsize=9)
    
    create_arrow(ax, 5, 3.45, 5, 2.95)
    
    create_box(ax, 5, 2.6, 4, 0.6, 
               'Calculate Opposite Side of City', 
               COLORS['process'], fontsize=9)
    
    ax.text(5, 2.0, 
            'ToCityDir = (City - Vehicle).Normalized\n' +
            'Distance = |City - Vehicle|\n' +
            'OppositePos = City + (ToCityDir × Distance)',
            ha='center', fontsize=7, family='monospace',
            bbox=dict(boxstyle='round,pad=0.4', facecolor='white', edgecolor='gray'))
    
    create_arrow(ax, 5, 1.6, 5, 1.1)
    
    create_box(ax, 5, 0.8, 3.5, 0.5, 
               'Teleport & Clear Assignment', 
               COLORS['action'], fontsize=9)
    
    # Key differences
    create_box(ax, 1.5, 0.3, 3, 0.5, 
               'OLD: Planet center ref\n→ Sent across world', 
               COLORS['modifier'], fontsize=7)
    
    create_box(ax, 8.5, 0.3, 3, 0.5, 
               'NEW: City center ref\n→ Stays in territory', 
               COLORS['process'], fontsize=7)
    
    create_arrow(ax, 3, 0.3, 7, 0.3, 'FIXED', COLORS['action'])
    
    pdf.savefig(fig, bbox_inches='tight')
    plt.close()

def page7_example(pdf):
    """Page 7: Example Scenario Walkthrough"""
    fig, ax = plt.subplots(figsize=(11, 8.5))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 12)
    ax.axis('off')
    fig.patch.set_facecolor(COLORS['background'])
    
    # Title
    ax.text(5, 11.5, 'Example Scenario: Aggressive AI Mid-Game', 
            ha='center', fontsize=20, weight='bold', color=COLORS['text'])
    
    # Scenario setup
    create_box(ax, 5, 10.5, 6, 0.6, 
               'Personality: Agg=0.8, Greed=0.6, Def=0.3, Attention=0.7', 
               COLORS['modifier'], fontsize=9, bold=True)
    
    ax.text(5, 9.9, 
            'Situation: 2 cities owned, 5 vehicles, $3500, enemy city nearby (weak, 3 resources)',
            ha='center', fontsize=9, style='italic')
    
    # Step-by-step
    steps = [
        (9.2, 'Pressures Gathered', COLORS['input'],
         'ProximalEnemyCities=1, Money=3500, VehiclesOwned=5\nProximalResources=2, OwnedResources=4, IncomingThreats=2'),
        
        (8.0, 'Utilities Calculated', COLORS['process'],
         'CaptureResources=0.85, CaptureCities=0.92 (high!)\nBuildFactories=0.78, DefendCities=0.45\nOther actions <0.5'),
        
        (6.8, 'Personality Applied', COLORS['modifier'],
         'Overextender detected (Agg×Greed=0.48<0.6)\nNo major modifiers, base utilities stand'),
        
        (5.6, 'Focus Boosted', COLORS['modifier'],
         'CurrentFocus=None (first decisive action)\nNo boost applied this round'),
        
        (4.4, 'Normalized', COLORS['decision'],
         'After normalization:\nCaptureCities=38%, CaptureResources=28%\nBuildFactories=25%, Others=9%'),
        
        (3.2, 'Focus Updated', COLORS['action'],
         'CaptureCities >35% (aggression threshold)\nSet CurrentStrategicFocus=CaptureCities\nFocusStrength=0.7'),
        
        (2.0, 'Vehicles Allocated', COLORS['action'],
         'CaptureCities gets 38% of 5 = 2 vehicles\nCaptureResources gets 28% of 5 = 1 vehicle\nBuildFactories executes (has money)'),
        
        (0.8, 'Next Update', COLORS['process'],
         'Next tick: CaptureCities gets +50% boost\nUtility: 0.92 × 1.35 = 1.24 (massive lead)\nCommitted to city attack until won or desperate')
    ]
    
    for y, title, color, text in steps:
        create_box(ax, 2, y, 2.5, 0.4, title, color, fontsize=9, bold=True)
        ax.text(6.5, y, text, ha='left', va='center', fontsize=7,
                bbox=dict(boxstyle='round,pad=0.3', facecolor='white', edgecolor='gray'))
        
        if y > 0.8:
            create_arrow(ax, 2, y - 0.2, 2, y - 0.8)
    
    pdf.savefig(fig, bbox_inches='tight')
    plt.close()

def page8_summary(pdf):
    """Page 8: System Summary & Key Features"""
    fig, ax = plt.subplots(figsize=(11, 8.5))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 12)
    ax.axis('off')
    fig.patch.set_facecolor(COLORS['background'])
    
    # Title
    ax.text(5, 11.5, 'AI System Summary', 
            ha='center', fontsize=24, weight='bold', color=COLORS['text'])
    ax.text(5, 11, 'Planet Conquest Decision-Making Intelligence', 
            ha='center', fontsize=14, style='italic', color=COLORS['text'])
    
    # Key features grid
    features = [
        ('Economic Reasoning', 2.5, 9.5, COLORS['process'],
         '• Factories prioritized over turrets\n• ROI-aware building (2.5min payback)\n• Income vs cost calculations\n• Smart resource saturation'),
        
        ('Strategic Commitment', 7.5, 9.5, COLORS['decision'],
         '• No flip-flopping between strategies\n• AttentionSpan-based persistence\n• Special holds for city offensives\n• Momentum system'),
        
        ('Smart Combat', 2.5, 7, COLORS['action'],
         '• City strength assessment\n• Confidence-based targeting\n• Overkill ratio (1.2-1.5×)\n• Won\'t suicide into strong cities'),
        
        ('Resource Intelligence', 7.5, 7, COLORS['input'],
         '• Strategic value calculation\n• Weakness × Resources scoring\n• 3000-unit proximity awareness\n• Enemy resource denial'),
        
        ('Threat Prediction', 2.5, 4.5, COLORS['modifier'],
         '• 5-second velocity lookahead\n• Incoming threat tracking\n• Preemptive defense (1.5× mult)\n• Active attack urgency'),
        
        ('Personality Archetypes', 7.5, 4.5, COLORS['modifier'],
         '• Overextender: Rushes weak targets\n• Fortress: Waits for strong force\n• Turtle: Builds economy first\n• Emergent behaviors')
    ]
    
    for title, x, y, color, text in features:
        create_box(ax, x, y, 4, 0.5, title, color, fontsize=10, bold=True)
        ax.text(x, y - 0.8, text, ha='center', va='top', fontsize=7,
                bbox=dict(boxstyle='round,pad=0.4', facecolor='white', edgecolor='gray'))
    
    # Bottom stats
    create_box(ax, 5, 2, 8, 1.2, 
               'System Statistics\n\n' +
               '18 Environmental Sensors • 8 Action Types • 4 Personality Traits\n' +
               '3 Personality Archetypes • 5-Second Update Cycle\n' +
               'Strategic Focus Boost: 0-50% • Commitment Thresholds: 25-35%\n' +
               'Stuck Vehicle Detection • Position Tracking • Smart Recovery',
               COLORS['input'], fontsize=9, bold=True)
    
    # Version info
    ax.text(5, 0.5, 
            'Generated for Planet Conquest AI System • February 2026',
            ha='center', fontsize=8, style='italic', color='gray')
    
    pdf.savefig(fig, bbox_inches='tight')
    plt.close()

def generate_pdf():
    """Main function to generate the complete PDF"""
    filename = 'AI_Decision_Tree.pdf'
    
    print(f"Generating AI Decision Tree PDF: {filename}")
    print("Creating visualizations...")
    
    with PdfPages(filename) as pdf:
        print("  Page 1/8: Overview...")
        page1_overview(pdf)
        
        print("  Page 2/8: Environmental Pressures...")
        page2_pressures(pdf)
        
        print("  Page 3/8: Utility Calculations...")
        page3_utilities(pdf)
        
        print("  Page 4/8: Strategic Focus...")
        page4_strategic_focus(pdf)
        
        print("  Page 5/8: City Assessment...")
        page5_city_assessment(pdf)
        
        print("  Page 6/8: Execution & Recovery...")
        page6_execution(pdf)
        
        print("  Page 7/8: Example Scenario...")
        page7_example(pdf)
        
        print("  Page 8/8: Summary...")
        page8_summary(pdf)
        
        # PDF metadata
        d = pdf.infodict()
        d['Title'] = 'Planet Conquest AI Decision Tree'
        d['Author'] = 'AI System Generator'
        d['Subject'] = 'Visual flowchart of AI decision-making pipeline'
        d['Keywords'] = 'AI, Decision Tree, Game AI, Strategy'
        d['CreationDate'] = None  # Will use current time
    
    print(f"\n✓ PDF generated successfully: {filename}")
    print(f"  8 pages created")
    print(f"  Contents:")
    print(f"    1. High-level pipeline overview")
    print(f"    2. Environmental pressures (18 sensors)")
    print(f"    3. Utility calculations (8 actions)")
    print(f"    4. Strategic focus system")
    print(f"    5. City assessment & targeting")
    print(f"    6. Execution & stuck vehicle recovery")
    print(f"    7. Example scenario walkthrough")
    print(f"    8. System summary")

if __name__ == '__main__':
    generate_pdf()
