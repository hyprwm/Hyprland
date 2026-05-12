local state = {
    order = {},
    split = {},
    default_split = "h",
}

local function target_id(target)
    local window = target.window
    return window and tostring(window.stable_id) or tostring(target.index)
end

local function active_id(ctx)
    for _, target in ipairs(ctx.targets) do
        local window = target.window
        if window and window.active then
            return target_id(target)
        end
    end

    return state.order[#state.order]
end

local function index_of(tbl, value)
    for i, v in ipairs(tbl) do
        if v == value then
            return i
        end
    end
end

local function sync_order(ctx)
    local present = {}
    local targets = {}

    for _, target in ipairs(ctx.targets) do
        local id = target_id(target)
        present[id] = true
        targets[id] = target
    end

    local old_order = state.order
    state.order = {}

    for _, id in ipairs(old_order) do
        if present[id] then
            table.insert(state.order, id)
        else
            state.split[id] = nil
        end
    end

    local focused = active_id(ctx)
    for _, target in ipairs(ctx.targets) do
        local id = target_id(target)
        if not index_of(state.order, id) then
            local after = focused and index_of(state.order, focused)
            table.insert(state.order, after and (after + 1) or (#state.order + 1), id)
        end
    end

    return targets
end

local function place_chain(ctx, targets, ids, area, i)
    if i > #ids then
        return
    end

    local target = targets[ids[i]]
    if not target then
        return
    end

    if i == #ids then
        target:place(area)
        return
    end

    local split = state.split[ids[i]] or state.default_split
    if split == "v" then
        target:place(ctx:split(area, "top", 0.5))
        place_chain(ctx, targets, ids, ctx:split(area, "bottom", 0.5), i + 1)
    else
        target:place(ctx:split(area, "left", 0.5))
        place_chain(ctx, targets, ids, ctx:split(area, "right", 0.5), i + 1)
    end
end

local function move_active(ctx, delta)
    local id = active_id(ctx)
    local i = id and index_of(state.order, id)
    local j = i and (i + delta)

    if not i or j < 1 or j > #state.order then
        return
    end

    state.order[i], state.order[j] = state.order[j], state.order[i]
end

hl.layout.register("manual", {
    recalculate = function(ctx)
        local targets = sync_order(ctx)
        place_chain(ctx, targets, state.order, ctx.area, 1)
    end,

    layout_msg = function(ctx, msg)
        local id = active_id(ctx)
        local command = msg:match("^(%S+)")

        if command == "splith" or command == "h" then
            if id then
                state.split[id] = "h"
            end
        elseif command == "splitv" or command == "v" then
            if id then
                state.split[id] = "v"
            end
        elseif command == "splittoggle" or command == "toggle" then
            if id then
                state.split[id] = state.split[id] == "v" and "h" or "v"
            end
        elseif command == "promote" then
            local i = id and index_of(state.order, id)
            if i then
                table.remove(state.order, i)
                table.insert(state.order, 1, id)
            end
        elseif command == "swapnext" then
            move_active(ctx, 1)
        elseif command == "swapprev" then
            move_active(ctx, -1)
        elseif command == "rotate" then
            for k, v in pairs(state.split) do
                state.split[k] = v == "v" and "h" or "v"
            end
            state.default_split = state.default_split == "v" and "h" or "v"
        else
            return "manual: expected splith, splitv, splittoggle, promote, swapnext, swapprev, or rotate"
        end

        return true
    end,
})
