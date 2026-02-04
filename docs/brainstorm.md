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
    * EMPTY - invisible space, crates and gems pass through
    * SOLID - wall space, renders, will never move
    * CRATE - wall type, renders, moves based on gravity changes, crates combos
    * GEM   - gem, renders, vfx, moves based on gravity chances, has a color, will combo with same color in 4 directions (top, left, down, right)
- Data Types
    * enum for spaces (i8)
    * enum for gem colors (limit to 3 colors MAX) (i8); red, green, blue
    * enum for directions (i8); up, right, down, left; can also be packed to i2
    * since we have 4 space types and 3 colors, we can store both using int2 to pack data the best we can
- Data format on disk
    * dimensions: width, height (i8, i8)
    * starting gravity direction
    * colors array: size (i8) | i2 array, max size 128, represents the colors of the gem, order from spaces array
    * spaces array: i2 array, max size 128x128, will never reach even close to this
- Level Filesize (in mem)
    * dims -> 2xi8 -> 2bytes
    * start_gravity -> i8 -> 1byte
    * colors len -> i8 -> 1 byte
    * colors arr -> max 32 -> i2 each -> 8bytes
    * spaces arr -> max 32x32 -> i2 each -> 16bytes
    * total = 2 + 1 + 1 + 8 + 16 -> 28 bytes

### Demo Level

```
6,6 (dims)
2 (start_gravity)
3|G,G,G (colors: len | list)
# # # # # #
# 0 X . . #
# . . . . #
# 1 . . . #
# . . 2 # #
# # # # # #

11111110 00011000 01100001 10001111 1111
```

- u8  -> packed width/height
- u8  -> start gravity
- u8  -> num crates
- u8  -> num gems
- u64 -> colors data
- u8  -> x32 crates starts
- u8  -> x32 gem starts
- u8  -> x32 solid map - 256 bits

## Gameplay

- De-couple elements positions w/ visuals to better handle animations
- Each time elements all stop moving we need to check for combos
    * handle chains
