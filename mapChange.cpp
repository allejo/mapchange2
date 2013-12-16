/*
Copyright (c) 2013 Vladimir "allejo" Jimenez
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
     derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <dirent.h>
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "bzfsAPI.h"
#include "plugin_utils.h"

std::string getExtension(std::string filename)
{
    // Find the location of the period to find where to start trimming
    int position = filename.find_last_of(".");

    // If we can't find the dot or it's at the beginning, then there is no extension
    if (position == std::string::npos || position == 0)
    {
        return "none";
    }

    // Return just the extension, so one more than the location of the period so we don't include the period
    return filename.substr(position + 1);
}

std::string getFilename(std::string filename)
{
    // Find the location of the period to find where to start trimming
    int position = filename.find_last_of(".");

    // If we can't find the dot or it's at the beginning, then there is no extension
    if (position == std::string::npos || position == 0)
    {
        return "none";
    }

    // Return just the file name, so from the beginning to the location of the period
    return filename.substr(0, position);
}

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
        newMapTitle,
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
    {
        bz_debugMessage(0, "ERROR :: Map Change :: Configuration file contains errors.");
    }

    // Extract all the data in the configuration file and assign it to plugin variables
    mapFolderLocation = config.item(section, "MAP_FOLDER");
    mapChangePermission = config.item(section, "PERMISSION");

    // I'm only supporting linux because Windows sux and no one uses Windows to host BZFlag servers
    if (*mapFolderLocation.rbegin() != '/')
    {
        mapFolderLocation += "/"; // Add a trailing slash if one doesn't exist so we can have a complete path when
                                  // we are looping through all the maps to make available.
    }

    // Send debug information to aid server owners to see what is going wrong
    bz_debugMessagef(2, "DEBUG :: Map Change :: Using '%s' permission for slash commands.", mapChangePermission.c_str());
    bz_debugMessagef(2, "DEBUG :: Map Change :: Looking for maps in directory: %s", mapFolderLocation.c_str());


    //
    // Find all the .bzw files in our directory to make them available in our map switching
    //
    DIR *dp;
    struct dirent *ep;
    dp = opendir(mapFolderLocation.c_str());

    // Check to see if we can open the folder so we won't segfault
    if (dp != NULL)
    {
        while (ep = readdir(dp)) // Loop through all the files in the directory to look only for maps
        {
            if (ep != NULL) // To avoid segfaults, make sure that our current file has been successfully set/read
            {
                // Set some variables for easy access
                std::string fullFilename = ep->d_name;
                std::string filename = getFilename(fullFilename);
                std::string extension = getExtension(fullFilename);

                // Only load bzw files or else we'll be loading configuration, log, or report files
                if (extension == "bzw")
                {
                    // Tell debug a map file has been succsesfully loaded
                    bz_debugMessagef(1, "DEBUG :: Map Change :: Making map file '%s' available as '%s'...", fullFilename.c_str(), filename.c_str());

                    // Add it to the map
                    availableMaps[filename] = mapFolderLocation + fullFilename;
                }
            }
        }

        // Let's not forget to close our directory to avoid memory leaks
        closedir(dp);
    }
    else // Well we can't access the folder, write an error
    {
        bz_debugMessagef(0, "ERROR :: Map Change :: Could not open directory: %s", mapFolderLocation.c_str());
    }


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
                initialRun = false;

                // Set a BZDB variable with this information in case another plugin would like to know what map file is being used
                bz_setBZDBString("mapChangeName", getFilename(initialMapFile).c_str(), 3, true);
            }
            else // Set the map to whatever was specified
            {
                struct stat buffer; // Create a struct to store the information given by stat()

                // Check if the new map file still exists in case it was deleted
                if (stat(newMapFile.c_str(), &buffer) == 0)
                {
                    worldData->worldFile = newMapFile;
                }
                else // If the map file doesn't exist, then use the orginal map file
                {
                    bz_debugMessagef(0, "ERROR :: Map Change :: Map file (%s) no longer found.", newMapFile.c_str());
                    worldData->worldFile = initialMapFile;
                }

                // Set a BZDB variable with this information in case another plugin would like to know what map file is being used
                bz_setBZDBString("mapChangeName", newMapTitle.c_str(), 3, true);
            }

            // Tell debug what map is now playing on the server and test the BZDB variable for other plugins to access
            bz_debugMessagef(2, "DEBUG :: Map Change :: Testing locked BZDB variable... Now playing: %s", bz_getBZDBString("mapChangeName").c_str());
        }
        break;

        default: break;
    }
}

bool mapChange::SlashCommand(int playerID, bz_ApiString command, bz_ApiString /*message*/, bz_APIStringList *params)
{
    std::unique_ptr<bz_BasePlayerRecord> playerData(bz_getPlayerByIndex(playerID));

    // We don't want unregistered folks to be changing maps to disable that
    if (!bz_hasPerm(playerID, mapChangePermission.c_str()))
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
            bz_sendTextMessagef(BZ_SERVER, playerID, "The map %s cannot be found or has not be configured.", params->get(0).c_str());
            return true;
        }

        // Announce it so players don't freak out
        bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "%s has requested a map change to '%s'.", playerData->callsign.c_str(), params->get(0).c_str());
        bz_sendTextMessage(BZ_SERVER, BZ_ALLUSERS, "Please rejoin when kicked. Restarting...");

        // Save the location of the map file we will be switching to
        newMapFile = availableMaps[params->get(0).c_str()];
        newMapTitle = params->get(0).c_str();

        // Restart!
        bz_restart();
        return true;
    }
    else if (command == "maplist")
    {
        bz_sendTextMessage(BZ_SERVER, playerID, "Available Maps");
        bz_sendTextMessage(BZ_SERVER, playerID, "--------------");

        // Create an interator so we an go through the map
        std::map<std::string, std::string>::iterator iter;

        // Loop through the map to list the avaiable maps that exist and send them to the player
        for (iter = availableMaps.begin(); iter != availableMaps.end(); ++iter)
        {
            bz_sendTextMessagef(BZ_SERVER, playerID, "  * %s", iter->first.c_str());
        }
    }
}