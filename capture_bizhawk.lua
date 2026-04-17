-- BizHawk Lua script to auto-capture reference screenshots at specific frames.
-- Runs in EmuHawk after loading "SNES Test Program.sfc".
-- Save screenshots to output directory.

local outDir = "C:\\Users\\GH\\Desktop\\snes_emu\\tests\\snes9x_ref\\"

-- Frames at which to capture (aligned with emulator test timeline).
-- Each test card typically shows for ~50-80 frames in Electronics Test.
local captures = {
    [60]   = "tc01_menu",
    [120]  = "tc02_bgmode0",
    [200]  = "tc03_smw",
    [280]  = "tc04_princess",
    [360]  = "tc05_windows",
    [440]  = "tc06_colormath",
    [520]  = "tc07_stars",
    [600]  = "tc08_marios",
    [680]  = "tc09_mario_rot",
    [760]  = "tc10_mario_flip",
    [840]  = "tc11_princess_zoom",
    [920]  = "tc12_passed",
    [1200] = "tc13_color_test",
}

-- Auto-input schedule: frame -> button mask
local inputs = {
    [90]   = { ["Start"] = true },
    [92]   = { ["Start"] = false },
    [1000] = { ["Select"] = true },
    [1002] = { ["Select"] = false },
    [1050] = { ["Select"] = true },  -- cycle to item 5
    [1052] = { ["Select"] = false },
    [1070] = { ["Select"] = true },
    [1072] = { ["Select"] = false },
    [1090] = { ["Select"] = true },
    [1092] = { ["Select"] = false },
    [1110] = { ["Select"] = true },
    [1112] = { ["Select"] = false },
    [1150] = { ["Start"] = true },
    [1152] = { ["Start"] = false },
}

local function applyInput(f)
    if inputs[f] then
        local buttons = {}
        for btn, press in pairs(inputs[f]) do
            buttons[btn] = press
        end
        joypad.set(buttons, 1)
    end
end

while true do
    local f = emu.framecount()
    applyInput(f)
    if captures[f] then
        local path = outDir .. captures[f] .. ".png"
        client.screenshot(path)
        console.log("[CAPTURE] frame " .. f .. " -> " .. captures[f] .. ".png")
    end
    if f > 1300 then
        console.log("[DONE] captures complete at frame " .. f)
        break
    end
    emu.frameadvance()
end
