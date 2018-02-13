local debugButtonTextComponent = nil
local options = nil
local slowDownFactorField = nil
local freqHzField = nil

function updateText(textComponent, isOn)
	if isOn then
    	textComponent.text = "On"
    else
    	textComponent.text = "Off"
    end
end

function activate(context, view)
	debugButtonTextComponent = view.FindByPath("**/DebugButton").GetComponentByName("UITextComponent", 0)
    options = context.data.clientOptions

    updateText(debugButtonTextComponent, options.isDebug)

    slowDownFactorField = view.FindByPath("**/SlowDownFactorField/Field")
    slowDownFactorField.text = tostring(options.slowDownFactor)

    freqHzField = view.FindByPath("**/FreqHzField/Field")
    freqHzField.text = tostring(options.freqHz)
end

function deactivate(context, view)
    options.freqHz = tonumber(freqHzField.text)
end

function processEvent( eventName )
	if eventName == "DEBUG_SWITCH" then
		options.isDebug = not options.isDebug
		updateText(debugButtonTextComponent, options.isDebug)
		return true
	end
	return false
end
