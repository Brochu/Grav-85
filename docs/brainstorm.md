# Grav-85: brainstorm

## Networking / Multiplayer ideas

- client/server arch 100%
- client should be able to play fully local and connected to a server
    * TODO: define finite state machine of all possible states for the client
    * better to keep a flat FSM, easier to debug
- server should be separate application that we can run on a server box in the cloud
    * should handle main server, initial connections, create sub-servers to handle specific on-going games
    * on-going games match max 8 people each other, gives them the levels to play in order
    * needs to validate players moves, completion times
    * broadcast other player's progress across

### Client FSM definition

## Assets Handling

- want to be able to bundle assets into logical binary packages
    * should make updates more reasonable
- engine should handle all loading/unloading
- different memory arenas based on asset lifetimes

## Levels Structure

- Levels is a 2d grid with 4 different types of elements
    * EMPTY - invisible space, blocks and orbs pass through
    * SOLID - wall space, renders, will never move
    * BLOCK - wall type, renders, moves based on gravity changes, blocks combos
    * STONE - gemstones, renders, vfx, moves based on gravity chances, has a color, will combo with same color in 4 directions (top, left, down, right)
- Data Types
    * enum for spaces
    * enum for stone colors (limit to 3 colors MAX)
    * since we have 4 space types and 3 colors, we can store both using int2 to pack data the best we can
- Data format on disk
    * dimensions: width, height (i8, i8)
    * colors array: size (i8) | i2 array, max size 128, represents the colors of the stone, order from spaces array
    * spaces array: i2 array, max size 128x128, will never reach even close to this

### Demo Level

```
6,6 (dims)
3|G,G,G (colors: len | list)
# # # # # #
# 0 X     #
#         #
# 1       #
#     2 # #
# # # # # #
```

## Gameplay

- De-couple elements positions w/ visuals to better handle animations
- Each time elements all stop moving we need to check for combos
    * handle chains
