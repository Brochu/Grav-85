#include <unordered_set>
#include <queue>

#define SOLVER_MAX_MOVES 64
#define SOLVER_DEFAULT_DEPTH 15
#define SOLVER_DEFAULT_MAX_STATES 2000000

struct solve_result {
    bool solvable;
    i32 optimal_moves;
    i32 states_explored;
    direction solution[SOLVER_MAX_MOVES];
};

struct solver_node {
    sim_state state;
    i32 depth;
    direction moves[SOLVER_MAX_MOVES];
};

static u64 sim_state_hash(sim_state *s) {
    // FNV-1a over sorted positions + gems_active + gravity
    u64 h = 14695981039346656037ull;
    auto fnv_byte = [&](u8 b) { h ^= b; h *= 1099511628211ull; };
    auto fnv_i32 = [&](i32 v) {
        fnv_byte((u8)(v));
        fnv_byte((u8)(v >> 8));
        fnv_byte((u8)(v >> 16));
        fnv_byte((u8)(v >> 24));
    };

    // Hash crate positions (sorted for canonical form)
    ivec2 sorted_crates[ELEMENTS_MAX_NUM];
    memcpy(sorted_crates, s->crates, sizeof(ivec2) * s->num_crates);
    for (i32 i = 1; i < s->num_crates; i++) {
        ivec2 key = sorted_crates[i];
        i32 kv = key.y * 16 + key.x;
        i32 j = i - 1;
        while (j >= 0 && (sorted_crates[j].y * 16 + sorted_crates[j].x) > kv) {
            sorted_crates[j + 1] = sorted_crates[j];
            j--;
        }
        sorted_crates[j + 1] = key;
    }
    for (i32 i = 0; i < s->num_crates; i++) {
        fnv_i32(sorted_crates[i].x);
        fnv_i32(sorted_crates[i].y);
    }

    // Hash active gem positions (sorted)
    ivec2 sorted_gems[ELEMENTS_MAX_NUM];
    color sorted_colors[ELEMENTS_MAX_NUM];
    i32 num_active = 0;
    for (i32 i = 0; i < s->num_gems; i++) {
        if ((s->gems_active >> i) & 1) {
            sorted_gems[num_active] = s->gems[i];
            sorted_colors[num_active] = s->gem_colors[i];
            num_active++;
        }
    }
    // Sort by position
    for (i32 i = 1; i < num_active; i++) {
        ivec2 kp = sorted_gems[i];
        color kc = sorted_colors[i];
        i32 kv = kp.y * 16 + kp.x;
        i32 j = i - 1;
        while (j >= 0 && (sorted_gems[j].y * 16 + sorted_gems[j].x) > kv) {
            sorted_gems[j + 1] = sorted_gems[j];
            sorted_colors[j + 1] = sorted_colors[j];
            j--;
        }
        sorted_gems[j + 1] = kp;
        sorted_colors[j + 1] = kc;
    }
    for (i32 i = 0; i < num_active; i++) {
        fnv_i32(sorted_gems[i].x);
        fnv_i32(sorted_gems[i].y);
        fnv_byte((u8)sorted_colors[i]);
    }

    fnv_i32((i32)s->gems_active);
    fnv_byte((u8)s->current_gravity);

    return h;
}

solve_result solver_solve(level *lvl, i32 max_depth, i32 max_states) {
    solve_result result = {};

    sim_state start;
    sim_init(&start, lvl);

    if (sim_is_solved(&start)) {
        result.solvable = true;
        result.optimal_moves = 0;
        result.states_explored = 1;
        return result;
    }

    std::unordered_set<u64> visited;
    std::queue<solver_node> frontier;

    solver_node root = {};
    root.state = start;
    root.depth = 0;

    visited.insert(sim_state_hash(&root.state));
    frontier.push(root);

    while (!frontier.empty()) {
        if ((i32)visited.size() >= max_states) break;

        solver_node node = frontier.front();
        frontier.pop();
        result.states_explored++;

        if (node.depth >= max_depth) continue;

        for (i32 d = 0; d < 4; d++) {
            direction dir = (direction)d;
            if (dir == node.state.current_gravity) continue;

            sim_state next = node.state;
            sim_apply_move(&next, lvl, dir);

            u64 hash = sim_state_hash(&next);
            if (visited.count(hash)) continue;
            visited.insert(hash);

            solver_node child = {};
            child.state = next;
            child.depth = node.depth + 1;
            memcpy(child.moves, node.moves, sizeof(direction) * node.depth);
            child.moves[node.depth] = dir;

            if (sim_is_solved(&next)) {
                result.solvable = true;
                result.optimal_moves = child.depth;
                result.states_explored++;
                memcpy(result.solution, child.moves, sizeof(direction) * child.depth);
                return result;
            }

            frontier.push(child);
        }
    }

    return result;
}
