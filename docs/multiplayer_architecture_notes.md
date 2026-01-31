# Multiplayer Architecture Notes for Puzzle Racing Game

## Game Context

Gravnic-style competitive puzzle game where players race to solve identical levels. Match formats:
- **Best of N**: Individual races per level, winner takes majority
- **First to N**: Continuous race across levels, first to complete N wins
- **Battle Royale**: Free-for-all lobbies (Tetris 99 style), last standing or first to N

---

## Why This Game Type is Network-Friendly

Very low bandwidth requirements:
- **Input**: 4 possible moves (2 bits) + timestamp
- **State sync**: Only broadcast "player X completed level Y in Z moves/time"
- **No real-time physics sync**: Each player simulates locally with identical levels (seeded generation)

A single VPS can handle hundreds of concurrent players.

---

## Architecture Options

### Option 1: Single Authoritative Server (Recommended Start)

```
[Player A] ──┐
[Player B] ──┼──► [Your Server] ──► [Database]
[Player C] ──┘         │
                       ▼
                 Match State
                 Leaderboards
                 Level Seeds
```

**Flow:**
1. Server generates match (level seed + player list)
2. Clients receive seed, generate identical levels locally
3. Clients send: "I completed level N" with solution proof (move sequence)
4. Server validates solution (replays moves), broadcasts standings
5. First to N wins

**Anti-cheat**: Client sends move sequence, server replays to confirm. Minimal bandwidth, prevents cheating.

**Tech stack suggestions:**
- TCP or WebSocket server
- Single process handles all connections
- SQLite or PostgreSQL for persistence
- C++ (consistency with client), Go, or Rust

### Option 2: Steam Networking (Good Middle Ground)

Steam provides:
- **Steamworks Matchmaking**: Lobby system, skill-based matching
- **Steam Datagram Relay**: NAT traversal, relay servers worldwide
- **No server costs**: Peer-to-peer with Steam relay

```
[Player A] ◄──Steam Relay──► [Player B]
     │                            │
     └────► [Steam Backend] ◄─────┘
              (Matchmaking)
```

**Implementation:**
- One player acts as "host" (generates seed, validates)
- Or use Steam's Game Server SDK for dedicated servers
- Leaderboards via Steam Stats & Achievements

**Downside**: Steam-only (no cross-platform with console/mobile)

### Option 3: Hybrid (Scale Later)

Start with Option 1, then:
- Add regional servers when latency matters
- Use message queue (Redis pub/sub) for cross-server coordination
- Kubernetes/containers for scaling match servers

Likely unnecessary for puzzle games with discrete moves.

---

## Server-Side Validation

Critical for anti-cheat:

```cpp
struct CompletionMessage {
    uint32_t level_index;
    uint32_t move_count;
    std::vector<Direction> moves;  // The actual solution
    uint64_t timestamp;
};

bool validate_solution(const Level& level, const CompletionMessage& msg) {
    // Replay the moves on server
    GameState state = level.initial_state();
    
    for (Direction dir : msg.moves) {
        state.apply_gravity(dir);
        state.resolve_matches();
    }
    
    return state.is_cleared();
}
```

---

## Local Testing Strategies

### Bot Clients for Load Testing

```cpp
class BotClient {
    void connect(const char* server_addr);
    void join_match();
    void play() {
        while (!match_complete) {
            // Simulate human-ish solve time
            sleep_ms(random(500, 2000));
            
            Level& lvl = current_level();
            auto solution = solve(lvl);  // Your solver from generation
            send_completion(solution);
        }
    }
};

// Test harness
int main() {
    Server server;
    server.start(8080);
    
    // Spawn 10 bot clients
    std::vector<BotClient> bots(10);
    for (auto& bot : bots) {
        bot.connect("localhost:8080");
        bot.join_match();
    }
    
    server.start_match();
    for (auto& bot : bots) bot.play();
}
```

### Docker Compose for Realistic Testing

```yaml
version: '3'
services:
  server:
    build: ./server
    ports:
      - "8080:8080"
  
  bot1:
    build: ./bot
    environment:
      - SERVER_ADDR=server:8080
      - SKILL_LEVEL=beginner
  
  bot2:
    build: ./bot
    environment:
      - SERVER_ADDR=server:8080
      - SKILL_LEVEL=expert
  
  # Scale: docker-compose up --scale bot1=50
```

### Network Condition Simulation

```bash
# Linux: add artificial latency
tc qdisc add dev lo root netem delay 100ms 20ms

# Or use toxiproxy
toxiproxy-cli toxic add -t latency -a latency=100 game_server
```

---

## Matchmaking Implementation

### 1v1 Matchmaking

```cpp
struct MatchmakingQueue {
    struct Entry {
        PlayerId player;
        int skill_rating;
        time_t queued_at;
    };
    
    std::vector<Entry> waiting;
    
    void try_match() {
        // Sort by queue time
        // Find pairs within skill range
        // Expand skill range over time (avoid infinite waits)
        
        for (auto& a : waiting) {
            // Skill tolerance expands the longer you wait
            int max_diff = 100 + (now() - a.queued_at) * 10;
            
            for (auto& b : waiting) {
                if (&a == &b) continue;
                if (abs(a.skill_rating - b.skill_rating) < max_diff) {
                    create_match(a.player, b.player);
                    remove_from_queue(a, b);
                    return;
                }
            }
        }
    }
};
```

### Battle Royale (N Players)

- Fill lobbies up to max (e.g., 99 players)
- Start when full OR after timeout with minimum players
- Backfill with bots if needed for consistent experience

---

## Skill Rating (Elo-like System)

```cpp
void update_ratings(Player& winner, Player& loser) {
    const float K = 32.0f;  // Adjustment factor
    
    float expected_winner = 1.0f / (1.0f + pow(10, (loser.rating - winner.rating) / 400.0f));
    float expected_loser = 1.0f - expected_winner;
    
    winner.rating += K * (1.0f - expected_winner);
    loser.rating += K * (0.0f - expected_loser);
}
```

---

## Recommended Development Path

1. **Start with single server** (WebSocket-based)
2. **Validate solutions server-side** (replay move sequences)
3. **Use seeded level generation** (all clients produce identical levels)
4. **Test locally with bot clients** (auto-solve at varying speeds)
5. **Add Steam integration later** (matchmaking/relay if needed)

The puzzle game format is forgiving—no tick-perfect synchronization needed. Focus on game feel first, scale backend when you have players.

---

## Key Bandwidth Estimates

| Event | Size | Frequency |
|-------|------|-----------|
| Player input (1 move) | ~4 bytes | Per move (~1-2/sec during solve) |
| Level completion | ~50-100 bytes | Per level solved |
| Match state update | ~200 bytes | Per completion (broadcast) |
| Initial match setup | ~500 bytes | Once per match |

For 100 concurrent matches with 2 players each: < 1 Mbps total bandwidth

---

*Last updated: [Current conversation date]*
