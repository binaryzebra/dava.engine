local ipField = nil
local portField = nil
local tokenField = nil


function activate(context, view)
    ipField = view.FindByPath("**/IpField/Field")
    ipField.text = "34.241.135.221"

    portField = view.FindByPath("**/PortField/Field")
    portField.text = "31000"

    tokenField = view.FindByPath("**/TokenField/Field")
    tokenField.text = context.data.clientOptions.token
end

function deactivate(context, view)
    context.data.clientOptions.hostName = ipField.text
    context.data.clientOptions.port = tonumber(portField.text)
    context.data.clientOptions.token = tokenField.text
end

function processEvent(event)
	if (event == "aws") then
		ipField.text = "34.241.135.221"
		portField.text = "31000"
		return true
	elseif (event == "localhost") then
		ipField.text = "localhost"
		portField.text = "9000"
		return true
	end

	return false
end
