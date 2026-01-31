# Gravnic-Style Puzzle Level Generation Notes

## Game Overview

Based on Gravnic (bonus mode in Puzznic, NES 1990):
- 4-way directional input changes gravity for all pieces
- All movable gems slide in the chosen direction until hitting walls/obstacles
- Matching gems (same color, adjacent) disappear
- Win: Clear all gems within move limit
- Some obstacles are fixed, some slide with gravity

---

## Verification Approaches

### 1. BFS/DFS with State Hashing (Recommended for validation)

Exhaustive search is feasible due to small state space:

```cpp
struct State {
    uint64_t grid_hash;  // Compact grid representation
    uint8_t moves_remaining;
};

bool is_solvable(const State& initial) {
    std::unordered_set<uint64_t> visited;
    std::queue<State> frontier;
    frontier.push(initial);
    
    while (!frontier.empty()) {
        State current = frontier.front();
        frontier.pop();
        
        if (is_win(current)) return true;
        if (visited.count(current.grid_hash)) continue;
        visited.insert(current.grid_hash);
        
        if (current.moves_remaining == 0) continue;
        
        for (Direction dir : {UP, DOWN, LEFT, RIGHT}) {
            State next = apply_gravity(current, dir);
            next.moves_remaining--;
            frontier.push(next);
        }
    }
    return false;
}
```

**Complexity:** O(4^max_moves) worst case, but much smaller in practice due to:
- State convergence (different move sequences → same grid)
- Hash-based duplicate elimination
- Early termination on win states

### 2. Bidirectional Search

For high move counts (10+), search from both initial state forward and cleared board backward. Tricky because reverse gravity isn't always deterministic.

### 3. Quick Rejection Heuristics

Before full solve, check:
- Odd number of any gem color → unsolvable
- Isolated gems with no path to others → likely unsolvable
- Simple parity checks on grid structure

---

## Backwards Generation (Constructive Approach)

### Core Idea

Instead of generating randomly and verifying, **start from solved state and work backwards**:

```
FORWARD (player):
  Initial gems → Move1 → matches → Move2 → matches → ... → empty (win)

BACKWARD (generator):
  Empty ← "undo" Move1 ← "unplace" gems ← "undo" Move2 ← ... ← initial state
```

### Challenge: Gravity Isn't Cleanly Reversible

Given final state X and "last move was LEFT", many previous states could lead to X. Gems might have:
- Moved and matched (disappeared)
- Moved and stopped against wall
- Not moved (already against left wall)

### Solution: Construct Solution and Level Simultaneously

```cpp
struct GemPlacement {
    Vec2 position;
    Color color;
    int appears_after_move;  // -1 means present at start
};

Level generate_level(int num_moves, int difficulty) {
    Grid obstacles = generate_obstacle_layout(difficulty);
    std::vector<Direction> solution;
    std::vector<GemPlacement> gems;
    
    // Decide solution sequence first
    for (int i = 0; i < num_moves; i++) {
        solution.push_back(pick_interesting_direction(obstacles, i));
    }
    
    // Place gem pairs that will match at specific moves
    for (int move_idx = 0; move_idx < num_moves; move_idx++) {
        // Find where two gems need to be BEFORE this move
        // so that applying solution[move_idx] brings them together
        
        auto [pos_a, pos_b] = find_matching_pair_positions(
            obstacles, 
            solution, 
            move_idx
        );
        
        Color c = random_color();
        gems.push_back({pos_a, c, -1});
        gems.push_back({pos_b, c, -1});
    }
    
    return {obstacles, gems, solution};
}
```

### Finding Valid Gem Pair Positions

For a match to occur on move M in direction D:

```cpp
std::pair<Vec2, Vec2> find_matching_pair_positions(
    const Grid& obstacles,
    const std::vector<Direction>& solution,
    int target_move
) {
    Direction d = solution[target_move];
    
    // Find two cells that:
    // 1. Are NOT adjacent BEFORE move M (would match early)
    // 2. BECOME adjacent AFTER applying moves 0..M
    // 3. The path closes specifically on move M
    
    // Simulate forward to move M-1 to get intermediate state
    // Then find pairs where applying D causes collision
    
    // Example for D = LEFT:
    // Gem A stops at position X (against wall/obstacle)
    // Gem B slides into X+1 from the right
    // After LEFT, B hits A → match
}
```

---

## Dependency Graph Approach (Advanced)

Model puzzle as a directed acyclic graph of match events:

```cpp
struct MatchEvent {
    int id;
    Color color;
    int occurs_on_move;
    std::vector<int> depends_on;  // MatchEvents that must happen first
};

// Generate DAG of match events
// Place gems to satisfy the DAG
// Deeper DAG = harder puzzle (more sequential dependencies)
```

### Example Dependency Chain

```
Move 1 (DOWN): Red gems match, clearing a path
Move 2 (LEFT): Blue gems slide through where reds were, match
Move 3 (UP):   Green gems match using space freed by blue
```

This guarantees:
- Unique intended solution path
- Not trivially solvable by random moves
- Interesting puzzle structure

---

## Making Puzzles Interesting

### Difficulty Levers

1. **Red herrings**: Extra gem pairs matching on different moves (visual noise)
2. **Dependent chains**: Pair A must match first to create space for pair B
3. **Moving obstacles**: Obstacles that slide with gravity increase complexity
4. **Solution length**: More moves = more mental state tracking
5. **Near-traps**: Obvious moves cause mismatches (odd gem left over)

### Difficulty Metrics

- Move count (optimal solution length)
- Dependency depth (longest chain in DAG)
- Obstacle density
- Number of gem colors
- Red herring count

---

## Production Pipeline for Racing Game

### Recommended Architecture

1. **Pre-generate large pools** of verified levels at various difficulties
2. **Tag each level** with properties:
   - Move count
   - Dependency depth  
   - Obstacle density
   - Estimated solve time
3. **Matchmaking selects levels** based on player skill ratings
4. **Seeded generation** for reproducible level sequences (replays, sharing)
5. **Forward solver verification** as sanity check after generation

### Hybrid Approach: Generate + Filter

```cpp
void build_level_pool(int target_count, Difficulty diff) {
    while (pool.size() < target_count) {
        Level candidate = generate_candidate(diff);
        
        // Quick rejection
        if (!passes_heuristics(candidate)) continue;
        
        // Full verification
        auto [solvable, optimal_moves] = solve(candidate);
        if (!solvable) continue;
        
        candidate.metadata.optimal_moves = optimal_moves;
        pool.push_back(candidate);
    }
}
```

---

## Implementation Notes

### State Hashing

For small grids, pack entire state into uint64_t:
- 2 bits per cell (empty, wall, gem colors 0-3)
- 8x8 grid = 128 bits (use two uint64_t or optimize)

### Simulation Performance

Gravity application is the hot path:
- Use SIMD for parallel gem movement
- Precompute "slide distance" lookup tables per direction
- Cache obstacle positions

### Memory Considerations

- BFS can use significant memory for large state spaces
- Consider iterative deepening DFS (IDDFS) as alternative
- State compression reduces memory footprint

---

## Match Formats Discussion

### Best of N (Individual Races)
- Each level isolated
- Players can take risks
- Encourages aggressive play and pattern recognition speed

### First to Complete N (Continuous)
- Mistakes compound
- Creates tension and comeback potential
- Scales to battle royale (Tetris 99 style)
- Better for spectators/streamers

---

*Last updated: [Current conversation date]*
