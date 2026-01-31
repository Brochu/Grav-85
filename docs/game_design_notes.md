# Game Design Notes for Competitive Gravnic-Style Puzzle Game

## Core Concept

Competitive puzzle racing game inspired by Gravnic (bonus mode in Puzznic, NES 1990). Players race to solve identical puzzles, competing in brackets or free-for-all formats.

---

## Core Mechanics (From Gravnic)

- **4-way gravity control**: D-pad changes gravity direction for all pieces
- **All pieces move simultaneously** in chosen direction until hitting walls/obstacles
- **Matching**: Same-color gems disappear when adjacent after a move
- **Win condition**: Clear all gems within move limit
- **Obstacles**: Some fixed, some slide with gravity

---

## Competition Format

### Primary Format: Free-For-All Bracket Tournament

8 players join a lobby and compete through elimination rounds.

```
Round 1: 8 players → top 4 advance
Round 2: 4 players → top 2 advance
Final:   2 players → 1 winner
```

**Each round is free-for-all** (not head-to-head pairings):
- All remaining players solve the same set of puzzles
- Total time across all puzzles determines ranking
- Top N players advance to next round

**Why free-for-all per round:**
- Simpler disconnect handling (player just ranks last)
- Everyone plays simultaneously
- No waiting for other matches to finish
- Natural spectator experience

### Puzzles Per Round

5-7 puzzles per round (to be tuned through playtesting).

**Considerations:**
- Too few: High variance, one mistake = elimination
- Too many: Rounds drag, fatigue sets in
- Sweet spot probably 5 puzzles (~2-5 minutes per round)

### Scaling Player Counts

| Players | Rounds | Advancement |
|---------|--------|-------------|
| 4 | 2 | 4→2→1 |
| 8 | 3 | 8→4→2→1 |
| 16 | 4 | 16→8→4→2→1 |
| 32 | 5 | 32→16→8→4→2→1 |

---

## Alternative Formats (Future Modes)

### Battle Royale (Tetris 99 Style)

- Large lobby (up to 99 players)
- Everyone plays same puzzles continuously
- Bottom percentage eliminated periodically
- Last player standing wins

### Pure Time Trial

- No elimination, just leaderboard
- All players complete all puzzles
- Final ranking by total time
- Good for casual/practice mode

### Head-to-Head Bracket

- Traditional tournament bracket
- 1v1 matches, first to solve N puzzles wins
- More direct rivalry feel
- More complex matchmaking

---

## Scoring & Ranking

### Primary: Total Time

Sum of solve times across all puzzles in a round. Fastest total advances.

### Alternative Scoring Ideas

**Per-puzzle points (diminishing over time):**
```
First to solve:  100 points
Within 5 sec:    90 points
Within 10 sec:   80 points
Within 20 sec:   70 points
...
```
Rewards consistency, not just raw speed.

**Move efficiency bonus:**
- Fewer moves than optimal = bonus points
- Encourages elegant solutions, not just fast ones

### Tiebreakers

If total time is identical (rare):
1. Fewer total moves used
2. Faster time on final puzzle
3. Faster time on first puzzle (reward fast starts)

---

## Disconnect Handling

**Free-for-all format simplifies this:**
- Disconnected player receives worst possible time for unsolved puzzles
- They rank last for that round (eliminated if applicable)
- No need to pause or wait for reconnection
- Other players unaffected

**Optional: Reconnection grace period**
- If player reconnects within X seconds, resume their timer
- Only viable if puzzle state is saved server-side

---

## Puzzle Difficulty

### Within a Round

Options:
- **Flat**: All 5 puzzles same difficulty
- **Escalating**: Puzzles get harder (1-2-3-4-5 difficulty)
- **Front-loaded**: Hard puzzle first, then easier (tests composure)

Recommendation: Slight escalation (easy opener, medium middle, hard closer).

### Across Rounds

Options:
- **Same difficulty all rounds**: Pure skill test
- **Increasing difficulty**: Finals are harder (raises stakes)
- **Random from pool**: Prevents memorization across sessions

---

## Player Experience Flow

### Lobby Phase
- Players join (up to 8 for bracket, more for battle royale)
- Countdown timer or "ready up" system
- Can see other players' ratings/stats

### During Round
- All players start simultaneously
- See own puzzle and timer
- Progress indicator: "Puzzle 3 of 5"
- Optional: See others' progress (puzzle number, not solution)

### Between Rounds
- Results screen: rankings, times, who advanced
- Brief break (10-15 seconds)
- Show updated bracket state
- "Round 2 starting in..."

### Match End
- Final standings
- Winner celebration
- Stats summary (total time, best puzzle, etc.)
- Option to rematch or return to lobby

---

## Information Display During Play

### What players should see:
- Their own puzzle board
- Move counter (moves used / max moves)
- Timer (current puzzle and/or total)
- Progress: "Puzzle 3/5"
- Their current standing: "You're in 3rd place"

### What to hide (adds tension):
- Exact times of other players (revealed at round end)
- Other players' boards (prevents copying)
- Precise gap to leader (just relative position)

### Optional reveal:
- Show other players' puzzle progress (which puzzle they're on)
- Creates moments of "they're ahead, I need to hurry"

---

## Comeback Mechanics

Problem: Falling behind early can feel hopeless in time trial.

### Ideas to Keep It Competitive

**1. Hidden times until round end**
- Players know position (1st, 2nd, 3rd) but not time gap
- Prevents "I'm 30 seconds behind, why bother"
- Adds suspense to results reveal

**2. Later puzzles worth more**
- Puzzle 1: 1x time weight
- Puzzle 5: 1.5x time weight
- Rewards finishing strong

**3. Per-puzzle scoring instead of raw time**
- Points decrease over time (100→90→80...)
- Caps maximum advantage from any single puzzle
- One great solve can offset one slow solve

**4. Clutch factor**
- Bonus for solving final puzzle fastest in round
- Rewards composure under pressure

---

## Skill Rating System

### Elo-style Rating

```cpp
void update_ratings_tournament(std::vector<Player>& players, std::vector<int>& placements) {
    // Compare each pair of players
    // Winner = higher placement
    // Adjust ratings based on expected vs actual outcome
}
```

### Rating Considerations

- **K-factor**: How much ratings change per match (higher = more volatile)
- **Placement weighting**: 1st vs 2nd matters more than 4th vs 5th?
- **Provisional period**: New players have higher K until N matches played

### Matchmaking Based on Rating

- Lobbies fill with similarly-rated players
- Skill range expands over queue time (avoid infinite waits)
- Display skill tier/rank to players

---

## Spectator Mode (Future Feature)

### Features
- Watch any player's board
- Switch between players freely
- See aggregate standings
- Commentator-friendly view (all boards at once)

### Tournament Streaming
- Delay to prevent cheating (if spectators can communicate with players)
- Highlight mode: auto-focus on closest races
- Replay system for highlights

---

## Progression & Rewards (Future Feature)

### Cosmetics
- Board themes
- Gem skins
- Victory animations
- Profile badges

### Unlocks
- Win N tournaments
- Reach rating threshold
- Complete daily/weekly challenges

### Seasons
- Rating resets (soft reset, not full)
- Season rewards based on peak rating
- Leaderboards

---

## Open Questions

1. **Puzzle count per round?** 5 feels right, needs playtesting

2. **Round time limit?** Should rounds have a max time, or go until everyone finishes/gives up?

3. **Surrender option?** Can players forfeit a round to save time?

4. **Practice mode?** Play puzzles without competition to learn mechanics?

5. **Puzzle seeding?** Same seed for all players (identical levels) vs seeded RNG per player (different but equivalent difficulty)?

6. **Move limit display?** Show optimal moves? Just max allowed? Neither?

7. **Audio/visual feedback?** Sounds for other players completing puzzles? Distracting or motivating?

---

## MVP Feature Set

For initial playable version:

**Must have:**
- [ ] Core Gravnic mechanics (gravity, matching, obstacles)
- [ ] Level generation (verified solvable)
- [ ] Single-player puzzle solving
- [ ] Basic timer
- [ ] 8-player lobby
- [ ] Free-for-all rounds with advancement
- [ ] Results screen

**Nice to have:**
- [ ] Skill ratings
- [ ] Matchmaking
- [ ] Progress visibility during round
- [ ] Between-round bracket display

**Future:**
- [ ] Battle royale mode
- [ ] Spectator mode
- [ ] Cosmetics/progression
- [ ] Replays

---

*Last updated: [Current conversation date]*
