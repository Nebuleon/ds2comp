DS2Compress version 0.01, 2013-01-29

A fast gzip compressor and decompressor for the Supercard DSTWO.

Based on:
* zlib, by Jean-Loup Gailly and Mark Adler
* NDSSFC's GUI, by the Supercard team, improved by GBAtemp users BassAceGold,
  ShadauxCat (in CATSFC) and Nebuleon (in CATSFC)

# Compiling

(If you downloaded the plugin ready-made, you can safely skip this section.
 In this case, go to `# Installing`.)

Compiling DS2Compress is best done on Linux. Make sure you have access to a
Linux system to perform these steps.

## The DS2 SDK
To compile DS2Compress, you need to have the Supercard team's DS2 SDK.
The Makefile expects it at `/opt/ds2sdk`, but you can move it anywhere,
provided that you update the Makefile's `DS2SDKPATH` variable to point to it.

For best results, download version 0.13 of the DS2 SDK, which will have the
MIPS compiler (`gcc`), extract it to `/opt/ds2sdk`, follow the instructions,
then download version 1.2 of the DS2 SDK and extract its files into
`opt/ds2sdk`, overwriting version 0.13.

Additionally, you will need to add the updated `zlib` and filesystem access
routines provided by BassAceGold and recompile `libds2a.a`. To do this:

> sudo rm /opt/ds2sdk/{console,fs,key,zlib,Makefile}
> sudo cp -r sdk-modifications/libsrc /opt/ds2sdk
> cd /opt/ds2sdk/libsrc
> sudo rm libds2a.a ../lib/libds2a.a
> sudo make

## The MIPS compiler (`gcc`)
You also need the MIPS compiler from the DS2 SDK.
The Makefile expects it at `/opt/mipsel-4.1.2-nopic`, but you can move it
anywhere, provided that you update the Makefile's `CROSS` variable to point to
it.

## Making the plugin
To make the plugin, `catsfc.plg`, use the `cd` command to change to the
directory containing your copy of the CATSFC source, then type
`make clean; make`. `catsfc.plg` should appear in the same directory.

# Installing

To install the plugin to your storage card after compiling it, copy
`catsfc.plg`, `catsfc.ini` and `catsfc.bmp` to the card's `_dstwoplug`
directory. Then, copy the source directory's CATSFC subdirectory to the
root of the card.

# Cheats

The format accepted by the "Load a cheat file" function is equivalent to the
old format used in Mightymo's BSNES Cheat Code Pack.

1. Download the BSNES Cheat Code Pack at
   <http://www.mightymo.net/downloads.html>. It will be a zip archive.
2. Open the zip file, with WinZip, WinRAR or the built-in zip extension in
   the operating system on your computer.
3. In the zip file, open the folder called *BSNES Cheat Code Pack*, then
   the one called *BSNES v0.51-0.74 Cheat Code Pack*.
4. Open your microSD card's CATSFC folder, then descend into gamecht.
5. Drag the cheat code files from the zip archive to the card's gamecht
   folder.
6. In the card's gamecht directory, create two folders. Name the first one
   *a-m* and the second one *n-z*. Drag the cheat files from the games whose
   name starts with A to M into *a-m* and the rest into *n-z*.

This is because the directory display does not handle more than 512 files.

To add cheats to the menu in a game, first load the game, then use the Cheats
menu's "Load a cheat file" option.

# Frame skipping

In the Video & audio menu, the **Frame skipping** option allows you to select
a number of frames to skip between rendered frames.
* Setting this to 0 will show every single frame, but this will slow down the
  game considerably, as the DSTWO would only have enough processing power to
  emulate **and** render a few frames per second. It has enough power to
  emulate all frames and render **some**, though.
* Setting this to 10 will skip 10 frames and render one, but this will
  severely desynchronise the audio. You will also find yourself unable to
  perform actions during the correct frame with the controller.
* Setting this to - (Keep up with the game) will make the emulator try to
  render the game at its correct speed, dropping frames as needed (up to 8).

It is recommended to start with frame skipping 4 (Show 1 frame every 5) and
go to 3 or 2 if the game doesn't run with major slowdowns with them. If you
don't like the slowdowns, return to frame skipping 4 or -.

# Hotkeys

You can set buttons to press to perform certain actions. For each action,
there is a *global hotkey* and a *game-specific override hotkey*. You might,
for example, want to have the R button bound to Temporary fast-forward, but
a specific game uses R for something important. In that case, you can set the
global hotkey to R and make an override with X for that game.

Hotkeys are sent to the current game as well as to their corresponding action.
The criterion for a hotkey is met when **at least** all of its buttons are
held. Additional keys are sent to the game and can trigger another hotkey.
For example, setting a hotkey to L and another to R+X, then pressing L+R+X+Y
will trigger both and send L+R+X+Y to the game.

Available actions are:
* Go to main menu. In addition to tapping the Touch Screen to return to
  the main menu, you can set a hotkey to do the same.
* Temporary fast-forward. While this hotkey is held, the fast-forward option
  will be forced on.
* Toggle sound. Each time this hotkey is held, the sound will be disabled if
  it's currently enabled, and vice-versa.

# The font

The font used by CATSFC is now similar to the Pictochat font. To modify it,
see `source/font/README.txt`.

# Translations

Translations for CATSFC may be submitted to the author(s) under many forms,
one of which is the Github pull request. To complete a translation, you will
need to do the following:

* Open `CATSFC/system/language.msg`.
* Copy what's between `STARTENGLISH` and `ENDENGLISH` and paste it at the end
  of the file.
* Change the tags. For example, if you want to translate to German, the tags
  will become `STARTGERMAN` and `ENDGERMAN`.
* Translate each of the messages, using the lines starting with `#MSG_` as a
  guide to the context in which the messages will be used.
* Edit `source/nds/message.h`. Find `enum LANGUAGE` and add the name of your
  language there. For the example of German, you would add this at the end of
  the list:
  ```
	,
	GERMAN
  ```
* Still in `source/nds/message.h`, just below `enum LANGUAGE`, you will find
  `extern char* lang[` *some number* `]`. Add 1 to that number.
* Edit `source/nds/gui.c`. Find `char *lang[` *some number* `] =`.
  Add the name of your language, in the language itself. For the example of
  German, you would add this at the end of the list:
  ```
	,
	"Deutsch"
  ```
* Still in `source/nds/gui.c`, find `char* language_options[]`, which is below
  the language names. Add an entry similar to the others, with the last number
  plus 1. For example, if the last entry is `, (char *) &lang[2]`, yours would
  be `, (char *) &lang[3]`.
* Still in `source/nds/gui.c`, find `case CHINESE_SIMPLIFIED`. Copy the lines
  starting at the `case` and ending with `break`, inclusively. Paste them
  before the `}`. Change the language name and tags. For the example of
  German, you would use:
  ```
	case GERMAN:
		strcpy(start, "STARTGERMAN");
		strcpy(end, "ENDGERMAN");
		break;
  ```

Compile again, copy the plugin and your new `language.msg` to your card
under `CATSFC/system`, and you can now select your new language in CATSFC!
