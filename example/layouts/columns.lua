hl.layout.register("columns", {
    recalculate = function(ctx)
        local n = #ctx.targets
        if n == 0 then
            return
        end

        for i, target in ipairs(ctx.targets) do
            target:place(ctx:column(i, n))
        end
    end,
})
