-- Each NPC owns an independent Lua state. NPC_ID is assigned by C++ before
-- this script is loaded.
local last_message_time = 0

function event_player_move(player_id)
    local npc_x = API_get_x(NPC_ID)
    local npc_y = API_get_y(NPC_ID)
    local player_x = API_get_x(player_id)
    local player_y = API_get_y(player_id)

    if npc_x < 0 or npc_y < 0 or player_x < 0 or player_y < 0 then
        return
    end

    local distance = math.abs(npc_x - player_x) + math.abs(npc_y - player_y)
    local now = os.clock()
    if distance <= 3 and now - last_message_time >= 3 then
        API_SendMessage(NPC_ID, player_id, "I see you.")
        last_message_time = now
    end
end
