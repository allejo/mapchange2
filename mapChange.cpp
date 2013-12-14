#include <memory>

#include "bzfsAPI.h"
#include "plugin_utils.h"

class mapChange : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
    virtual const char* Name (){return "Map Change";}
    virtual void Init(const char* config);
    virtual void Event(bz_EventData *eventData);
    virtual bool SlashCommand(int playerID, bz_ApiString, bz_ApiString, bz_APIStringList*);
    virtual void Cleanup();

    std::map<std::string, std::string>
        availableMaps;

    std::string
        newMapFile,
        initialMapFile,
        mapFolderLocation,
        mapChangePermission;

    bool
        initialRun;
};

BZ_PLUGIN(mapChange)

void mapChange::Init(const char* commandLine)
{
    bz_debugMessage(4, "mapChange plugin loaded");

    //
    // Setup our plugin variables
    //
    initialRun = true; // We will only need this to save the default the map in case other maps are lost


    //
    // Register our events
    //
    Register(bz_eGetWorldEvent);


    //
    // Load the configuration file
    //
    PluginConfig config = PluginConfig(commandLine);
    std::string section = "mapChange";

    // Shutdown the server
    if (config.errors)
        bz_shutdown();

    // Extract all the data in the configuration file and assign it to plugin variables
    mapFolderLocation = config.item(section, "MAP_FOLDER");
    mapChangePermission = config.item(section, "PERMISSION");


    //
    // Register our slash commands
    //
    bz_registerCustomSlashCommand("mapchange", this);
    bz_registerCustomSlashCommand("maplist", this);
}

void mapChange::Cleanup (void)
{
    bz_debugMessage(4, "mapChange plugin unloaded");

    Flush(); // Clean up all the events

    // Remove all the slash commands
    bz_removeCustomSlashCommand("mapchange");
    bz_removeCustomSlashCommand("maplist");
}

void mapChange::Event(bz_EventData *eventData)
{
    switch (eventData->eventType)
    {
        case bz_eGetWorldEvent:
        {
            bz_GetWorldEventData_V1* worldData = (bz_GetWorldEventData_V1*)eventData;

            // If it's the first run, we know this map works for sure so let's save it just in case
            if (initialRun)
            {
                initialMapFile = worldData->worldFile.c_str();
            }
            else // Set the map to whatever was specified
            {
                worldData->worldFile = newMapFile;
            }
        }
        break;

        default: break;
    }
}

bool mapChange::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    // We don't want unregistered folks to be changing maps to disable that
    if (!playerData->verified || !bz_hasPerm(playerID, mapChangePermission.c_str()))
    {
        bz_sendTextMessagef(BZ_SERVER, playerID, "You do not have permission to run the /%s command.", command.c_str());
        return true;
    }

    if (command == "mapchange")
    {
        // So we don't interrupt any matches, force players to end the game first before a map change
        if (bz_isTimeManualStart() || bz_isCountDownActive() ||
            bz_isCountDownInProgress() || bz_isCountDownPaused())
        {
            bz_sendTextMessage(BZ_SERVER, playerID, "There is currently a match going on, please wait until it's over to change the map.");
            return true;
        }

        if (availableMaps.find(params->get(0).c_str()) == availableMaps.end())
        {
            bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "The map %s cannot be found or has not be configured.", params->get(0).c_str());
            return true;
        }

        // Set a BZDB variable with this information in case another plugin would like to know what map file is being used
        bz_setBZDBString("mapChangeName", params->get(0).c_str(), 3, true);

        // Announce it so players don't freak out
        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s has requested a map change to %s.", playerData->callsign.c_str(), params->get(0).c_str());
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Please rejoin when kicked. Restarting...");

        // Restart!
        bz_restart();
        return true;
    }
    else if (command == "maplist")
    {

    }
}