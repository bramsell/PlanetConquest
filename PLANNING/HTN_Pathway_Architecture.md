# Hierarchical Task Network & Pathway Architecture
## AI Decision System Redesign - February 2026

---

## Core Concepts

### 1. **Desire-Driven Strategy Selection**
All AI behavior flows from three fundamental **desires** (not goals):
- **DOMINANCE** - Control territory, defeat enemies, project power
- **WEALTH** - Generate income, acquire resources, build economic capacity
- **SECURITY** - Protect assets, reduce threats, ensure survival

Desires map directly to **strategies** (methods to achieve desires). There is no intermediate "goals" layer.

These desires have **dependency relationships**: Dominance strategies require Wealth, Security can be pursued through Wealth OR Dominance strategies.

### 2. **Relationship-Aware Strategy Evaluation**
Every AI maintains relationships with every other AI:
- **Relationship value**: -1.0 (enemy) to +1.0 (ally), starts at 0.0 (neutral)
- **Threat calculation**: `EnemyStrength × -(Relationship - 1)`
  - Ally with 10 military: 10 × -(0.5 - 1) = 5 (low threat)
  - Enemy with 10 military: 10 × -(-0.8 - 1) = 18 (high threat!)
  - Neutral with 10 military: 10 × -(0.0 - 1) = 10 (moderate concern)

Strategies are evaluated **per target**, considering relationship, threat level, and situational factors.

### 3. **Derived Strategy Benefits (Not Hardcoded)**
Strategy effectiveness is **calculated from the environment**, not predetermined:

Example: "Capture Enemy Resources"
- If target has 10 resources, 0 turrets → High Wealth benefit (easy money)
- If target has 3 resources, 5 turrets → Negative Wealth benefit (attrition costs more than gain)
- If target is threatening (relationship -0.7) → High Security benefit
- If target is neutral (relationship 0.0) → Relationship penalty (making new enemy)

The AI derives value axiomatically:
```cpp
IncomeGain = ResourceCount × 100 × TimeHorizon
AttritionCost = EstimatedLosses × 1000
NetWealth = IncomeGain - AttritionCost
→ Only pursue if NetWealth > 0
```

### 4. **Hierarchical Task Network (HTN) / Prerequisite Resolution**
Strategies have **prerequisites**. When a strategy cannot be immediately executed, the AI recursively resolves what it needs first:
```
Want: Assault Enemy City (Dominance desire)
  ├─ Check: Have enough vehicles? NO
  │  └─ Prerequisite: Build Military (Wealth strategy)
  │     ├─ Check: Can afford vehicles fast enough? NO
  │     │  └─ Prerequisite: Increase Income (Wealth strategy)
  │     │     └─ Resolved: Capture Resources ✓
```

Maximum recursion depth: **2 levels** to prevent over-planning and maintain responsiveness.

### 5. **Time Horizon & Flow-Based Reasoning**
The AI thinks in **rates** (per minute) not just totals:
- **Production Rate**: How fast can I build vehicles? (income/min ÷ cost)
- **Consumption Rate**: How fast am I losing vehicles? (estimated attrition)
- **Net Rate**: Can I sustain this strategy? (production - consumption)

**Investment Analysis**: Should I buy what I want now, or invest in infrastructure for better long-term rates?

Example:
- Option A: Buy 1 vehicle/min with current $1000/min income
- Option B: Spend 1 minute to build factory → earn $2000/min → buy 2 vehicles/min
- Decision: If time horizon > 1 minute, Option B is superior

### 6. **Urgency Modifier**
Not all problems can wait for optimal solutions:
- **High Urgency** (under attack): Need vehicles NOW, skip factory investment
- **Low Urgency** (peaceful buildup): Invest in long-term infrastructure
- Urgency affects strategy selection within the same desire

### 7. **Multi-Desire Strategies**
Some strategies satisfy multiple desires simultaneously:
- **Capture Enemy Resources**: Wealth (income) + Dominance (expand) + Security (weaken threat)
- **Build Factory**: Wealth only
- Multi-desire strategies receive **combined weight** from all desires they serve

### 8. **Strategy Consolidation**
When multiple desires resolve to the same prerequisite strategy, their weights **merge**:
- Security wants defense → needs Wealth strategies (60%)
- Dominance wants assault → needs Wealth strategies (20%)
- Original Wealth desire (10%)
- **Consolidated Wealth strategies: 90%** - single-minded focus, compound motivation

---

## What We Already Have

### Current 5-Layer Architecture
```
Layer 1: PERSONALITY
  ├─ Aggression (0.0 - 1.0)
  ├─ Greed (0.0 - 1.0)
  ├─ Security (0.0 - 1.0)
  ├─ Patience (0.0 - 1.0)
  └─ RiskTolerance (0.0 - 1.0)

Layer 2: DESIRES (calculated from personality)
  ├─ Dominance = Aggression × weights
  ├─ Wealth = Greed × weights
  └─ Security = Security trait × weights

Layer 3: GOALS (11 strategic objectives)
  ├─ Conquer, Weaken, Dominate (from Dominance)
  ├─ ExpandIncome, BuildIncome, OptimizeIncome (from Wealth)
  ├─ Defend, Deter, Contain (from Security)
  └─ Survive, Prepare (from multiple)

Layer 4: STRATEGIES (13 methods)
  ├─ Assault, Raid, Siege, Harass
  ├─ BuildInfrastructure, ExpandTerritory, SaveResources
  ├─ Fortify, Patrol, Garrison
  └─ MassVehicles, MassTurrets, Reserve

Layer 5: ACTIONS (concrete execution)
  ├─ BuildVehicles, BuildTurrets, BuildFactories
  ├─ CaptureCities, CaptureResources
  └─ DefendCities, etc.
```

### Current Decision Flow
1. `GatherPressures()` - Assess situation (threats, resources, money, etc.)
2. `CalculateDesireWeights()` - Personality → Desires
3. `CalculateGoalWeights()` - Desires + Situation → Goals
4. `SelectStrategies()` - Goals → Strategies
5. `ScoreAllActions()` - Strategies boost action scores
6. `AllocateVehicles()` + `AllocateSpending()` - Execute top actions

### Current System Limitations
- **No relationships**: All enemies treated equally
- **Hardcoded strategy benefits**: Can't adapt to situation
- **No target-specific evaluation**: Doesn't consider which enemy to attack
- **Intermediate goals layer**: Adds unnecessary complexity
- **Binary strategy selection**: Strategies are "on" or "off" (>15% threshold)

---

## What We Want to Add

### 1. **Relationship System (NEW Layer 3)**
Track per-AI relationships and threat levels:

```cpp
struct FAIRelationship
{
    AAITeamController* OtherAI;
    
    // Relationship value: -1.0 = enemy, 0.0 = neutral, +1.0 = ally
    float RelationshipValue = 0.0f;
    
    // Derived threat level
    // Threat = EnemyStrength × -(Relationship - 1)
    // - Ally with 10 strength: 10 × -(0.5-1) = 5 (low threat)
    // - Enemy with 10 strength: 10 × -(-0.8-1) = 18 (high threat)
    float ThreatLevel = 0.0f;
    
    // Opportunity assessment (how vulnerable are they?)
    float Opportunity = 0.0f;
    
    // History tracking (for future relationship mechanics)
    int32 TimesAttackedMe = 0;
    int32 TimesIAttackedThem = 0;
    float LastInteractionTime = 0.0f;
    
    // Spatial awareness
    float DistanceToNearestCity = 0.0f;
    bool bSharesBorder = false;
};

TMap<AAITeamController*, FAIRelationship> Relationships;
```

**For now**: Relationships change only through actions (attack → worsen relationship)
**Future**: Complex relationship mechanics (decay, alliance negotiations, etc.)

### 2. **Remove Goals Layer - Direct Desire → Strategy Mapping**
Instead of: `Desires → Goals → Strategies`  
We have: `Desires → Strategies (evaluated per target)`

Strategies ARE the methods to achieve desires. No intermediate abstraction needed.

### 3. **Derived Strategy Benefits**
Replace hardcoded benefit percentages with situation-based calculations:

```cpp
// OLD: Hardcoded
StrategyBenefits[CaptureEnemyResources][Wealth] = 100%;

// NEW: Derived from environment
float EvaluateWealthBenefit(CaptureEnemyResources, TargetAI)
{
    int32 ResourceCount = TargetAI->CountResources();
    float DefenseStrength = AssessDefenses(TargetAI);
    float AttritionRate = CalculateAttrition(DefenseStrength);
    
    float IncomeGain = ResourceCount × 100 × TimeHorizon;
    float AttritionCost = AttritionRate × 1000 × MissionDuration;
    
    return (IncomeGain - AttritionCost) / 1000.0f;  // Normalized benefit
}
```

Strategy benefits are **computed** not **configured**.

### 4. **Target-Specific Strategy Evaluation**
Every strategy is evaluated against specific targets:

```cpp
for (AAITeamController* OtherAI : AllAIs)
{
    // Evaluate: Capture their resources
    float Benefit = EvaluateStrategy(CaptureResources, OtherAI);
    
    // Modify by relationship
    FAIRelationship& Rel = Relationships[OtherAI];
    if (Rel.RelationshipValue > 0.3f)  // Ally
        Benefit *= 0.1f;  // Huge penalty (don't attack friends)
    else if (Rel.RelationshipValue < -0.3f)  // Enemy
        Benefit *= 1.2f;  // Bonus (already hostile)
    else if (Rel.RelationshipValue < -0.3f)  // Enemy
        Benefit *= 1.2f;  // Bonus (already hostile)
    else  // Neutral
        Benefit *= 0.7f;  // Penalty (making new enemy has cost)
    
    // Threat-based bonus (Security desire)
    if (Rel.ThreatLevel > 0.7f)
        Benefit *= (1.0f + Rel.ThreatLevel);  // Fighting threats is valuable
}
```

### 5. **Recursive Prerequisite Resolution** (Max 2 Levels)
```cpp
Strategy.Check() → Blocked by prerequisite
  → Resolve(PrerequisiteStrategy)
     → Blocked by prerequisite
        → Resolve(Sub-PrerequisiteStrategy) 
           → Can execute ✓
```

Example chain:
```
Assault Enemy City (Dominance strategy)
  → Need 10 vehicles, have 2
     → Build Vehicles (Wealth strategy)
        → Need income $2400/min, have $800/min
           → Capture Resources (Wealth strategy) ✓
```

Weight flows: Assault 40% → Build Vehicles 40% → Capture Resources 40%

### 6. **Time Horizon Planning**
Each strategy has temporal profile:
- **One-Shot**: Single action (Assault a city once)
- **Sustained**: Continuous pressure (Siege requires ongoing vehicle replacement)
- **Periodic**: Repeated bursts (Harassment raids)
- **Permanent**: Indefinite commitment (Fortifications)

Expected duration determines:
- Whether investment is worthwhile (payback period)
- Required production rates (vehicles/min needed)
- Resource commitment (total cost over duration)

### 7. **Rate Calculations**
**Production Rate** = Income/min ÷ Unit cost
- $2000/min income ÷ $1000/vehicle = 2 vehicles/min

**Consumption Rate** = Estimated losses/min
- Attacking turret: enemy turret DPS × fire rate × time to destroy
- Defensive losses: enemy vehicle count × their DPS
- Derived from observable facts (turret count, vehicle count)

**Net Rate** = Production - Consumption
- Positive: Can sustain strategy
- Negative: Will run out, strategy fails

### 8. **Investment Analysis**
Compare options:
```
Option A: Direct Purchase
  - Cost: $1000
  - Benefit: 1 vehicle now
  - Rate: income/1000 vehicles/min

Option B: Invest in Factory
  - Cost: $1000  
  - Delay: 1 minute (time to afford at current income)
  - New income: +$1000/min
  - New rate: (income + 1000)/1000 vehicles/min
  - Payback: When benefit × duration > delay cost

Choose B if: Expected duration > Payback period
```

### 9. **Urgency System**
Calculated from situation:
- Under attack: 1.0 (maximum urgency)
- Nearby enemy vehicles: 0.5
- Low city health: 0.3
- Safe situation: 0.0

Effects:
- **High urgency**: Choose fast solutions (vehicles) over efficient ones (turrets/factories)
- **Low urgency**: Invest in long-term infrastructure
- Urgency threshold = acceptable wait time before action

---

## Revised Architecture: 7-Layer System

```
Layer 1: PERSONALITY
  ├─ Aggression, Greed, Security, Patience, RiskTolerance
  
Layer 2: DESIRES (from personality)
  ├─ Dominance (from Aggression)
  ├─ Wealth (from Greed)
  └─ Security (from Security trait)
  
Layer 3: RELATIONSHIPS (per-AI assessment) ← NEW
  ├─ For each AI:
  │   ├─ Relationship value (-1.0 to +1.0)
  │   ├─ Threat level = Strength × -(Relationship - 1)
  │   └─ Opportunity value (how vulnerable?)
  
Layer 4: SITUATION ASSESSMENT
  ├─ My resources, money, vehicles, income
  ├─ Each AI's resources, defenses, vehicles
  ├─ Environmental factors (urgency, threats)
  └─ Available targets and opportunities
  
Layer 5: STRATEGY EVALUATION (per target, per desire) ← CHANGED
  ├─ For each potential strategy:
  │   ├─ Calculate Dominance benefit (derived from situation)
  │   ├─ Calculate Wealth benefit (derived from situation)
  │   ├─ Calculate Security benefit (derived from situation)
  │   ├─ Apply relationship modifier
  │   ├─ Weigh by active desires
  │   └─ Total strategy value
  └─ Select highest-value strategy
  
Layer 6: PREREQUISITE RESOLUTION (HTN, max 2 levels)
  ├─ Can I execute this strategy?
  ├─ If NO → What prerequisite strategy do I need?
  ├─ Recurse with weight flow
  └─ Find achievable strategy
  
Layer 7: ACTIONS (execute selected strategy)
  ├─ BuildVehicles, BuildFactories
  ├─ CaptureResources(target)
  ├─ AssaultCity(target, city)
  └─ etc.
```

**Key Changes from Current System**:
1. Added **Relationships layer** (Layer 3)
2. Removed **Goals layer** entirely
3. **Strategies are target-specific** and benefits are **derived** (Layer 5)
4. Prerequisite resolution operates on strategies, not goals (Layer 6)

---
  - Wealth: None (minimal cost)
- **Temporal Pattern**: One-Shot per resource
- **Pathway Benefits**:
  - Dominance: 30% (expand territory)
  - Wealth: 100% (direct income gain)
  - Security: 20% (more resources = more options)

#### 2. CAPTURE ENEMY RESOURCES
- **When**: Enemy resources reachable, worth the risk
- **Prerequisites**:
  - Vehicles: 2-4 to fight for it
  - Wealth: Cost of vehicles
- **Temporal Pattern**: One-Shot but risky
- **Pathway Benefits**:
  - Dominance: 70% (deny enemy, expand)
  - Wealth: 100% (steal income)
  - Security: 50% (weaken enemy economy)

#### 3. BUILD FACTORIES
- **When**: Safe territory, have money OR high existing income
- **Prerequisites**:
  - Vehicles: 0 (no combat requirement)
  - Wealth: $1000 per factory
  - Safety: No immediate threats
  - Time horizon: > 1 minute (payback period)
- **Temporal Pattern**: Permanent (ongoing income)
- **Pathway Benefits**:
  - Dominance: 0% (no territorial impact)
  - Wealth: 100% (pure economic growth)
  - Security: 10% (economic resilience)

#### 4. OPTIMIZE INCOME (Future: trade, efficiency)
- **When**: Have stable economy, want to maximize
- **Prerequisites**: Diplomatic system (not yet implemented)
- **Temporal Pattern**: Permanent
- **Pathway Benefits**:
  - Dominance: 0%
  - Wealth: 100%
  - Security: 30% (economic partnerships)

---

### SECURITY PATHWAY
**Objective**: Protect assets, reduce threats, ensure survival

**Methods**:

#### 1. DIRECT DEFENSE - Emergency Response
- **When**: Urgency > 0.5 (under attack or imminent threat)
- **Sub-methods**:
  - **Mass Vehicles** (Urgency > 0.8)
    - Fast to deploy
    - Mobile, can respond anywhere
    - More expensive per damage output
  - **Fortify / Mixed** (Urgency 0.3-0.8)
    - Balance of speed and efficiency
    - Some vehicles for immediate response
    - Some turrets for permanent defense
  - **Mass Turrets** (Urgency < 0.3)
    - Slow to build
    - Permanent, cost-efficient
    - Immobile (only defends fixed locations)
- **Prerequisites**:
  - Wealth: Money for defenders
  - Time: Function of urgency (less time = more expensive method)
- **Temporal Pattern**: Permanent (defenders stay)
- **Pathway Benefits**:
  - Dominance: 10% (defensive posture, not aggressive)
  - Wealth: 0% (pure cost, no income)
  - Security: 100% (direct protection)

#### 2. WEAKEN NEARBY THREATS - Proactive Defense
- **When**: Identified threat exists but not yet attacking
- **Means**: Use Dominance pathway methods (Assault, Weaken)
- **Prerequisites**:
  - Vehicles: Enough to attack
  - Wealth: Fund offensive operation
  - (This is essentially routing through DOMINANCE pathway)
- **Temporal Pattern**: One-Shot or Sustained
- **Pathway Benefits**:
  - Dominance: 60% (offensive action)
  - Wealth: 30% (might capture resources)
  - Security: 100% (eliminate threat before it strikes)

#### 3. GARRISON / PATROL (Maintain readiness)
- **When**: No immediate threat but want deterrence
- **Prerequisites**:
  - Vehicles: Small standing force
  - Wealth: Maintenance costs
- **Temporal Pattern**: Permanent (ongoing)
- **Pathway Benefits**:
  - Dominance: 20% (show of strength)
  - Wealth: -10% (ongoing cost)
  - Security: 80% (deterrence, early warning)

#### 4. ALLY / BRIBE (Future: diplomatic solutions)
- **When**: Can't defend militarily
- **Prerequisites**: Diplomatic system, relationship values
- **Temporal Pattern**: Permanent (relationships persist)
- **Pathway Benefits**:
  - Dominance: -20% (shows weakness)
  - Wealth: -30% (payments required)
  - Security: 90% (non-military protection)

---

## Strategy Cross-Reference Table

| Strategy | Dominance Benefit | Wealth Benefit | Security Benefit | Prerequisites | Pattern |
|----------|-------------------|----------------|------------------|---------------|---------|
| **Assault** | 100% | 30% | 40% | Vehicles + Wealth | One-Shot |
| **Cripple Defense** | 80% | 10% | 60% | Vehicles + Wealth | Sustained |
| **Cripple Economy** | 70% | 100% | 50% | Vehicles + Wealth | Sustained |
| **Siege** | 90% | 20% | 30% | Vehicles + Wealth | Sustained |
| **Raid/Harass** | 50% | 40% | 20% | Few Vehicles | Periodic |
| **Capture Neutral Resources** | 30% | 100% | 20% | 1 Vehicle | One-Shot |
| **Capture Enemy Resources** | 70% | 100% | 50% | Few Vehicles | One-Shot |
| **Build Factories** | 0% | 100% | 10% | Wealth + Safety | Permanent |
| **Mass Vehicles (Defense)** | 10% | 0% | 100% | Wealth + High Urgency | Permanent |
| **Mass Turrets (Defense)** | 10% | 0% | 100% | Wealth + Low Urgency | Permanent |
| **Fortify (Mixed)** | 10% | 0% | 100% | Wealth + Medium Urgency | Permanent |
| **Weaken Threats (Proactive)** | 60% | 30% | 100% | Vehicles + Wealth | Variable |
| **Garrison/Patrol** | 20% | -10% | 80% | Vehicles + Wealth | Permanent |

---

## Strategy Dependency & Flow Diagram

```
┌─────────────────────────────────────────────────┐
│ PERSONALITY (Layer 1)                           │
│  - Aggression, Greed, Security, Patience, Risk  │
└──────────────────┬──────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────┐
│ DESIRES (Layer 2)                               │
│  - Dominance (from Aggression)                  │
│  - Wealth (from Greed)                          │
│  - Security (from Security trait)               │
└──┬──────────────┬──────────────────┬────────────┘
   │              │                  │
   │              │                  │
   ▼              ▼                  ▼
┌──────────┐  ┌──────────┐      ┌──────────┐
│DOMINANCE │  │  WEALTH  │      │ SECURITY │
│(30%) want│  │(50%) want│      │(20%) want│
│conquer   │  │ economy  │      │  safety  │
└────┬─────┘  └────┬─────┘      └────┬─────┘
     │             │                  │
     │             │  ┌───────────────┘
     │             │  │ Security can use:
     │             │  │ - Wealth strategies OR
     │             │  │ - Dominance strategies
     │             │  │   (weaken threats)
     ▼             ▼  ▼
┌────────────────────────────────────────┐
│ RELATIONSHIPS (Layer 3) - NEW!         │
│ For each AI:                           │
│  - Relationship: -1.0 to +1.0         │
│  - Threat = Strength × -(Rel - 1)     │
│  - Opportunity (vulnerability)         │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│ STRATEGY EVALUATION (Layer 5)          │
│ Per-target, derived benefits:          │
│                                        │
│ Against Red AI (enemy, high threat):   │
│  - CaptureResources: High Security    │
│  - AssaultCity: High Dominance        │
│                                        │
│ Against Green AI (neutral, weak):      │
│  - CaptureResources: High Wealth      │
│  - Relationship penalty applies        │
│                                        │
│ No target (internal):                  │
│  - BuildFactory: Pure Wealth          │
│  - MassVehicles: Pure Security        │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│ PREREQUISITE CHECK (Layer 6)           │
│ Can execute strategy?                  │
│  - Assault Red → Need 10 vehicles     │
│    Have 2 → BLOCKED                   │
│    Prerequisite: Build vehicles       │
│      → Need income → BLOCKED          │
│        Prerequisite: Capture resources│
│          → Can do! ✓                  │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│ WEIGHT CONSOLIDATION                   │
│ Assault(30%) → BuildVehicles(30%)     │
│ Defense(20%) → BuildVehicles(20%)     │  
│ Wealth(50%)  → CaptureResources(50%)  │
│                                        │
│ Final: BuildVehicles 50%              │
│        CaptureResources 50%           │
└──────────────┬─────────────────────────┘
               │
               ▼
┌────────────────────────────────────────┐
│ ACTIONS (Layer 7)                      │
│ - Build vehicles                       │
│ - Capture resources                    │
└────────────────────────────────────────┘
```

**Key Insights**:
1. **No Goals layer** - Desires map directly to Strategies
2. **Relationships inform strategy value** - Same strategy has different value against different targets
3. **All paths flow through Wealth** - Economic foundation for Dominance and Security
4. **Weight consolidation** - Multiple motivations create single-minded focus

---

## Comprehensive Strategy Catalog

Strategies are evaluated **per target** with **derived benefits** from the environment.

### Category: DOMINANCE Strategies (Territory & Conquest)

#### ASSAULT_CITY
**Target**: Specific enemy city  
**Temporal Pattern**: One-Shot  
**Prerequisites**:
- Vehicles: Enough to overcome defenses (situation-dependent)
- Wealth: Money to build attack force

**Benefit Calculation**:
```cpp
DominanceBenefit = 1.0 (always maximum - primary purpose)

WealthBenefit = (City income value) / 100.0
  // Capturing cities gives income

SecurityBenefit = Target.ThreatLevel / 2.0
  // Eliminating threatening enemies improves security
  
RelationshipModifier:
  If Target.Relationship > 0.3: × 0.1 (don't attack allies)
  If Target.Relationship < -0.3: × 1.2 (already enemies)
  Else: × 0.7 (making new enemies has cost)
```

#### CAPTURE_ENEMY_RESOURCES
**Target**: Specific enemy AI (captures their resources)  
**Temporal Pattern**: One-Shot per resource  
**Prerequisites**:
- Vehicles: 1-3 depending on defense strength
- Can reach target resources

**Benefit Calculation**:
```cpp
// Calculate expected gain
ResourceCount = Target.GetResourceCount()
IncomeGain = ResourceCount × 100 × TimeHorizon

// Calculate expected cost (attrition)
DefenseStrength = AssessDefenses(Target location)
AttritionRate = EstimateVehicleLosses(DefenseStrength)
AttritionCost = AttritionRate × 1000 × MissionDuration

// Net wealth benefit (can be negative!)
WealthBenefit = (IncomeGain - AttritionCost) / 1000.0

// Dominance benefit
DominanceBenefit = 0.7 (territorial expansion)

// Security benefit (weakening threat)
SecurityBenefit = Target.ThreatLevel × 0.5

// Only pursue if net positive
If WealthBenefit < 0: TotalValue = 0
```

**Real Example**:
```
Target AI "Red" (enemy, heavily defended):
  - Resources: 10
  - Turrets near resources: 5  
  - Relationship: -0.7 (enemy)
  - Threat: 15.0 (high)
  
Calculation:
  Income gain: 10 × 100 × 5min = 5000
  Defense DPS: 5 turrets × 20 DPS = 100 damage/sec
  Expected losses: ~15 vehicles over mission
  Attrition cost: 15 × 1000 = 15000
  Net wealth: 5000 - 15000 = -10000 (NEGATIVE!)
  
Result: Don't pursue (too costly)

Target AI "Green" (neutral, undefended):
  - Resources: 3
  - Turrets near resources: 0
  - Relationship: 0.0 (neutral)
  - Threat: 5.0 (low)
  
Calculation:
  Income gain: 3 × 100 × 5min = 1500
  Defense DPS: 0
  Expected losses: 0 vehicles
  Attrition cost: 0
  Net wealth: 1500 (POSITIVE!)
  Relationship penalty: × 0.7 (neutral → enemy)
  Adjusted wealth: 1050
  
Result: Good strategy!
```

#### DESTROY_ENEMY_FACTORIES
**Target**: Specific enemy AI  
**Temporal Pattern**: One-Shot  
**Prerequisites**:
- Vehicles: Enough to reach and destroy factories
- Enemy has factories to destroy

**Benefit Calculation**:
```cpp
FactoryCount = Target.GetFactoryCount()
If FactoryCount == 0: Return 0 (can't destroy what doesn't exist)

// Dominance: Weakening enemy
DominanceBenefit = 0.7

// Wealth: NO BENEFIT (destroyed, not captured)
WealthBenefit = 0.0

// Security: Reduces enemy production capacity
FutureThreatReduction = FactoryCount × 1000 (income/min they lose)
SecurityBenefit = (FutureThreatReduction / 2000.0) × Target.ThreatLevel

// Only valuable against threatening enemies with factories
```

#### SIEGE_CITY
**Target**: Specific enemy city  
**Temporal Pattern**: Sustained (attrition warfare)  
**Prerequisites**:
- Vehicles: Continuous production rate ≥ attrition rate
- Wealth: Income to sustain losses

**Benefit Calculation**:
```cpp
// Production rate check
ProductionRate = MyIncome / 1000.0  // vehicles/min
ConsumptionRate = EstimateSiegeLosses(Target.City)  // vehicles/min

If ProductionRate < ConsumptionRate:
  Return 0  // Can't sustain siege

DominanceBenefit = 0.9 (aggressive territorial pressure)
WealthBenefit = 0.2 (eventual capture gives income)
SecurityBenefit = Target.ThreatLevel × 0.3 (weakening enemy over time)

// Time cost: Siege takes long time
TimeHorizon = 10+ minutes typically
```

#### RAID_HARASS
**Target**: Specific enemy AI (hit and run)  
**Temporal Pattern**: Periodic  
**Prerequisites**:
- Vehicles: 1-3 (small mobile force)
- Target has vulnerable assets (resources, weak cities)

**Benefit Calculation**:
```cpp
DominanceBenefit = 0.5 (minor pressure)
WealthBenefit = 0.3 (might opportunistically capture resources)
SecurityBenefit = 0.2 (keeps enemy reactive/defensive)

// Low commitment, low reward
```

---

### Category: WEALTH Strategies (Economic Growth)

#### CAPTURE_NEUTRAL_RESOURCES
**Target**: None (any available neutral resources)  
**Temporal Pattern**: One-Shot per resource  
**Prerequisites**:
- Vehicles: 1 per resource
- Resources exist

**Benefit Calculation**:
```cpp
AvailableResources = CountNeutralResources()
If AvailableResources == 0: Return 0

IncomeGain = AvailableResources × 100 × TimeHorizon

DominanceBenefit = 0.3 (minor territorial expansion)
WealthBenefit = IncomeGain / 1000.0
SecurityBenefit = 0.2 (more income = more options)

// Simple, safe, always good if resources available
```

#### BUILD_FACTORY
**Target**: None (build at own city)  
**Temporal Pattern**: Permanent  
**Prerequisites**:
- Money: $1000
- Safe territory (urgency < 0.5)
- Time horizon > payback period (~1 minute)

**Benefit Calculation**:
```cpp
// Investment analysis
TimeToAfford = 1000 / MyIncome  // minutes
PaybackPeriod = TimeToAfford
FutureIncomeGain = 1000 × TimeHorizon

If TimeHorizon < PaybackPeriod: Return 0 (not worth it)
If Urgency > 0.5: Return 0 (need units now, not later)

WealthBenefit = 1.0 (pure economic strategy)
DominanceBenefit = 0.0 (no territorial impact)
SecurityBenefit = 0.1 (economic resilience)
```

---

### Category: SECURITY Strategies (Defense & Protection)

#### MASS_VEHICLES_DEFENSE
**Target**: None (defensive vehicles around own cities)  
**Temporal Pattern**: Permanent (until threat gone)  
**Prerequisites**:
- Money: Variable (depends on threat)
- Urgency: > 0.5 (emergency response)

**Benefit Calculation**:
```cpp
ThreatLevel = CalculateIncomingThreat()
VehiclesNeeded = ThreatLevel / 20.0  // Rough estimate

// Emergency defense - fast but expensive
DominanceBenefit = 0.1 (defensive posture)
WealthBenefit = 0.0 (pure cost)
SecurityBenefit = 1.0 × Urgency

// High urgency makes this very valuable
If Urgency > 0.8:
  SecurityBenefit = 1.5  // Maximum priority
```

#### MASS_TURRETS_DEFENSE
**Target**: None (turrets at own cities)  
**Temporal Pattern**: Permanent  
**Prerequisites**:
- Money: $1000 per turret
- Urgency: < 0.5 (long-term investment)
- Time to build: ~1 minute per turret

**Benefit Calculation**:
```cpp
ThreatLevel = CalculateIncomingThreat()
TurretsNeeded = ThreatLevel / 30.0  // Turrets stronger than vehicles

// Long-term defense - slow but efficient
DominanceBenefit = 0.1
WealthBenefit = -0.1 (cost, but cheaper per DPS than vehicles)
SecurityBenefit = 1.0

// Low urgency makes this preferred (more efficient)
If Urgency < 0.3:
  SecurityBenefit = 1.3  // Bonus for efficiency
```

#### FORTIFY_MIXED
**Target**: None (mix of vehicles and turrets)  
**Temporal Pattern**: Permanent  
**Prerequisites**:
- Money: Variable
- Urgency: 0.3 - 0.8 (moderate threat)

**Benefit Calculation**:
```cpp
// Balanced approach
DominanceBenefit = 0.1
WealthBenefit = -0.05
SecurityBenefit = 1.0

// Good middle ground for moderate urgency
```

#### WEAKEN_THREATENING_NEIGHBOR
**Target**: Specific AI with high threat level  
**Temporal Pattern**: Sustained or One-Shot  
**Prerequisites**:
- Target.ThreatLevel > 0.7
- Vehicles: Enough to engage

**Benefit Calculation**:
```cpp
// Proactive defense: attack before they attack you
DominanceBenefit = 0.6 (offensive action)
WealthBenefit = 0.2 (might capture their resources)
SecurityBenefit = Target.ThreatLevel × 1.5  // High value against threats!

// This makes fighting threats highly valuable
// Essentially routes Security desire → Dominance strategy
```

#### GARRISON_PATROL
**Target**: None (maintain readiness)  
**Temporal Pattern**: Permanent  
**Prerequisites**:
- Vehicles: Small standing force
- Money: Maintenance costs

**Benefit Calculation**:
```cpp
DominanceBenefit = 0.2 (show of strength)
WealthBenefit = -0.1 (ongoing cost)
SecurityBenefit = 0.8 (deterrence, early warning)

// Good for moderate Security desire without immediate threats
```

---

## Strategy Cross-Reference Table

| Strategy | Primary Desire | Requires Target | Benefit Type | Urgency Sensitive | Key Prerequisites |
|----------|---------------|-----------------|--------------|-------------------|-------------------|
| **Assault City** | Dominance | Yes (enemy AI) | Derived | No | Vehicles, wealth, target city |
| **Capture Enemy Resources** | Wealth/Dominance | Yes (enemy AI) | Derived | No | Vehicles, target resources |
| **Destroy Enemy Factories** | Dominance/Security | Yes (enemy AI) | Derived | No | Vehicles, enemy has factories |
| **Siege City** | Dominance | Yes (enemy AI) | Derived | No | Production rate ≥ consumption |
| **Raid/Harass** | Dominance | Yes (enemy AI) | Fixed | No | Few vehicles |
| **Capture Neutral Resources** | Wealth | No | Derived | No | Resources exist |
| **Build Factory** | Wealth | No | Fixed | Yes (urgency < 0.5) | Money, time horizon, safety |
| **Mass Vehicles (Defense)** | Security | No | Fixed | Yes (urgency > 0.5) | Money |
| **Mass Turrets (Defense)** | Security | No | Fixed | Yes (urgency < 0.5) | Money, time |
| **Fortify (Mixed)** | Security | No | Fixed | Yes (0.3-0.8) | Money |
| **Weaken Threat** | Security | Yes (threatening AI) | Derived++ | No | High threat level |
| **Garrison/Patrol** | Security | No | Fixed | No | Vehicles |

**Benefit Types**:
- **Derived**: Calculated from environment (can be 0 or even negative!)
- **Fixed**: Predetermined values
- **Derived++**: Derived AND highly sensitive to relationships/threats

---

## Temporal Patterns & Rate Requirements

### One-Shot Actions
- **Definition**: Single action, no ongoing commitment
- **Examples**: Assault city, capture resource
- **Rate requirement**: 0/min (just need total vehicles)
- **Investment consideration**: Low (won't benefit from long-term income)

### Sustained Actions
- **Definition**: Continuous pressure over time
- **Examples**: Siege, cripple enemy economy
- **Rate requirement**: Production ≥ Consumption
- **Investment consideration**: High (factory payoff if duration > 2min)

### Periodic Actions
- **Definition**: Repeated bursts with gaps
- **Examples**: Harassment raids
- **Rate requirement**: Rebuild between raids
- **Investment consideration**: Medium

### Permanent Commitments
- **Definition**: Indefinite ongoing presence
- **Examples**: Fortifications, garrison, factories
- **Rate requirement**: Initial cost only (no consumption)
- **Investment consideration**: Very High (infinite duration = always worth it)

---

## Investment Decision Framework

### The Core Question
For any goal requiring resources: Should I buy what I need now, or invest in production capacity first?

### The Math

**Current State**:
- Income: I/min
- Unit cost: C
- Production rate: I/C units/min

**After Factory Investment** ($1000 cost, $1000/min income):
- Time to afford factory: 1000/I minutes
- New income: (I + 1000)/min
- New production rate: (I + 1000)/C units/min
- Rate improvement: 1000/C units/min

**Breakeven Analysis**:
```
Benefit of factory = (Rate improvement) × (Duration)
Cost of factory = Delay to afford factory

Choose factory if:
  (1000/C) × Duration > (1000/I)
  
Simplified:
  Duration > C/I
  
If unit cost = $1000 and income = $1000/min:
  Duration > 1 minute → Factory worth it
```

### Urgency Modifier
Available time before action needed:
```
Urgency = 0.0 → Time available: Infinite (invest freely)
Urgency = 0.5 → Time available: ~5 minutes
Urgency = 1.0 → Time available: 0 minutes (emergency)

Factory viable if:
  Time to afford factory < Time available
```

---

## Consumption Rate Estimation

### Attacking Cities (Turret Fire)
```
Consumption Rate = Σ(Enemy Turrets) × (Turret DPS) / (Vehicle Health)

Example:
  Enemy turrets: 3
  Turret damage: 30
  Turret fire rate: 0.67/sec
  Turret DPS: 30 × 0.67 = 20 damage/sec
  Total DPS: 3 × 20 = 60 damage/sec
  Vehicle health: 100
  Vehicles lost per second: 60/100 = 0.6 vehicles/sec
  Vehicles lost per minute: 0.6 × 60 = 36 vehicles/min
  
Production needed: 36+ vehicles/min to sustain assault
(Likely too high → siege/cripple defense instead)
```

### Attacking Enemy Vehicles
```
Consumption Rate = (Enemy vehicles) × (Enemy DPS) / (Own vehicle health)

Example:
  Enemy vehicles: 5
  Enemy vehicle damage: 20
  Enemy fire rate: 0.33/sec
  Enemy DPS: 20 × 0.33 = 6.67 damage/sec each
  Total enemy DPS: 5 × 6.67 = 33.3 damage/sec
  Own vehicle health: 100
  Vehicles lost per minute: (33.3 / 100) × 60 = 20 vehicles/min
  
If own forces = 8 vehicles with 20 DPS each:
  Own DPS: 8 × 20 × 0.33 = 53 damage/sec
  Time to kill all enemies: (5 × 100) / 53 = 9.4 seconds
  Own losses in that time: (33.3 × 9.4) / 100 = 3.1 vehicles
  
Net cost: 3 vehicles for clearing the area
Not sustained, so consumption rate = 0 after battle
```

### Defensive Attrition (Being Attacked)
```
Incoming threat DPS = (Enemy vehicles) × (Their DPS)
Defensive DPS = (Own turrets + vehicles) × (DPS)

If defensive DPS > threat DPS → Will win
If threat DPS > defensive DPS → Need reinforcements

Reinforcement rate needed:
  = (Threat DPS - Defense DPS) / (Vehicle health) 
  = vehicles/min to maintain parity
```

---

## Example Decision Flows

### Example 1: Greedy AI with Multiple Neighbors (Relationship-Aware)

**Setup**:
- **My AI**: Personality: Aggression 0.3, Greed 0.8, Security 0.4
  - Desires: Dominance 30%, Wealth 50%, Security 20%
  - Resources: $800, income $1200/min
  - Military: 2 vehicles, 0 turrets
  - Territory: 1 city, 0 captured resources

- **Neighbor "Red"** (Enemy):
  - Relationship: -0.7 (established enemy)
  - Military: 6 turrets, 3 vehicles (threat level: 15.0)
  - Resources: 10 captured resources ($1000/min income)
  - Territory: 1 city, heavily defended
  
- **Neighbor "Green"** (Neutral):
  - Relationship: 0.0 (never interacted)
  - Military: 0 turrets, 1 vehicle (threat level: 1.0)
  - Resources: 3 captured resources ($300/min income)
  - Territory: 1 city, minimally defended
  
- **Neutral Resources**: 2 uncaptured resources available

**Strategy Evaluation (with Relationships)**:

**DOMINANCE (30%)**:

*Option 1: Assault Red City*
```
Prerequisites:
  - Need ~18 vehicles to overcome 6 turrets
  - Have: 2 vehicles → BLOCKED (need 16 more)
  - Cost to build 16: $16,000
  - Time to afford: 13.3 minutes at $1200/min
  
Benefit IF we could:
  - Dominance: 1.0 (maximum)
  - Wealth: 10 resources × 100 = 1000/min income → 0.5 benefit
  - Security: 15.0 threat × 0.5 = 7.5 benefit
  - Relationship modifier: × 1.2 (already enemies, no cost)
  
Total theoretical value: (1.0 × 0.3) + (0.5 × 0.5) + (7.5 × 0.2) = 1.85 (HIGH!)

But BLOCKED by prerequisites → DEFER
```

*Option 2: Assault Green City*
```
Prerequisites:
  - Need ~2 vehicles (minimal defense)
  - Have: 2 vehicles ✓
  - Can execute immediately!
  
Benefits:
  - Dominance: 1.0 (maximum)
  - Wealth: 3 resources × 100 = 300/min → 0.15 benefit
  - Security: 1.0 threat × 0.5 = 0.5 benefit
  - Relationship modifier: × 0.7 (making new enemy has cost)
  
Total value: [(1.0 × 0.3) + (0.15 × 0.5) + (0.5 × 0.2)] × 0.7 = 0.298

Can do now, but relationship penalty hurts!
```

*Option 3: Capture Red's Resources*
```
Prerequisites:
  - Red has 10 resources
  - Defense assessment: 3 turrets near resources
  - Need ~9 vehicles to overcome turrets
  - Have: 2 → BLOCKED
  
Benefit IF we could:
  - Dominance: 0.7 (territorial)
  - Wealth: Income gain vs attrition
    - Gain: 10 × 100 × 5min horizon = 5000
    - Attrition: 3 turrets × 20 DPS → ~12 vehicle losses
    - Cost: 12 × 1000 = 12000
    - NET: 5000 - 12000 = -7000 (NEGATIVE!)
  - Wealth benefit: -7.0 (terrible investment)
  - Security: 15.0 threat × 0.5 = 7.5 benefit
  
Total: (0.7 × 0.3) + (-7.0 × 0.5) + (7.5 × 0.2) = -1.79 (DON'T DO!)

Even if not blocked, negative wealth makes this bad!
```

*Option 4: Capture Green's Resources*
```
Prerequisites:
  - Green has 3 resources
  - Defense: 0 turrets nearby
  - Need: 3 vehicles (1 per resource)
  - Have: 2 → Need 1 more vehicle ($1000)
  
Benefits:
  - Dominance: 0.7 (territorial)
  - Wealth: Income gain vs attrition
    - Gain: 3 × 100 × 5min = 1500
    - Attrition: 0 turrets → 0 losses
    - NET: +1500
  - Wealth benefit: 1.5
  - Security: 1.0 threat × 0.5 = 0.5
  - Relationship modifier: × 0.7 (neutral → enemy)
  
Total: [(0.7 × 0.3) + (1.5 × 0.5) + (0.5 × 0.2)] × 0.7 = 0.679

Good value! But need 1 more vehicle first
```

**WEALTH (50%)**:

*Option 5: Capture Neutral Resources*
```
Prerequisites:
  - 2 neutral resources available
  - Need: 2 vehicles ✓ (have exactly 2!)
  - Defense: 0
  
Benefits:
  - Dominance: 0.3 (minor territorial)
  - Wealth: 2 × 100 × 5min = 1000 → 1.0 benefit
  - Security: 0.2 (more economy = more options)
  - Relationship modifier: 1.0 (no relationship cost)
  
Total: (0.3 × 0.3) + (1.0 × 0.5) + (0.2 × 0.2) = 0.63

No relationship penalty! Safer choice than attacking Green
```

*Option 6: Build Factory*
```
Prerequisites:
  - Need: $1000
  - Have: $800
  - Time to afford: 10 seconds ✓
  - Safety check: No immediate attacks ✓
  - Time horizon: 5+ minutes ✓
  
Benefits:
  - Dominance: 0.0 (no territorial effect)
  - Wealth: 1.0 (pure economic)
  - Security: 0.1 (economic resilience)
  
Total: (0.0 × 0.3) + (1.0 × 0.5) + (0.1 × 0.2) = 0.52

Lower value than resources, but permanent income boost
```

**SECURITY (20%)**:

*Option 7: Weaken Red (Proactive Defense)*
```
Prerequisites:
  - Red threat: 15.0 (high!)
  - Would need combat vehicles
  - Currently blocked by prerequisites
  
Benefits (if could):
  - Dominance: 0.6 (offensive)
  - Wealth: 0.2 (might gain resources)
  - Security: 15.0 threat × 1.5 = 22.5 benefit (!!)
  
BLOCKED, flows to prerequisites → Need Wealth first
```

**FINAL RANKING** (accounting for what's possible NOW):
```
1. Capture Neutral Resources: 0.63
   - Can do immediately with current 2 vehicles
   - No relationship cost
   - Adds +$200/min income
   
2. Build Factory: 0.52
   - Can afford in 10 seconds
   - Adds +$1000/min income (long-term better)
   - But resources are one-time capture opportunity
   
3. Capture Green Resources: 0.679 (BEST value!)
   - BUT requires 1 more vehicle first
   - Relationship cost hurts
   - Neutral resources are safer immediate option

Decision: Capture neutral resources NOW, then reassess
```

**WHY NOT ATTACK GREEN?**
Despite Green being weak:
- Relationship penalty (× 0.7) reduces total value
- Need to build 1 more vehicle first (delay)
- Neutral resources are available NOW with no penalty
- Greedy AI (Wealth 50%) prefers safe economic gain over aggression

**EMERGENT BEHAVIOR**: 
- AI avoids creating new enemies when neutral options exist
- Target selection based on opportunity + relationship cost
- Economic calculation shows Red's resources are a TRAP (high attrition)
- Personality (Greed 0.8) naturally defers combat for safer wealth

**FINAL DECISION**:
1. Send 2 vehicles to capture neutral resources (+$200/min)
2. Income now $1400/min
3. In 43 seconds, afford factory (+$1000/min)
4. Income becomes $2400/min
5. Re-evaluate: Can now afford vehicles to attack Green or continue building
6. Red remains "too expensive" until overwhelming force available

---

### Example 2: Defensive AI Under Attack (Relationship Dynamics)

**Setup**:
- **My AI**: Personality: Aggression 0.2, Greed 0.3, Security 0.9
  - Desires: Dominance 10%, Wealth 20%, Security 70%
  - Resources: $600, income $1800/min
  - Military: 2 turrets, 3 vehicles
  - Territory: 1 city, 2 captured resources
  
- **Attacker "Red"**:
  - Relationship: -0.9 (bitter enemy, has attacked multiple times)
  - Incoming force: 8 vehicles (DPS: 53.4)
  - Distance to my city: 2000 units (~4 seconds at 500 speed)
  - Threat level: 18.0 (very high!)
  - Known resources: 8 resources (estimated $800/min)
  
- **Neighbor "Blue"** (Ally):
  - Relationship: +0.6 (friendly, traded favors before)
  - Military: 12 turrets, 5 vehicles
  - Distance from Red: ~3000 units
  - Threat level to Red: 25.0 (Blue is stronger than Red)
  - Known to dislike Red: relationship Red-Blue = -0.4

**Urgency Calculation**:
```
My DPS: (2 turrets × 20) + (3 vehicles × 6.67) = 60 DPS
Enemy DPS: 8 vehicles × 6.67 = 53.4 DPS
DPS ratio: 60 / 53.4 = 1.12 (barely winning)
Time to city: 4 seconds (IMMINENT!)
Urgency: 0.9 (EMERGENCY!)
```

**Strategy Evaluation**:

**SECURITY (70%)**:

*Option 1: Mass Vehicles (Emergency Defense)*
```
Prerequisites:
  - Need: $1000 per vehicle
  - Have: $600
  - To afford 1 vehicle: 13 seconds wait
  - Enemy arrival: 4 seconds
  - BLOCKED: Can't afford before they arrive
  
Emergency tactic: Send current 3 vehicles to intercept
  - Fight happens away from city
  - Buys ~20 seconds before city engagement
  - Income during fight: $600 (20 sec × $30/sec)
  - Can build 1 vehicle after ~13 more seconds
  
Benefits (delaying action):
  - Security: 1.5 (urgency bonus)
  - Buys time for income
  - Might survive long enough to build reinforcements
  
Value: Emergency response, do it!
```

*Option 2: Request Help from Blue (NEW! Relationship-based)*
```
Prerequisites:
  - Blue relationship: +0.6 (ally)
  - Blue dislikes Red: -0.4 (common enemy)
  - Blue has superior force to Red
  - Blue distance: 3000 units = 6 seconds travel
  
Calculation:
  - My vehicles intercept: holds for ~20 seconds
  - Blue vehicles travel time: 6 seconds
  - Arrival time: Well before my vehicles defeated!
  - Blue+Me combined: >> Red's force
  
Benefits:
  - Security: 2.0 (ally cooperation bonus!)
  - No direct cost (favor economy, might owe favor later)
  - Relationship with Blue: +0.6 → +0.7 (cooperation strengthens bonds)
  - Red relationship: -0.9 → -1.0 (humiliated)
  
Risks:
  - Blue might not respond immediately
  - Owe favor to Blue (future obligation)
  - If Blue doesn't help, relationship drops to +0.3
  
Expected value: HIGH (ally with common enemy likely to help)
```

*Option 3: Fortify and Hold*
```
Current forces barely sufficient
Turrets can't move, but provide solid DPS
Send vehicles to intercept (buys time for turret range)

Benefits:
  - Security: 1.0 (standard defense)
  - Use existing assets efficiently
  
Risks:
  - 1.12× DPS advantage is tiny margin
  - Losing even 1 vehicle shifts to losing fight
  - City might take damage
```

**DOMINANCE (10%)**:

*Option 4: Counter-attack Red's City*
```
Red sent 8 vehicles here → their city is weaker!
Known: Red has 8 resources, possibly few defenders left

Prerequisites:
  - BLOCKED: All my vehicles needed for defense
  - Can't split forces
  
Deferred until after defense resolves
```

**WEALTH (20%)**:

*Option 5: Economic Response*
```
Build factory for long-term recovery?
BLOCKED: Emergency situation, no time
All planning deferred until survival secured
```

**FINAL DECISION** (Relationship-Aware):

**AI reasoning**:
```
1. Immediate: Send 3 vehicles to intercept (buys 20 seconds)
2. Simultaneous: Signal Blue for assistance
   - Blue relationship +0.6 is strong
   - Blue vs Red relationship -0.4 (common enemy)
   - Blue has force advantage (25.0 threat vs Red's ~15.0)
   - Expected response: 80% chance Blue helps
   
3. Backup plan if Blue doesn't respond:
   - Vehicles delay, then retreat to turret range
   - Build 1 vehicle at +13 seconds
   - Accept heavy losses but survive
   - Relationship with Blue drops (-0.3 penalty for not helping)
   
4. If Blue helps:
   - Combined force obliterates Red's attack
   - Red loses 8 vehicles ($8000 value)
   - Blue relationship strengthens (+0.1 cooperation)
   - Possible joint counter-attack on Red
```

**EMERGENT BEHAVIORS**:

1. **Ally Cooperation**: 
   - Strong relationship (+0.6) makes requesting help high-value
   - Common enemy (-0.4 Red-Blue) increases help probability
   - Security-focused AI recognizes diplomatic strength

2. **Relationship Exploitation**:
   - Red attacking created vulnerability at their city
   - Blue might be willing to help counter-attack after defense
   - Relationships enable multi-AI coordination

3. **Risk Assessment**:
   - Solo defense: 1.12× advantage = risky
   - With Blue: 3× advantage = safe
   - Relationship benefit >>> cost of owing favor

4. **Future Implications**:
   - Blue expects reciprocation later (implicit alliance mechanics)
   - Red relationship hits -1.0 (maximum enemy)
   - Future decisions will heavily favor attacking weakened Red
   - Security desire might route to "Destroy Red" for permanent safety

**OUTCOME** (assuming Blue helps):
```
Timeline:
  T+0s: Red vehicles spawn attack
  T+4s: My 3 vehicles intercept, fighting begins
  T+6s: Blue's 5 vehicles arrive (total: 8v vs 8v + 2 turrets in range)
  T+15s: Red forces destroyed, my losses: 1 vehicle
  T+20s: Blue returns home, relationship +0.7
  T+30s: My AI has $1200, builds replacement vehicle
  T+60s: Situation stable, begin planning counter-attack on Red

Future evaluation:
  - Red is weakened (lost 8 vehicles = $8000)
  - My Security desire might choose "Destroy Red City" strategy
    - Eliminates threat permanently (Security benefit: 18.0 × 2.0!)
    - Red can't rebuild quickly
  - Blue might join attack (relationship +0.7, common enemy)
  - Emergent alliance behavior without explicit code!
```

**WHY THIS IS BETTER THAN SOLO DEFENSE**:
- Relationships provide force multipliers (Blue's 5 vehicles = free army)
- Common enemies create natural cooperation opportunities
- Security AI recognizes diplomatic strength > pure military strength
- Future-oriented: Destroying Red = permanent security increase

---

## Open Questions & Future Refinements

### 1. Relationship Change Mechanics
- **When do relationships change?**
  - After every hostile action? (shoot vehicle → -0.05)
  - After strategic events? (captured city → -0.3)
  - Gradual decay over time? (enemies might reconcile)
  
- **How fast do relationships change?**
  - Single attack: -0.05 per vehicle destroyed
  - Major aggression (city assault): -0.3 immediate
  - Betrayal (attack former ally): -0.8 immediate
  - Cooperation (help in battle): +0.1 to +0.2
  
- **Can relationships recover?**
  - Currently: Simple decay (no active diplomacy)
  - Future: Peace offers, trade agreements, alliances
  - Start with irreversible for simplicity

### 2. Alliance Mechanics
- **Should AIs form explicit alliances?**
  - Current: Just relationship tracking (-1.0 to +1.0)
  - Future: Formal treaties, shared vision, coordinated attacks
  
- **How does AI ask for help?**
  - Implicit: High-relationship AIs automatically help?
  - Explicit: Request system with accept/decline?
  - Start with implicit (relationship > 0.5 = likely to help)
  
- **Betrayal costs?**
  - Attacking ally: Massive relationship penalty (-1.0 immediate)
  - Not helping when asked: Moderate penalty (-0.2 to -0.4)
  - Reputation system: All AIs learn about betrayals?

### 3. Patience & Risk as Multipliers
- Should patient AIs plan deeper (3 levels instead of 2)?
- Should risk-averse AIs require higher confidence thresholds?
- Implementation: Treat as optional modifiers to axioms/thresholds

### 4. Dynamic Desire Shifts
- Should severe situations override personality? (Under attack → Security spikes to 100%)
- Or should personality remain constant? (Aggressive AI stays aggressive even when losing)
- Philosophically: Is personality immutable, or do survival instincts override?

### 5. Aggressive Personality & Relationships
- Should aggressive AIs respect other aggressive AIs? (mutual respect)
- Or hate them? (competition for dominance)
- Current design: Relationship separate from personality
  - Aggressive + enemy relationship: Very hostile
  - Aggressive + neutral: Might initiate conflict
  - Aggressive + ally: Loyal but domineering

### 6. Opportunity Cost
- How to quantify "opportunity cost of not having vehicles now"?
- Time-based: "Every minute without defense costs X potential damage"
- May need "risk cost" axiom

### 7. Strategy Duration Prediction
- How long will a Siege realistically last?
- Historical data (Theories system)?
- Simple heuristic (enemy strength / own DPS = time to victory)?

### 8. Failed Goal Memory
- If "Assault" fails twice, should AI stop trying?
- Or just requirement for higher confidence?
- Affects learning/adaptation

---

## Implementation Phases (High-Level)

### Phase 0: Relationship System (FIRST!)
**Do this BEFORE refactoring controller**
- Create FAIRelationship struct:
  ```cpp
  struct FAIRelationship {
    float Value;  // -1.0 to +1.0
    TArray<FRelationshipEvent> History;  // Optional: track what caused changes
  };
  ```
- Add to AITeamController:
  ```cpp
  TMap<AAITeamController*, FAIRelationship> Relationships;
  ```
- Implement basic relationship changes:
  - OnVehicleDestroyed(AAITeamController* Attacker): Attacker.Relationship -= 0.05
  - OnCityCaptured(AAITeamController* Attacker): Attacker.Relationship -= 0.3
  - OnResourceCaptured(AAITeamController* Attacker): Attacker.Relationship -= 0.1
- Calculate threat with relationships:
  ```cpp
  float Threat = EnemyStrength * -(Relationship - 1.0);
  ```
- Test with current AI system (confirm relationships track correctly)

### Phase 1: Desire → Strategy Mapping
- Remove CalculateGoalWeights() (goals layer)
- Create strategy evaluation system:
  ```cpp
  struct FStrategyEvaluation {
    EStrategy Type;
    AAITeamController* Target;  // nullptr for non-targeted
    float DominanceBenefit;
    float WealthBenefit;
    float SecurityBenefit;
    float TotalValue;  // Weighted by desires
  };
  ```
- Implement EvaluateStrategy(Strategy, Target) for each strategy
- Per-target evaluation for targeted strategies

### Phase 2: Derived Benefit Calculations
- Implement assessment functions:
  - AssessDefenses(Location) → DPS estimate
  - EstimateAttrition(MyForce, TheirDefense) → Expected losses
  - CalculateNetWealth(Strategy, Target) → Income gain - costs
- Real-time threat calculation per AI
- Relationship modifiers in benefit calculations

### Phase 3: Prerequisite & HTN Resolution
- Implement prerequisite checking (2-level max)
- Build goal resolution with weight flow
- Add consolidation system
- Test that blocked strategies defer to prerequisites

### Phase 4: Time & Rates
- Add temporal pattern data to strategies
- Implement rate calculations (production, consumption)
- Build investment comparison logic
- Payback period analysis

### Phase 5: Urgency Integration
- Calculate urgency from situation
- Modify method selection based on urgency
- Add urgency thresholds to strategies
- Emergency override logic

### Phase 6: Multi-Desire Strategy Scoring
- Score strategies by combined pathway benefits
- Weight by active pathway desires
- Select highest-value strategy
- Handle ties (prefer multi-benefit strategies)

### Phase 7: Integration & Testing
- Wire into existing action execution
- Replace current strategy selection entirely
- Test with different personality profiles
- Multi-AI scenarios with evolving relationships

### Phase 8: Logging & Debugging
- Show pathway flow (Dominance → Wealth)
- Show consolidation (70% wealth from 3 sources)
- Show investment decisions (factory vs direct buy)
- Relationship change logs
- Strategy evaluation scores

---

## Success Criteria

The system works if:

1. **Economic AIs prioritize wealth** even when desires conflict
   - Greedy AI builds factories before assaulting
   - Captures neutral resources over attacking weak enemies (relationship cost)

2. **Defensive AIs invest in security** through appropriate pathways
   - Secure AI weakens threats OR builds defenses (not confused)
   - Uses relationships to identify biggest threats

3. **Aggressive AIs recognize economic prerequisites**
   - Dominance-focused AI still builds economy to fund wars
   - Chooses attackable targets (weak enemies, not strong allies)

4. **Urgency overrides optimization**
   - Under attack → fast response, not efficient investment
   - Calls for ally help when available (relationship > 0.5)

5. **Relationship-aware target selection**
   - AI evaluates same strategy against multiple targets differently
   - Prefers attacking enemies over neutrals (when equal value)
   - Avoids attacking allies (massive value penalty)
   - Example: Two enemies with same resources, attacks weaker one

6. **Derived benefits prevent traps**
   - AI sees "capture heavily defended resources" has NEGATIVE wealth benefit
   - Doesn't pursue strategies with net losses
   - Attrition costs factored into all combat decisions

7. **Weight consolidation creates focus**
   - AI doesn't split between 10 small goals, focuses on 1-2 big goals
   - Multiple desires route to same strategy (weight merging)

8. **Emergent long-term planning**
   - AI "plans" by chaining prerequisites without explicit planner
   - Logs show clear "I want X → need Y → doing Z" chains

9. **Different personalities produce different behaviors**
   - Greedy AI plays differently than Aggressive AI
   - Same situation, different responses based on personality
   - Relationships evolve differently based on personality

10. **Emergent diplomacy** (NEW!)
    - AIs with common enemies naturally cooperate
    - Strong AIs assist weaker allies when attacked
    - Repeated cooperation strengthens relationships
    - Betrayal creates lasting enmity
    - All without hardcoded alliance logic!

---

## Visual: Complete Decision Flow (with Relationships)

```
PERSONALITY TRAITS (Aggression, Greed, Security, Patience, Risk)
       ↓
    DESIRES (Dominance 30%, Wealth 50%, Security 20%)
       ↓
┌──────────────────────────────────────────────────┐
│ RELATIONSHIP ASSESSMENT (NEW!)                   │
│                                                  │
│ For each other AI:                               │
│  - Relationship value: -1.0 to +1.0             │
│  - Threat = Strength × -(Relationship - 1)      │
│  - Opportunity = Vulnerability × (1-Relationship)│
│                                                  │
│ Red: -0.7 enemy, 10 resources, 6 turrets        │
│   → Threat: 15.0, Opportunity: 1.7              │
│                                                  │
│ Green: 0.0 neutral, 3 resources, 0 turrets      │
│   → Threat: 1.0, Opportunity: 1.0               │
│                                                  │
│ Blue: +0.6 ally, 5 resources, 12 turrets        │
│   → Threat: 0.0, Opportunity: 0.0 (ally!)       │
└──────────────────────────────────────────────────┘
       ↓
STRATEGY EVALUATION (per-target, derived benefits)
       ↓
┌──────────────────────────────────────────────────┐
│ Against Red:                                     │
│  - Assault City: Dom 1.0, Wealth 0.5, Sec 7.5   │
│    × Relationship 1.2 (enemy bonus)             │
│    = 2.22 value BUT prerequisite blocked        │
│                                                  │
│  - Capture Resources: Dom 0.7, Wealth -7.0, Sec 7.5 │
│    = -1.79 (NEGATIVE! Don't do)                 │
│                                                  │
│ Against Green:                                   │
│  - Assault City: Dom 1.0, Wealth 0.15, Sec 0.5  │
│    × Relationship 0.7 (neutral penalty)         │
│    = 0.298 value, can do now                    │
│                                                  │
│  - Capture Resources: Dom 0.7, Wealth 1.5, Sec 0.5 │
│    × Relationship 0.7 (neutral penalty)         │
│    = 0.679 value, need 1 vehicle first          │
│                                                  │
│ No target (internal):                            │
│  - Capture Neutral Resources: Wealth 1.0        │
│    × Relationship 1.0 (no penalty)              │
│    = 0.63 value, can do NOW                     │
│                                                  │
│  - Build Factory: Wealth 1.0                    │
│    = 0.52 value, need $200 more                 │
└──────────────────────────────────────────────────┘
       ↓
CHECK PREREQUISITES (can I do it?)
       ↓ NO → Recurse (max 2 levels)
RESOLVE DEPENDENCIES (what do I need first?)
       ↓
CONSOLIDATE WEIGHTS (merge duplicate needs)
       ↓
┌──────────────────────────────────────────────────┐
│ Strategy: Capture Neutral Resources              │
│  - From Dominance desire (30%): 0.3 × 0.3 = 0.09 │
│  - From Wealth desire (50%): 0.5 × 1.0 = 0.5     │
│  - From Security desire (20%): 0.2 × 0.2 = 0.04  │
│  Total weight: 0.63                              │
│                                                  │
│ This consolidation makes neutral resources       │
│ more valuable than attacking Green (0.679)       │
│ BECAUSE neutral has no relationship penalty!     │
└──────────────────────────────────────────────────┘
       ↓
CALCULATE RATES (how fast can I produce/consume?)
       ↓
INVESTMENT ANALYSIS (build now vs invest first?)
       ↓
┌──────────────────────────────────────────────────┐
│ Factory: $1000 cost, +$1000/min income           │
│ Payback: 1 minute                                │
│ Time horizon: 5+ minutes (long-term prep)        │
│ Value over horizon: $5000                        │
│                                                  │
│ Vehicles: $1000 each, immediate tactical value   │
│ Can capture resources now: +$200/min immediate   │
│ Value over horizon: $1000                        │
│                                                  │
│ Factory is 5× better long-term, BUT...          │
│ Resources are one-time opportunity (if gone, gone)│
│ → Capture resources first = 0.63 weighted value  │
│ → Factory after = 0.52 weighted value            │
│ Decision: Resources > Factory                    │
└──────────────────────────────────────────────────┘
       ↓
URGENCY CHECK (how much time do I have?)
       ↓
MULTI-DESIRE SCORING (strategy benefits multiple desires?)
       ↓
SELECT STRATEGY (highest-value method)
       ↓
┌──────────────────────────────────────────────────┐
│ FINAL DECISION: Capture Neutral Resources        │
│                                                  │
│ Why not attack Green?                            │
│  - Relationship penalty (× 0.7) reduces value    │
│  - Making new enemies has strategic cost         │
│  - Neutral resources available with no penalty   │
│                                                  │
│ Why not attack Red?                              │
│  - Attrition cost creates NEGATIVE wealth (-7.0) │
│  - Prerequisites blocked (need 9 vehicles)       │
│  - Derived benefit calculation prevents trap     │
│                                                  │
│ Emergent behavior:                               │
│  - Target selection from environment analysis    │
│  - Relationship costs create "peaceful" choices  │
│  - Economic calculation prevents costly attacks  │
│  - No hardcoded "don't attack strong enemies"!   │
└──────────────────────────────────────────────────┘
       ↓
EXECUTE ACTIONS (concrete implementation)
```

---

## Conclusion

This architecture creates **emergent intelligence** through:

1. **Hierarchical Task Networks (HTN)**: Automatically finds prerequisites
   - "Want to assault" → "Need vehicles" → "Need income" → "Capture resources"
   - Max 2 recursion levels prevents analysis paralysis
   - Weight flows down chain (30% assault → 30% build → 30% economy)

2. **Relationship-Aware Evaluation**: Same strategy, different value per target
   - Target selection from opportunity + relationship cost
   - Common enemies create natural cooperation
   - Attrition-heavy fights avoided (derived negative benefits)
   - Neutral options preferred over creating new enemies

3. **Economic Realism**: Rates and investment analysis
   - Production rate vs consumption rate determines sustainability
   - Payback period vs time horizon for investment decisions
   - Attrition costs factored into all combat strategies
   - Prevents "economic traps" (high-cost, low-reward actions)

4. **Desire Consolidation**: Diverse motivations → focused execution
   - Multiple desires route to same strategy (weight merging)
   - Multi-benefit strategies naturally preferred
   - AI doesn't split attention across 10 goals
   - Creates single-minded focus from complex motivations

5. **Temporal Reasoning**: Plans ahead when safe, reacts when urgent
   - Urgency overrides optimization (emergency response)
   - Investment vs immediate spending based on time horizon
   - Long-term strategies (factories) vs short-term (vehicles)
   - Adapts from planning mode to reactive mode seamlessly

6. **Derived Benefits**: Environment determines strategy value
   - Not hardcoded percentages
   - Same strategy evaluated differently based on situation
   - Can be zero or negative (don't pursue!)
   - Creates intelligent-looking target prioritization

The AI doesn't "think" in the human sense, but by following these rules, it produces behavior that **looks** intelligent:

- **Prioritizes economy when weak** (can't afford combat → build income)
- **Attacks when strong** (overwhelming force → low attrition)
- **Defends when threatened** (urgency spike → emergency response)
- **Invests for long-term gain** (safe situation + time → factories)
- **Reacts to emergencies** (urgency override → immediate action)
- **Chooses targets wisely** (weak enemies over strong, neutral resources over making enemies)
- **Cooperates with allies** (relationship > 0.5 → likely to help)
- **Forms emergent rivalries** (repeated aggression → permanent enmity)

**All without**:
- Hardcoded decision trees
- Explicit planning algorithms
- Scripted behaviors
- Alliance logic
- Target selection rules

**Just**:
- Axioms (game physics rules)
- Dependencies (prerequisites)
- Mathematics (rates, costs, benefits)
- Relationships (simple -1.0 to +1.0 tracking)

The complexity emerges from **simple rules interacting**, not from complex programming. This is the essence of good AI design: **emergence over prescription**.

---

## Key Design Principles

1. **No Goals Layer**: Desires map directly to strategies
   - Eliminates confusion ("cripple economy" → what benefit?)
   - Strategies have clear, calculable outcomes
   - Benefits derived from environment, not hardcoded meanings

2. **Relationships Inform Value**: Same action, different contexts
   - Attacking enemy (-0.7): Good (relationship bonus × 1.2)
   - Attacking neutral (0.0): Costly (relationship penalty × 0.7)
   - Attacking ally (+0.6): Terrible (relationship penalty × 0.1)
   - Creates peaceful behavior without "don't attack" rules

3. **Negative Benefits Prevent Mistakes**: AI recognizes bad deals
   - Heavily defended resources: Attrition > gain = negative wealth
   - System naturally avoids without "don't attack strong enemies" rule
   - Economic calculation creates tactical intelligence

4. **Per-Target Evaluation**: Which enemy to attack?
   - Not hardcoded (always attack weakest/strongest/nearest)
   - Calculated from: Resources, defenses, relationship, threat
   - Different personalities prefer different targets naturally
   - Greedy → easy resources, Aggressive → threatening enemies

5. **Multi-Desire Consolidation**: Synergistic strategies
   - "Capture enemy resources" serves Dominance + Wealth + Security
   - Naturally preferred over single-benefit strategies
   - Creates focused behavior from diverse motivations
   - AI appears to have clear priorities despite complex personality

This design enables **3+ AIs with different personalities** to:
- Develop unique relationships with each other
- Form temporary coalitions against common threats
- Pursue different strategies in same situation
- Create emergent political dynamics
- All from personality traits + simple relationship tracking

**The game becomes interesting** not from scripted events, but from **emergent interactions** between AI agents following simple but well-designed rules.
