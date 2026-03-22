# AI Decision Tree - Planet Conquest

## System Overview

The AI makes decisions every **5 seconds** through a multi-stage pipeline. Below is the complete flow with color-coding:

- 🟦 **Blue** = Input/Data Gathering
- 🟩 **Green** = Calculation/Processing  
- 🟨 **Yellow** = Decision Points
- 🟧 **Orange** = Modifiers/Boosts
- 🟥 **Red** = Execution/Actions

---

## 📊 Stage 1: Pressure Gathering (Environmental Sensors)

```mermaid
graph TD
    A[🟦 AI Update Tick - Every 5 Seconds] --> B[🟦 Gather Pressures]
    
    B --> C1[🟦 Count Resources<br/>ProximalResources<br/>OwnedResources]
    B --> C2[🟦 Count Vehicles<br/>VehiclesOwned]
    B --> C3[🟦 Count Cities<br/>CitiesOwned<br/>ProximalEnemyCities]
    B --> C4[🟦 Money Check<br/>Current Funds]
    B --> C5[🟦 Enemy Detection<br/>ProximalEnemyVehicles]
    B --> C6[🟦 City Health<br/>LowestCityHealthPercent<br/>CitiesUnderActiveAttack]
    B --> C7[🟦 Stuck Vehicles<br/>Position Tracking]
    B --> C8[🟦 Incoming Threats<br/>5-Second Prediction]
    
    C8 --> C8A[🟩 Check Enemy Velocity]
    C8A --> C8B[🟩 Project Position]
    C8B --> C8C[🟨 Heading Toward City?]
    C8C --> C8D[🟦 IncomingThreats +1]
    
    style A fill:#4A90E2
    style B fill:#4A90E2
    style C1 fill:#4A90E2
    style C2 fill:#4A90E2
    style C3 fill:#4A90E2
    style C4 fill:#4A90E2
    style C5 fill:#4A90E2
    style C6 fill:#4A90E2
    style C7 fill:#4A90E2
    style C8 fill:#4A90E2
    style C8A fill:#50C878
    style C8B fill:#50C878
    style C8C fill:#FFD700
    style C8D fill:#4A90E2
```

---

## 🧮 Stage 2: Utility Calculations (Decision Scoring)

Each action gets a **utility score** based on personality and pressures:

```mermaid
graph LR
    A[🟩 Calculate Utilities] --> B1[🟩 Capture Resources]
    A --> B2[🟩 Capture Enemy Resources]
    A --> B3[🟩 Build Vehicles]
    A --> B4[🟩 Build Turrets]
    A --> B5[🟩 Build Factories]
    A --> B6[🟩 Capture Cities]
    A --> B7[🟩 Attack Vehicles]
    A --> B8[🟩 Defend Cities]
    
    style A fill:#50C878
    style B1 fill:#50C878
    style B2 fill:#50C878
    style B3 fill:#50C878
    style B4 fill:#50C878
    style B5 fill:#50C878
    style B6 fill:#50C878
    style B7 fill:#50C878
    style B8 fill:#50C878
```

### 🎯 Detailed Utility Formulas

#### 1️⃣ Capture Resources
```
Base = 0.7 + (Greed × 0.3)
ProximityBonus = ProximalResources / 8.0 (Linear)
SaturationPenalty = 1 - (OwnedResources / 10.0) (Linear)
MoneyNeed = 1 - (Money / 2000) (Sigmoid)

UTILITY = Base × (1 + ProximityBonus) × Saturation × (1 + MoneyNeed) × PersonalityModifier

PersonalityModifier:
- Overextender (Aggression×Greed>0.6): 1.2×
- Turtle (Greed×Defensiveness>0.6): 0.8×
```

#### 2️⃣ Build Vehicles
```
Base = 0.5
SafetyBonus = 1 - (CitiesUnderAttack / 3.0) (Exponential)
MoneyAvailable = (Money - 1000) / 2000 (Sigmoid)

UTILITY = Base × (1 + SafetyBonus) × MoneyAvailable
```

#### 3️⃣ Build Turrets  
```
Base = 0.2 + (Defensiveness × 0.3)

REQUIRES: CitiesUnderAttack > 0 OR ProximalEnemyVehicles > 3
OTHERWISE: UTILITY = 0

ThreatLevel = ProximalEnemyVehicles / 10.0 (Exponential)
UnderAttackUrgency = CitiesUnderAttack / 3.0 (Exponential) × 2.0
VulnerabilityBonus = (1 - LowestCityHealth) × 2.0 IF health < 0.8

Urgency = MAX(UnderAttackUrgency, ThreatLevel + Vulnerability)
MoneyAvailable = (Money - 2500) / 3500 (Sigmoid)

UTILITY = Base × Urgency × MoneyAvailable
```

#### 4️⃣ Build Factories
```
Base = 0.5 + (Greed × 0.4)
TotalFactories = Count all city factories
Saturation = 1 - (TotalFactories / 15.0) (Linear)
CurrentIncome = (Resources + Cities + Factories) × 100
IncomeNeed = 1 - (CurrentIncome / 1000) (Sigmoid)
MoneyAvailable = (Money - 3000) / 2000 (Sigmoid)
SafetyFactor = 1 - (CitiesUnderAttack × 0.2)

UTILITY = Base × (0.5 + Saturation) × (0.5 + IncomeNeed) × MoneyAvailable × SafetyFactor × PersonalityModifier

PersonalityModifier:
- Turtle Economy (Greed×Defensiveness>0.6): 1.2×
```

#### 5️⃣ Capture Cities
```
Base = Aggression

FOR EACH enemy city:
    Assess Strength = CityHP + TurretHP + TurretThreat + DefenderThreat
    Assess Resources = Count within 3000 units
    WeaknessFactor = 1 - (Strength / 5000) [clamped 0.1-1.0]
    ResourceFactor = 0.5 + (Resources × 0.2) [max 1.5]
    StrategicValue = Weakness × Resources
    
    VehiclesNeeded = Strength / 150 [min 3]
    ConfidenceRatio = AvailableVehicles / VehiclesNeeded
    TargetScore = (Confidence × 0.7) + (StrategicValue × 0.3)

BestTarget = Highest TargetScore
BestConfidence = Target's confidence ratio

Confidence Scaling:
- >= 1.5: ConfidenceFactor = 1.0 (full confidence)
- 1.0-1.5: ConfidenceFactor = (Confidence - 1.0) × 2.0
- 0.7-1.0: ConfidenceFactor = (Confidence - 0.7) × 1.5 × Aggression (only if Aggression > 0.7)
- < 0.7: ConfidenceFactor = 0 (no attack)

Opportunity = ProximalEnemyCities / 5.0 (Linear)
DefensivenessPenalty = 1 - (Defensiveness × 0.5)

UTILITY = Base × ConfidenceFactor × (1 + Opportunity × 0.3) × DefensivenessPenalty × PersonalityModifier

PersonalityModifier:
- Overextender (Aggression×Greed>0.6) + VehiclesOwned<8: 0.7× (penalty)
- Fortress (Aggression×Defensiveness>0.6) + VehiclesOwned>12: 1.3× (bonus)
```

#### 6️⃣ Defend Cities
```
Base = 0.6 + (Defensiveness × 0.4)

Threat = ProximalEnemyVehicles / 10.0 (Exponential)
IncomingThreatBonus = IncomingThreats / 5.0 (Linear) × 1.5
ActiveAttackUrgency = CitiesUnderAttack / 3.0 (Exponential) × 3.0
VulnerabilityBonus = (1 - LowestCityHealth) × 2.0 IF health < 0.8

FOR EACH damaged city:
    DamageRatio = 1 - (CurrentHealth / MaxHealth)
    DamagedCityBonus += DamageRatio × 1.5

CityValue = CitiesOwned / 5.0 (Linear)

UTILITY = Base × (1 + Threat + IncomingThreat + ActiveAttack + Vulnerability + DamagedCity) × CityValue × PersonalityModifier

PersonalityModifier:
- Fortress (Aggression×Defensiveness>0.6): 1.2×
```

#### 7️⃣ Attack Vehicles
```
Base = (Aggression × 0.7) + (Defensiveness × 0.3)
Opportunity = ProximalEnemyVehicles / 15.0 (Linear)
VehicleReadiness = VehiclesOwned from 4-12 (Sigmoid)
Superiority = OurVehicles / EnemyVehicles [clamped 0.5-1.5]

UTILITY = Base × (1 + Opportunity) × VehicleReadiness × Superiority
```

#### 8️⃣ Capture Enemy Resources
```
Base = 0.5 + (Aggression × 0.3) + (Greed × 0.2)
EnemyResourcesNearby = Count enemy-owned resources in range
Opportunity = EnemyResourcesNearby / 5.0 (Linear)
VehicleReadiness = VehiclesOwned from 3-10 (Sigmoid)
ConflictWillingness = 1 - (Defensiveness × 0.3)

UTILITY = Base × (1 + Opportunity) × VehicleReadiness × ConflictWillingness × PersonalityModifier

PersonalityModifier:
- Overextender (Aggression×Greed>0.6): 1.2×
- Turtle (Greed×Defensiveness>0.6): 0.8×
```

---

## 🎯 Stage 3: Strategic Focus (Commitment System)

```mermaid
graph TD
    A[🟩 All Utilities Calculated] --> B[🟧 Apply Strategic Focus Boost]
    
    B --> C{🟨 Current Focus<br/>Strength > 0.1?}
    C -->|Yes| D[🟧 Boost Focused Action<br/>Multiplier: 1.0 + FocusStrength × 0.5]
    C -->|No| E[No Boost]
    
    D --> F[🟧 Decay Focus<br/>Decay = Lerp 0.15-0.03 by AttentionSpan]
    E --> F
    
    style A fill:#50C878
    style B fill:#FF9933
    style C fill:#FFD700
    style D fill:#FF9933
    style E fill:#50C878
    style F fill:#FF9933
```

**Focus Strength Dynamics:**
- Starts at `0.7 + (AttentionSpan × 0.3)` when committed
- Decays each update by `Lerp(0.15, 0.03, AttentionSpan)`
- High AttentionSpan = slower decay, stronger commitment

---

## ⚖️ Stage 4: Normalization

```mermaid
graph TD
    A[🟩 Normalize Priorities] --> B[🟩 Sum All Utilities]
    B --> C[🟩 Divide Each by Total]
    C --> D[🟦 Normalized Weights<br/>Sum = 100%]
    
    style A fill:#50C878
    style B fill:#50C878
    style C fill:#50C878
    style D fill:#4A90E2
```

Example output:
- Resources: 35%
- Factories: 25%
- Cities: 20%
- Defense: 15%
- Vehicles: 5%
- (etc.)

---

## 🎲 Stage 5: Update Strategic Focus

```mermaid
graph TD
    A[🟨 Find Winning Action] --> B[🟨 Highest Weight?]
    B --> C{🟨 Action Type?}
    
    C -->|Aggressive Action| D[🟨 Threshold: 35%]
    C -->|Other Action| E[🟨 Threshold: 25%]
    
    D --> F{🟨 Weight > Threshold<br/>AND<br/>Focus Strength < 0.2?}
    E --> F
    
    F -->|No| G[🟧 Keep Current Focus<br/>Continue Decay]
    F -->|Yes| H{🟨 Special: Holding Offensive?}
    
    H -->|City Attack<br/>+ Focus > 0.3<br/>+ Money > 2000| G
    H -->|No| I[🟧 Commit to New Action!<br/>FocusStrength = 0.7 + AttentionSpan×0.3]
    
    G --> J[🟦 Focus Updated]
    I --> J
    
    style A fill:#FFD700
    style B fill:#FFD700
    style C fill:#FFD700
    style D fill:#FFD700
    style E fill:#FFD700
    style F fill:#FFD700
    style G fill:#FF9933
    style H fill:#FFD700
    style I fill:#FF9933
    style J fill:#4A90E2
```

**Aggressive Actions:** Capture Cities, Attack Vehicles, Capture Enemy Resources  
**Special Rule:** Won't abandon city offensive unless low on resources

---

## 🎯 Stage 6: Vehicle Allocation

```mermaid
graph TD
    A[🟩 Allocate Vehicles] --> B[🟩 Get Available Vehicles]
    B --> C[🟩 Sort Actions by Priority]
    
    C --> D{🟨 For Each Action<br/>Highest to Lowest}
    
    D --> E{🟨 Building Action?}
    E -->|Yes| F[🟨 Check Money Only]
    E -->|No| G[🟨 Calculate Vehicle Need<br/>Weight × TotalVehicles]
    
    F --> H[🟨 Can Execute?]
    G --> I[🟨 Min Vehicles Available?]
    
    I -->|Yes| J[🟩 Allocate Vehicles<br/>Set bCanExecute = true]
    I -->|No| K[❌ Skip Action]
    
    H -->|Yes| J
    H -->|No| K
    
    J --> D
    K --> D
    
    style A fill:#50C878
    style B fill:#50C878
    style C fill:#50C878
    style D fill:#FFD700
    style E fill:#FFD700
    style F fill:#FFD700
    style G fill:#FFD700
    style H fill:#FFD700
    style I fill:#FFD700
    style J fill:#50C878
    style K fill:#FF6B6B
```

---

## ⚡ Stage 7: Action Execution

```mermaid
graph TD
    A[🟥 Execute Actions] --> B{🟨 For Each Action<br/>with bCanExecute}
    
    B --> C1[🟥 Capture Resources]
    B --> C2[🟥 Capture Enemy Resources]
    B --> C3[🟥 Build Vehicles]
    B --> C4[🟥 Build Turrets]
    B --> C5[🟥 Build Factories]
    B --> C6[🟥 Capture Cities]
    B --> C7[🟥 Attack Vehicles]
    B --> C8[🟥 Defend Cities]
    
    C1 --> D1[🟥 Assign Vehicles<br/>Set Target: Neutral Resources]
    C2 --> D2[🟥 Assign Vehicles<br/>Set Target: Enemy Resources]
    C3 --> D3[🟥 Spawn Vehicles<br/>At Best City]
    C4 --> D4[🟥 Build Turret<br/>At Most Threatened City]
    C5 --> D5[🟥 Build Factory<br/>At Safest City]
    C6 --> D6[🟥 Assess Best City<br/>Send Attack Force]
    C7 --> D7[🟥 Assign Hunters<br/>Find Targets Autonomously]
    C8 --> D8[🟥 Assign Defenders<br/>Guard Threatened Cities]
    
    style A fill:#FF6B6B
    style B fill:#FFD700
    style C1 fill:#FF6B6B
    style C2 fill:#FF6B6B
    style C3 fill:#FF6B6B
    style C4 fill:#FF6B6B
    style C5 fill:#FF6B6B
    style C6 fill:#FF6B6B
    style C7 fill:#FF6B6B
    style C8 fill:#FF6B6B
    style D1 fill:#FF6B6B
    style D2 fill:#FF6B6B
    style D3 fill:#FF6B6B
    style D4 fill:#FF6B6B
    style D5 fill:#FF6B6B
    style D6 fill:#FF6B6B
    style D7 fill:#FF6B6B
    style D8 fill:#FF6B6B
```

### Execution Details:

#### 🏗️ Build Actions (Immediate):
- **Build Vehicles**: Spawn at city with best strategic position
- **Build Turrets**: Build at most threatened/damaged city
- **Build Factories**: Build at safest city (fewest nearby threats)

#### 🚗 Vehicle Assignment Actions:
Vehicles are marked with:
- `AssignedAction` = Action type
- `bHasAssignment` = true
- `AIController` = This AI controller

Then they **autonomously find targets** via their own `FindNextTarget()` logic.

#### ⚔️ Special: City Attack
```
1. Assess ALL enemy cities (strength + resources)
2. Find best target (70% confidence + 30% strategic value)
3. Calculate vehicles needed
4. Calculate overkill ratio = 1.2 + (Aggression × 0.3)
5. Send VehiclesNeeded × OverkillRatio
6. All attackers target SAME city
```

---

## 🔄 Stage 8: Stuck Vehicle Recovery

```mermaid
graph TD
    A[🟨 Stuck Vehicles > 0?] -->|Yes| B[🟩 For Each Vehicle]
    A -->|No| Z[Skip]
    
    B --> C{🟨 Has Assignment?}
    C -->|Yes| D[🟩 Check Previous Position]
    C -->|No| B
    
    D --> E{🟨 Moved < 100 units?}
    E -->|Yes| F[🟥 STUCK DETECTED!]
    E -->|No| B
    
    F --> G[🟩 Calculate Opposite Side<br/>Same Radius, 180° Away]
    G --> H[🟥 Clear Assignment]
    H --> I[🟥 Send to Reboot Position]
    I --> J[🟦 Remove from Assigned List]
    
    style A fill:#FFD700
    style B fill:#50C878
    style C fill:#FFD700
    style D fill:#50C878
    style E fill:#FFD700
    style F fill:#FF6B6B
    style G fill:#50C878
    style H fill:#FF6B6B
    style I fill:#FF6B6B
    style J fill:#4A90E2
    style Z fill:#50C878
```

---

## 🧬 Personality Archetypes

Based on trait combinations, AIs develop distinct behaviors:

### 🗡️ Overextender (High Aggression × High Greed)
- **Strengths:**
  - +20% resource capture speed
  - +20% enemy resource capture
- **Weaknesses:**
  - -30% city attack when weak (<8 vehicles)
  - Tends to spread thin
  
### 🏰 Fortress (High Aggression × High Defensiveness)  
- **Strengths:**
  - +30% city attack when strong (>12 vehicles)
  - +20% defense priority
- **Behavior:**
  - Builds up forces before attacking
  - Heavily defends territory
  
### 🐢 Turtle Economy (High Greed × High Defensiveness)
- **Strengths:**
  - +20% factory building
- **Weaknesses:**
  - -20% enemy resource capture (stays close to home)
- **Behavior:**
  - Economic powerhouse
  - Risk-averse expansion

### ⚖️ Balanced (Medium All Traits)
- No strong modifiers
- Adapts to situations
- Most flexible playstyle

---

## 📈 Complete Pipeline Summary

```
Every 5 Seconds:
1. 🟦 Gather Pressures (8 types of environmental data)
2. 🟩 Calculate 8 Utility Scores (personality + pressures + modifiers)
3. 🟧 Apply Strategic Focus Boost (commitment to current strategy)
4. 🟩 Normalize to 100% (convert utilities to weights)
5. 🟨 Update Strategic Focus (commit to new strategy if decisive win)
6. 🟩 Allocate Vehicles (distribute based on weights)
7. 🟥 Execute Actions (spawn buildings, assign vehicle missions)
8. 🟧 Detect & Recover Stuck Vehicles (reboot to opposite side)
```

**Key Features:**
- ✅ Economic awareness (ROI calculations)
- ✅ Threat prediction (5-second lookahead)
- ✅ Strategic commitment (won't flip-flop strategies)
- ✅ Dynamic target selection (weakness + resources)
- ✅ Personality-driven modifiers (emergent behaviors)
- ✅ Stuck vehicle recovery (prevent deadlocks)

---

## 🎮 Example Decision Flow

**Scenario:** AI with Aggression=0.8, Greed=0.6, Defensiveness=0.3

**Pressures:**
- Money: $3500
- Vehicles: 10
- Cities: 2
- Resources: 4
- Enemy Vehicles Nearby: 6
- Cities Under Attack: 0
- Incoming Threats: 2

**Utility Calculations:**
1. **Capture Resources:** Base 0.88 × modifiers = **0.45**
2. **Build Factories:** Base 0.74 × modifiers = **0.52**
3. **Capture Cities:** Base 0.8 × Confidence 1.2 × modifiers = **0.85** ⭐
4. **Defend Cities:** Base 0.72 × (1 + Incoming 0.6) = **0.68**
5. Others: <0.4

**Strategic Focus:**
- Current: Capture Cities (strength 0.5)
- Winning Action: Capture Cities (85% after normalization)
- Decision: **HOLD COMMITMENT** (offensive doing well, money >$2000)
- Boost: 0.85 × (1 + 0.5×0.5) = **1.06** (boosted!)

**Allocation:**
- 50% of vehicles (5) → City Attack
- 25% of vehicles (2-3) → Defense  
- 15% of vehicles (1-2) → Resources
- 10% reserved

**Execution:**
- Assess enemy cities, find weak target with 3 resources nearby
- Send 6 vehicles (5 needed × 1.2 overkill)
- Log: "AI Team 1: Attacking CityB with 6 vehicles (needed 5, confidence 2.0x, strength 750, resources 3)"

---

## 🎯 Key Takeaways

1. **Utility-Based AI:** Every decision is scored numerically
2. **Personality Matters:** Same situation = different decisions based on traits
3. **Strategic Commitment:** Won't abandon winning strategies easily
4. **Resource-Aware:** Targets weak cities with high resource value
5. **Predictive Defense:** Defends against threats before they arrive
6. **Self-Correcting:** Recovers stuck vehicles automatically

The system creates **emergent strategies** without hard-coded decision trees!
