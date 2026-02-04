package lvledit

import rl "vendor:raylib"
import "core:fmt"
import "core:os"
import "core:strings"
import "core:mem"

// Level format: 108 bytes total
// - byte 0:     packed width/height (high nibble = width, low nibble = height)
// - byte 1:     start gravity (0=up, 1=right, 2=down, 3=left)
// - byte 2:     num crates
// - byte 3:     num gems
// - bytes 4-11: colors (u64, 2 bits per gem: 0=red, 1=green, 2=blue)
// - bytes 12-43: 32 crate positions (each u8 = high nibble x, low nibble y)
// - bytes 44-75: 32 gem positions
// - bytes 76-107: solid bitmap (256 bits for 16x16)

LEVEL_SIZE :: 108
MAX_CRATES :: 32
MAX_GEMS   :: 32
MAX_DIM    :: 16

Cell_Type :: enum u8 {
    EMPTY = 0,
    SOLID = 1,
    CRATE = 2,
    GEM   = 3,
}

Gem_Color :: enum u8 {
    RED   = 0,
    GREEN = 1,
    BLUE  = 2,
}

Direction :: enum u8 {
    UP    = 0,
    RIGHT = 1,
    DOWN  = 2,
    LEFT  = 3,
}

Level :: struct {
    width:         u8,
    height:        u8,
    gravity:       Direction,
    num_crates:    u8,
    num_gems:      u8,
    gem_colors:    [MAX_GEMS]Gem_Color,
    crate_pos:     [MAX_CRATES][2]u8, // x, y
    gem_pos:       [MAX_GEMS][2]u8,
    solid:         [MAX_DIM][MAX_DIM]bool,
}

Editor :: struct {
    level:           Level,
    current_tool:    Cell_Type,
    current_color:   Gem_Color,
    cell_size:       i32,
    grid_offset_x:   i32,
    grid_offset_y:   i32,
    filepath:        string,
    status_msg:      string,
    show_help:       bool,
}

main :: proc() {
    rl.InitWindow(800, 600, "Grav-85 Level Editor")
    defer rl.CloseWindow()
    rl.SetTargetFPS(60)

    ed := Editor{
        cell_size     = 32,
        grid_offset_x = 200,
        grid_offset_y = 50,
        current_tool  = .SOLID,
        current_color = .GREEN,
        status_msg    = "New level - Press H for help",
    }

    // Default new level
    ed.level.width  = 6
    ed.level.height = 6
    ed.level.gravity = .DOWN

    // Load from command line if provided
    args := os.args
    if len(args) > 1 {
        ed.filepath = args[1]
        if load_level(&ed.level, ed.filepath) {
            ed.status_msg = fmt.tprintf("Loaded: %s", ed.filepath)
        } else {
            ed.status_msg = fmt.tprintf("Failed to load: %s", ed.filepath)
        }
    }

    for !rl.WindowShouldClose() {
        update(&ed)
        draw(&ed)
    }
}

update :: proc(ed: ^Editor) {
    // Tool selection
    if rl.IsKeyPressed(.ONE)   { ed.current_tool = .EMPTY; ed.status_msg = "Tool: EMPTY" }
    if rl.IsKeyPressed(.TWO)   { ed.current_tool = .SOLID; ed.status_msg = "Tool: SOLID" }
    if rl.IsKeyPressed(.THREE) { ed.current_tool = .CRATE; ed.status_msg = "Tool: CRATE" }
    if rl.IsKeyPressed(.FOUR)  { ed.current_tool = .GEM;   ed.status_msg = "Tool: GEM" }

    // Color selection for gems
    if rl.IsKeyPressed(.R) { ed.current_color = .RED;   ed.status_msg = "Gem color: RED" }
    if rl.IsKeyPressed(.G) { ed.current_color = .GREEN; ed.status_msg = "Gem color: GREEN" }
    if rl.IsKeyPressed(.B) { ed.current_color = .BLUE;  ed.status_msg = "Gem color: BLUE" }

    // Gravity direction
    if rl.IsKeyPressed(.UP)    { ed.level.gravity = .UP;    ed.status_msg = "Gravity: UP" }
    if rl.IsKeyPressed(.RIGHT) { ed.level.gravity = .RIGHT; ed.status_msg = "Gravity: RIGHT" }
    if rl.IsKeyPressed(.DOWN)  { ed.level.gravity = .DOWN;  ed.status_msg = "Gravity: DOWN" }
    if rl.IsKeyPressed(.LEFT)  { ed.level.gravity = .LEFT;  ed.status_msg = "Gravity: LEFT" }

    // Resize level
    if rl.IsKeyPressed(.KP_ADD) || rl.IsKeyPressed(.EQUAL) {
        if rl.IsKeyDown(.LEFT_SHIFT) {
            if ed.level.height < MAX_DIM { ed.level.height += 1 }
        } else {
            if ed.level.width < MAX_DIM { ed.level.width += 1 }
        }
        ed.status_msg = fmt.tprintf("Size: %dx%d", ed.level.width, ed.level.height)
    }
    if rl.IsKeyPressed(.KP_SUBTRACT) || rl.IsKeyPressed(.MINUS) {
        if rl.IsKeyDown(.LEFT_SHIFT) {
            if ed.level.height > 1 { ed.level.height -= 1 }
        } else {
            if ed.level.width > 1 { ed.level.width -= 1 }
        }
        ed.status_msg = fmt.tprintf("Size: %dx%d", ed.level.width, ed.level.height)
    }

    // Help toggle
    if rl.IsKeyPressed(.H) { ed.show_help = !ed.show_help }

    // Save/Load
    if rl.IsKeyPressed(.S) && rl.IsKeyDown(.LEFT_CONTROL) {
        path := ed.filepath if len(ed.filepath) > 0 else "level.bin"
        if save_level(&ed.level, path) {
            ed.status_msg = fmt.tprintf("Saved: %s", path)
        } else {
            ed.status_msg = "Save failed!"
        }
    }

    // Mouse editing
    if rl.IsMouseButtonDown(.LEFT) || rl.IsMouseButtonDown(.RIGHT) {
        mx := rl.GetMouseX()
        my := rl.GetMouseY()
        gx := (mx - ed.grid_offset_x) / ed.cell_size
        gy := (my - ed.grid_offset_y) / ed.cell_size

        if gx >= 0 && gx < i32(ed.level.width) && gy >= 0 && gy < i32(ed.level.height) {
            if rl.IsMouseButtonDown(.RIGHT) {
                // Right click: erase
                place_cell(ed, u8(gx), u8(gy), .EMPTY, ed.current_color)
            } else {
                // Left click: place current tool
                place_cell(ed, u8(gx), u8(gy), ed.current_tool, ed.current_color)
            }
        }
    }
}

place_cell :: proc(ed: ^Editor, x, y: u8, cell_type: Cell_Type, color: Gem_Color) {
    // First, remove anything at this position
    remove_at(ed, x, y)

    switch cell_type {
    case .EMPTY:
        // Already cleared
    case .SOLID:
        ed.level.solid[y][x] = true
    case .CRATE:
        if ed.level.num_crates < MAX_CRATES {
            ed.level.crate_pos[ed.level.num_crates] = {x, y}
            ed.level.num_crates += 1
        }
    case .GEM:
        if ed.level.num_gems < MAX_GEMS {
            ed.level.gem_pos[ed.level.num_gems] = {x, y}
            ed.level.gem_colors[ed.level.num_gems] = color
            ed.level.num_gems += 1
        }
    }
}

remove_at :: proc(ed: ^Editor, x, y: u8) {
    // Clear solid
    ed.level.solid[y][x] = false

    // Remove crate if present
    for i in 0..<ed.level.num_crates {
        if ed.level.crate_pos[i][0] == x && ed.level.crate_pos[i][1] == y {
            // Shift remaining crates
            for j in i..<ed.level.num_crates-1 {
                ed.level.crate_pos[j] = ed.level.crate_pos[j+1]
            }
            ed.level.num_crates -= 1
            break
        }
    }

    // Remove gem if present
    for i in 0..<ed.level.num_gems {
        if ed.level.gem_pos[i][0] == x && ed.level.gem_pos[i][1] == y {
            // Shift remaining gems and colors
            for j in i..<ed.level.num_gems-1 {
                ed.level.gem_pos[j] = ed.level.gem_pos[j+1]
                ed.level.gem_colors[j] = ed.level.gem_colors[j+1]
            }
            ed.level.num_gems -= 1
            break
        }
    }
}

draw :: proc(ed: ^Editor) {
    rl.BeginDrawing()
    defer rl.EndDrawing()

    rl.ClearBackground({30, 30, 40, 255})

    // Draw grid
    for y in 0..<ed.level.height {
        for x in 0..<ed.level.width {
            rx := ed.grid_offset_x + i32(x) * ed.cell_size
            ry := ed.grid_offset_y + i32(y) * ed.cell_size

            // Cell background
            rl.DrawRectangle(rx, ry, ed.cell_size - 1, ed.cell_size - 1, {50, 50, 60, 255})

            // Draw solid
            if ed.level.solid[y][x] {
                rl.DrawRectangle(rx + 2, ry + 2, ed.cell_size - 5, ed.cell_size - 5, {100, 100, 110, 255})
            }
        }
    }

    // Draw crates
    for i in 0..<ed.level.num_crates {
        x := ed.level.crate_pos[i][0]
        y := ed.level.crate_pos[i][1]
        rx := ed.grid_offset_x + i32(x) * ed.cell_size
        ry := ed.grid_offset_y + i32(y) * ed.cell_size
        rl.DrawRectangle(rx + 4, ry + 4, ed.cell_size - 9, ed.cell_size - 9, {180, 140, 80, 255})
        rl.DrawText("X", rx + 10, ry + 8, 16, {40, 30, 20, 255})
    }

    // Draw gems
    for i in 0..<ed.level.num_gems {
        x := ed.level.gem_pos[i][0]
        y := ed.level.gem_pos[i][1]
        rx := ed.grid_offset_x + i32(x) * ed.cell_size
        ry := ed.grid_offset_y + i32(y) * ed.cell_size
        cx := rx + ed.cell_size / 2
        cy := ry + ed.cell_size / 2

        color: rl.Color
        switch ed.level.gem_colors[i] {
        case .RED:   color = {220, 60, 60, 255}
        case .GREEN: color = {60, 220, 60, 255}
        case .BLUE:  color = {60, 100, 220, 255}
        }
        rl.DrawCircle(cx, cy, f32(ed.cell_size) / 3, color)
    }

    // Draw gravity indicator
    gx := ed.grid_offset_x + i32(ed.level.width) * ed.cell_size / 2
    gy := ed.grid_offset_y + i32(ed.level.height) * ed.cell_size / 2
    arrow_len: i32 = 40
    switch ed.level.gravity {
    case .UP:    rl.DrawLine(gx, gy, gx, gy - arrow_len, rl.YELLOW)
    case .DOWN:  rl.DrawLine(gx, gy, gx, gy + arrow_len, rl.YELLOW)
    case .LEFT:  rl.DrawLine(gx, gy, gx - arrow_len, gy, rl.YELLOW)
    case .RIGHT: rl.DrawLine(gx, gy, gx + arrow_len, gy, rl.YELLOW)
    }

    // Draw UI panel
    rl.DrawRectangle(0, 0, 190, 600, {40, 40, 50, 255})

    rl.DrawText("GRAV-85 LEVEL EDIT", 10, 10, 14, rl.WHITE)
    rl.DrawText("----------------", 10, 26, 14, rl.GRAY)

    rl.DrawText(fmt.ctprintf("Size: %dx%d", ed.level.width, ed.level.height), 10, 50, 14, rl.WHITE)
    rl.DrawText(fmt.ctprintf("Gravity: %v", ed.level.gravity), 10, 70, 14, rl.YELLOW)
    rl.DrawText(fmt.ctprintf("Crates: %d", ed.level.num_crates), 10, 90, 14, rl.WHITE)
    rl.DrawText(fmt.ctprintf("Gems: %d", ed.level.num_gems), 10, 110, 14, rl.WHITE)

    rl.DrawText("----------------", 10, 136, 14, rl.GRAY)
    rl.DrawText("Current Tool:", 10, 156, 14, rl.WHITE)

    tool_color: rl.Color = rl.WHITE
    switch ed.current_tool {
    case .EMPTY: tool_color = rl.GRAY
    case .SOLID: tool_color = {100, 100, 110, 255}
    case .CRATE: tool_color = {180, 140, 80, 255}
    case .GEM:
        switch ed.current_color {
        case .RED:   tool_color = {220, 60, 60, 255}
        case .GREEN: tool_color = {60, 220, 60, 255}
        case .BLUE:  tool_color = {60, 100, 220, 255}
        }
    }
    rl.DrawRectangle(10, 176, 24, 24, tool_color)
    rl.DrawText(fmt.ctprintf("%v", ed.current_tool), 40, 180, 14, rl.WHITE)

    if ed.current_tool == .GEM {
        rl.DrawText(fmt.ctprintf("Color: %v", ed.current_color), 10, 210, 14, rl.WHITE)
    }

    // Status bar
    rl.DrawRectangle(0, 580, 800, 20, {20, 20, 30, 255})
    rl.DrawText(fmt.ctprintf("%s", ed.status_msg), 10, 583, 14, rl.WHITE)

    // Help overlay
    if ed.show_help {
        rl.DrawRectangle(150, 100, 500, 380, {20, 20, 30, 240})
        rl.DrawText("CONTROLS", 170, 120, 20, rl.WHITE)
        rl.DrawText("1 - Empty tool", 170, 160, 14, rl.WHITE)
        rl.DrawText("2 - Solid tool", 170, 180, 14, rl.WHITE)
        rl.DrawText("3 - Crate tool", 170, 200, 14, rl.WHITE)
        rl.DrawText("4 - Gem tool", 170, 220, 14, rl.WHITE)
        rl.DrawText("R/G/B - Gem color (Red/Green/Blue)", 170, 250, 14, rl.WHITE)
        rl.DrawText("Arrow keys - Set gravity direction", 170, 280, 14, rl.WHITE)
        rl.DrawText("+/- - Resize width (Shift for height)", 170, 310, 14, rl.WHITE)
        rl.DrawText("Left click - Place", 170, 340, 14, rl.WHITE)
        rl.DrawText("Right click - Erase", 170, 360, 14, rl.WHITE)
        rl.DrawText("Ctrl+S - Save", 170, 390, 14, rl.WHITE)
        rl.DrawText("H - Toggle this help", 170, 420, 14, rl.WHITE)
        rl.DrawText("ESC - Quit", 170, 450, 14, rl.WHITE)
    }
}

load_level :: proc(level: ^Level, filepath: string) -> bool {
    data, ok := os.read_entire_file(filepath)
    if !ok || len(data) != LEVEL_SIZE {
        return false
    }
    defer delete(data)

    // Reset level
    level^ = {}

    // Parse header
    level.width  = (data[0] >> 4) & 0x0F
    level.height = data[0] & 0x0F
    level.gravity = Direction(data[1])
    level.num_crates = data[2]
    level.num_gems = data[3]

    // Parse colors (u64 at bytes 4-11, 2 bits per gem)
    colors_u64: u64 = 0
    for i in 0..<8 {
        colors_u64 |= u64(data[4 + i]) << (u64(i) * 8)
    }
    for i in 0..<level.num_gems {
        level.gem_colors[i] = Gem_Color((colors_u64 >> (u64(i) * 2)) & 0x3)
    }

    // Parse crate positions (bytes 12-43)
    for i in 0..<level.num_crates {
        packed := data[12 + i]
        level.crate_pos[i][0] = (packed >> 4) & 0x0F
        level.crate_pos[i][1] = packed & 0x0F
    }

    // Parse gem positions (bytes 44-75)
    for i in 0..<level.num_gems {
        packed := data[44 + i]
        level.gem_pos[i][0] = (packed >> 4) & 0x0F
        level.gem_pos[i][1] = packed & 0x0F
    }

    // Parse solid bitmap (bytes 76-107) - packed by actual level dimensions
    w := int(level.width)
    h := int(level.height)
    for y in 0..<h {
        for x in 0..<w {
            cell_idx := y * w + x
            byte_idx := cell_idx / 8
            bit_idx := u8(cell_idx % 8)
            if (data[76 + byte_idx] & (u8(1) << bit_idx)) != 0 {
                level.solid[y][x] = true
            }
        }
    }

    return true
}

save_level :: proc(level: ^Level, filepath: string) -> bool {
    data: [LEVEL_SIZE]u8

    // Header
    data[0] = (level.width << 4) | level.height
    data[1] = u8(level.gravity)
    data[2] = level.num_crates
    data[3] = level.num_gems

    // Colors (u64, 2 bits per gem)
    colors_u64: u64 = 0
    for i in 0..<level.num_gems {
        colors_u64 |= u64(level.gem_colors[i]) << (u64(i) * 2)
    }
    for i in 0..<8 {
        data[4 + i] = u8((colors_u64 >> (u64(i) * 8)) & 0xFF)
    }

    // Crate positions
    for i in 0..<level.num_crates {
        data[12 + i] = (level.crate_pos[i][0] << 4) | level.crate_pos[i][1]
    }

    // Gem positions
    for i in 0..<level.num_gems {
        data[44 + i] = (level.gem_pos[i][0] << 4) | level.gem_pos[i][1]
    }

    // Solid bitmap - packed by actual level dimensions
    w := int(level.width)
    h := int(level.height)
    for y in 0..<h {
        for x in 0..<w {
            if level.solid[y][x] {
                cell_idx := y * w + x
                byte_idx := cell_idx / 8
                bit_idx := cell_idx % 8
                data[76 + byte_idx] |= u8(1) << u8(bit_idx)
            }
        }
    }

    return os.write_entire_file(filepath, data[:])
}
