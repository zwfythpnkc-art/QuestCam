obs = obslua

local VIDEO_NAME = "QuestCam USB Video"
local AUDIO_NAME = "QuestCam USB Audio"
local adb_path = ""
local port = 5353

local function quote(value)
    return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

local function exists(path)
    local file = io.open(path, "r")
    if file then file:close() return true end
    return false
end

local function find_adb()
    local home = os.getenv("HOME") or ""
    local paths = {
        adb_path,
        home .. "/Library/Android/sdk/platform-tools/adb",
        "/opt/homebrew/bin/adb",
        "/usr/local/bin/adb",
        "/Applications/SideQuest.app/Contents/Resources/app.asar.unpacked/build/platform-tools/adb"
    }
    for _, path in ipairs(paths) do
        if path ~= "" and exists(path) then return path end
    end
    return ""
end

local function run(command)
    local process = io.popen(command .. " 2>&1")
    if not process then return false, "Could not start command" end
    local output = process:read("*a") or ""
    local ok, _, code = process:close()
    return ok == true or code == 0, output
end

local function adb_run(adb, args)
    return run(quote(adb) .. " " .. args)
end

local function connect_quest()
    local adb = find_adb()
    if adb == "" then return false, "ADB not found. Select it above." end
    local ok, output = adb_run(adb, "start-server")
    if not ok then
        adb_run(adb, "kill-server")
        ok, output = adb_run(adb, "start-server")
    end
    if not ok then return false, "ADB could not start: " .. output end
    ok, output = adb_run(adb, "devices")
    if not ok or not output:match("\tdevice") then
        return false, "No authorised Quest. Allow USB debugging in the headset."
    end
    adb_run(adb, "forward --remove tcp:" .. port)
    ok, output = adb_run(adb, "forward tcp:" .. port .. " tcp:" .. port)
    if not ok then return false, "USB forwarding failed: " .. output end
    return true, "QuestCam USB connected"
end

local function source_settings(url, format)
    local settings = obs.obs_data_create()
    obs.obs_data_set_string(settings, "input", url)
    obs.obs_data_set_string(settings, "input_format", format)
    obs.obs_data_set_bool(settings, "is_local_file", false)
    obs.obs_data_set_bool(settings, "restart_on_activate", true)
    obs.obs_data_set_bool(settings, "close_when_inactive", false)
    obs.obs_data_set_int(settings, "buffering_mb", 1)
    return settings
end

local function add_source(name, url, format)
    local settings = source_settings(url, format)
    local source = obs.obs_get_source_by_name(name)
    if source then
        obs.obs_source_update(source, settings)
    else
        source = obs.obs_source_create("ffmpeg_source", name, settings, nil)
        local scene_source = obs.obs_frontend_get_current_scene()
        if scene_source and source then
            obs.obs_scene_add(obs.obs_scene_from_source(scene_source), source)
        end
        if scene_source then obs.obs_source_release(scene_source) end
    end
    obs.obs_data_release(settings)
    if source then obs.obs_source_release(source) end
end

local function restart_sources()
    local nonce = tostring(os.time())
    add_source(VIDEO_NAME, "http://127.0.0.1:" .. port .. "/stream.mjpg?obs=" .. nonce, "mjpeg")
    add_source(AUDIO_NAME, "http://127.0.0.1:" .. port .. "/audio.wav?obs=" .. nonce, "wav")
end

function connect_clicked(props, property)
    local ok, message = connect_quest()
    if not ok then obs.script_log(obs.LOG_ERROR, message) return true end
    restart_sources()
    obs.script_log(obs.LOG_INFO, message .. "; sources added")
    return true
end

function refresh_clicked(props, property)
    restart_sources()
    return true
end

function disconnect_clicked(props, property)
    local adb = find_adb()
    if adb ~= "" then adb_run(adb, "forward --remove tcp:" .. port) end
    return true
end

function script_description()
    return "QuestCam USB plugin. Adds native video and audio Media Sources without Python, Safari, or a Browser Source."
end

function script_defaults(settings)
    local home = os.getenv("HOME") or ""
    obs.obs_data_set_default_string(settings, "adb_path", home .. "/Library/Android/sdk/platform-tools/adb")
    obs.obs_data_set_default_int(settings, "port", 5353)
end

function script_update(settings)
    adb_path = obs.obs_data_get_string(settings, "adb_path")
    port = math.max(1024, math.min(65535, obs.obs_data_get_int(settings, "port")))
end

function script_properties()
    local props = obs.obs_properties_create()
    obs.obs_properties_add_path(props, "adb_path", "ADB executable", obs.OBS_PATH_FILE, "Executable (*)", nil)
    obs.obs_properties_add_int(props, "port", "QuestCam port", 1024, 65535, 1)
    obs.obs_properties_add_button(props, "connect", "Connect Quest + Add Sources", connect_clicked)
    obs.obs_properties_add_button(props, "refresh", "Restart Media Sources", refresh_clicked)
    obs.obs_properties_add_button(props, "disconnect", "Disconnect USB", disconnect_clicked)
    return props
end
