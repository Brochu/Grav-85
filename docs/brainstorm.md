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
    * EMPTY - invisible space
    * SOLID - wall space, renders, will never move
    * BLOCK - wall type, renders, moves based on gravity changes, blocks combos
    * ORB   - orbs, renders, vfx, moves based on gravity chances, has a color, will combo with same color in 4 directions (top, left, down, right)
- FORMAT
- DATA TYPES

## Gameplay

- De-couple elements positions w/ visuals to better handle animations
- Each time elements all stop moving we need to check for combos
    * handle chains
