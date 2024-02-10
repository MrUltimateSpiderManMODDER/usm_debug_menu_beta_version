# Ultimate Spider-Man Debug Menu from Preview Build version recreated for PC



**Recreation** of the PS2 and XBOX builds version's debug menu for the PC version

## Download

Get the menu from [here](https://github.com/MrUltimateSpiderManMODDER/usm_debug_menu_beta_version/releases/tag/v1)

## Installation

* Rename `binkw32.dll` in the game's folder to `binkw32_.dll`
* Copy the `binkw32.dll` in the zip file to the game's folder

## Usage

* **INSERT** - opens and closes the menu
* **UP/DOWN Arrows** - moves the current select option up and down
* **ENTER** - executes the selected option
* **LEFT/RIGHT Arrows** - changes the value of the current **boolean** option
* **ESCAPE** - go to previous menu

## Usage DS4/DS5 Controller

* **PS BUTTON** - opens and closes the menu
* **UP/DOWN Pads** - moves the current select option up and down
* **CROSS BUTTON** - executes the selected option
* **LEFT/RIGHT Pad Arrows** - changes the value of the current **boolean** option
* **CIRCLE BUTTON** - go to previous menu


## Current implemented options

### Warp

Allows to teleport to any in-game region

**NOTE**: Points of Interest(POI) are only used in missions and the option only works when the direction arrow is visible 

### District variants

Allows to switch between variants of given regions.

### Entity variants

There are list of entity variants menu.

### Missions

You can replay all missions of this game and a few emergencies.


### Level Select

There are list of levels available that not working and crash the game if you load city option

Note exist only --REBOOT-- and city options.



### Hero Select

Allows to switch between *all* available characters

**NOTE**: `peter_parker` and `peter_hooded` are versions of peter without powers, used in the first missions of the game.

### Game

Allows to modify in-game flags that controls certain behaviours.

### DevOpt Flags

There are list of flags options available

Allows to modify flags that controls certain behaviours.

### DevOpt Ints

There are list of ints options available

Allows to modify ints that controls certain behaviours.

### Script

Access to in-game scripts that allow the player to tap into debug features.

### Progression

Set your progress to any point in the game.

### Camera

you can switch camera states Chase, User and SceneAnalizer.

## Extending functionality

If you desire to add any new option(s) to the menu, it's very simple.
`create_menu` is used to create a new menu.
`add_debug_menu_entry` is used to add entries to a menu.


## License

This project is MIT licensed
