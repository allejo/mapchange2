    Warning: Due to a bug with BZFS, this plugin will only partially work. Until BZFS is fixed, it is recommended you use the
    original plugin.

Map Change
=========

A BZFlag plugin to allow players to switch the map a server is using without restarting the actual server instance; this is a replacement for the original plugin found [here](http://forums.bzflag.org/viewtopic.php?f=79&t=9903). In other words, this plugin will kick all the players and load a new map file without having to actually restart the server.

## Author
Vladimir "allejo" Jimenez"

## BZFS Details
This plugin will __only__ work on *nix because Windows sux for hosting BZFlag servers. This plugin only requires a configuration file that contains two bits of information: a folder and a spawn permission.

### Slash Commands
```
/mapchange
/maplist
```

### Parameters
```
/path/to/mapChange.so,/path/to/mapChange.conf
```

### Configuration File Options

MAP_FOLDER

* Default: /path/to/map_folder
* Description:  The directory to look for map files to make available in a map change server.

PERMISSION

* Default: vote
* Description:  The permission, normal or custom, to be used to grant users the ability to change the map. For example, you can use the default "vote" permission or use a custom one such as "mapchange." The default permission will only be effective if only registered players can vote or else there may be abuse.

## License
BSD
