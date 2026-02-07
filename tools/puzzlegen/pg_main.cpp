#include <cstdio>
#include <cstring>
#include <ctime>
#include <direct.h>
#include <stdlib.h>

#include "shared.hpp"
#include "qg_config.hpp"
#include "qg_random.hpp"

struct cli_args {
    const char *config_path;
    const char *output_dir;
    const char *tier_name;
    i32 num_puzzles;
    i64 seed;
    bool verbose;
};

void cli_parse(cli_args *args, int argc, char **argv) {
    args->config_path = "puzzlegen.cfg";
    args->output_dir = nullptr;
    args->tier_name = nullptr;
    args->num_puzzles = 0;
    args->seed = 0;
    args->verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            args->config_path = argv[++i];
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            args->num_puzzles = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            args->tier_name = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            args->seed = atoll(argv[++i]);
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            args->output_dir = argv[++i];
        } else if (strcmp(argv[i], "-v") == 0) {
            args->verbose = true;
        } else {
            printf("Usage: puzzlegen.exe [options]\n");
            printf("  -c <path>    Config file (default: puzzlegen.cfg)\n");
            printf("  -n <count>   Number of puzzles to generate\n");
            printf("  -t <tier>    Bundle tier: easy|medium|hard|expert\n");
            printf("  -s <seed>    RNG seed (0 = random)\n");
            printf("  -o <dir>     Output directory\n");
            printf("  -v           Verbose output\n");
        }
    }
}

int main(int argc, char **argv) {
    cli_args args;
    cli_parse(&args, argc, argv);

    // Load config
    config cfg = {};
    config_init(&cfg, args.config_path);

    // Apply config defaults for values not set via CLI
    config_value val;
    if (args.num_puzzles == 0) {
        args.num_puzzles = 100;
        if (config_read(&cfg, "num_puzzles", &val)) args.num_puzzles = val.integer;
    }
    if (args.seed == 0) {
        if (config_read(&cfg, "seed", &val) && val.integer != 0) {
            args.seed = val.integer;
        } else {
            args.seed = (i64)time(nullptr);
        }
    }
    if (!args.output_dir) {
        args.output_dir = "bundles";
        if (config_read(&cfg, "output_dir", &val) && val.type == value_type::STRING) {
            args.output_dir = val.str.arr;
        }
    }
    if (!args.tier_name) {
        args.tier_name = "medium";
        if (config_read(&cfg, "bundle_tier", &val) && val.type == value_type::STRING) {
            args.tier_name = val.str.arr;
        }
    }

    i32 max_attempts = 1000;
    if (config_read(&cfg, "max_attempts", &val)) max_attempts = val.integer;

    i32 max_solve_moves = SOLVER_DEFAULT_DEPTH;
    if (config_read(&cfg, "max_solve_moves", &val)) max_solve_moves = val.integer;

    i32 max_visited = SOLVER_DEFAULT_MAX_STATES;
    if (config_read(&cfg, "max_visited_states", &val)) max_visited = val.integer;

    printf("puzzlegen: seed=%lld puzzles=%d tier=%s output=%s\n",
           args.seed, args.num_puzzles, args.tier_name, args.output_dir);

    rand_seed(args.seed);

    // Load generation params
    gen_params gp;
    gen_params_from_config(&gp, &cfg);

    // Load difficulty weights
    difficulty_weights dw;
    difficulty_weights_from_config(&dw, &cfg);

    // Load bundle tier
    bundle_tier tier;
    bundle_tier_from_config(&tier, &cfg, args.tier_name);

    // Generate puzzle pool
    puzzle_entry *pool = (puzzle_entry *)malloc(sizeof(puzzle_entry) * args.num_puzzles);
    i32 pool_count = 0;
    i32 attempts = 0;

    while (pool_count < args.num_puzzles && attempts < max_attempts) {
        attempts++;

        level lvl;
        if (!gen_random_level(&lvl, &gp)) continue;

        solve_result sol = solver_solve(&lvl, max_solve_moves, max_visited);
        if (!sol.solvable) continue;

        f32 diff = difficulty_score(&lvl, &sol, &dw, max_solve_moves);

        pool[pool_count].lvl = lvl;
        pool[pool_count].sol = sol;
        pool[pool_count].difficulty = diff;
        pool_count++;

        if (args.verbose) {
            printf("  [%d/%d] solvable in %d moves, difficulty=%.4f (explored %d states)\n",
                   pool_count, args.num_puzzles, sol.optimal_moves, diff, sol.states_explored);
        }
    }

    printf("Generated %d/%d solvable puzzles in %d attempts\n",
           pool_count, args.num_puzzles, attempts);

    if (pool_count < 5) {
        printf("ERROR: Not enough puzzles for a bundle (need at least 5, got %d)\n", pool_count);
        free(pool);
        config_free(&cfg);
        return 1;
    }

    // Sort and assemble bundles
    pool_sort_by_difficulty(pool, pool_count);

    // Create output directory
    _mkdir(args.output_dir);

    // Assemble as many bundles as we can
    i32 bundles_made = 0;
    i32 pool_offset = 0;

    // Find tier range in sorted pool
    while (true) {
        bundle b = {};
        // Try to assemble from remaining pool
        if (!bundle_assemble(&b, pool + pool_offset, pool_count - pool_offset, &tier)) {
            break;
        }

        char bin_path[256], meta_path[256];
        snprintf(bin_path, sizeof(bin_path), "%s/bundle_%s_%03d.bin",
                 args.output_dir, args.tier_name, bundles_made);
        snprintf(meta_path, sizeof(meta_path), "%s/bundle_%s_%03d.txt",
                 args.output_dir, args.tier_name, bundles_made);

        if (bundle_write(&b, bin_path, meta_path)) {
            printf("Wrote bundle: %s (difficulties: %.2f -> %.2f)\n",
                   bin_path, b.difficulty_scores[0], b.difficulty_scores[4]);
            bundles_made++;
        }

        // Advance past the puzzles we used
        pool_offset += 5;
        if (pool_offset + 5 > pool_count) break;
    }

    printf("Summary: %d bundles written to %s/\n", bundles_made, args.output_dir);

    free(pool);
    config_free(&cfg);
    return 0;
}
