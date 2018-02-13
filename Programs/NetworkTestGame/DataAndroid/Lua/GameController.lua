local engine = nil

function init(context)
    local engineService = context.GetService("UIFlowEngineService")
    engine = engineService.engine
end

function processEvent(event)
    if event == "EXIT" then
        engine.quitAsync(0)
        return true
    end
    return false
end