DS2Compress version 0.50, 2013-02-01

A fast gzip compressor and decompressor for the Supercard DSTWO.

Based on:
* zlib, by Jean-Loup Gailly and Mark Adler
* NDSSFC's GUI, by the Supercard team, improved by GBAtemp users BassAceGold,
  ShadauxCat (in CATSFC) and Nebuleon (in CATSFC)

# CAVEATS

* **Do not unpack files without extensions (such as README and ChangeLog)**
  **from .zip files, or uncompress .gz files that have no extension before**
  **.gz. They corrupt the filesystem and crash applications that read your**
  **storage card!**
* **Do not unpack .zip files that contain folders. The folders will not be**
  **created, and every file will fail to be unpacked.**
* **Unpacking some files creates a phantom file beside the real one. In**
  **certain circumstances, these phantom files may cause your computer to**
  **ask you to reformat your storage card. Do not reformat your card! In**
  **certain circumstances, the files can cause other real files to fail to**
  **be created correctly.**
* **Please make backups of files on your hard drive if you're not sure.**

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

Additionally, you will need to add the updated `zlib`, DMA
(Direct Memory Access) and filesystem access routines provided by BassAceGold
and recompile `libds2a.a`. To do this:

> sudo rm -r /opt/ds2sdk/libsrc/{console,core,fs,key,zlib,Makefile} /opt/ds2sdk/include
> sudo cp -r sdk-modifications/{libsrc,include} /opt/ds2sdk
> sudo chmod -R 600 /opt/ds2sdk/{libsrc,include}
> sudo chmod -R a+rX /opt/ds2sdk/{libsrc,include}
> cd /opt/ds2sdk/libsrc
> sudo rm libds2a.a ../lib/libds2a.a
> sudo make

## The MIPS compiler (`gcc`)
You also need the MIPS compiler from the DS2 SDK.
The Makefile expects it at `/opt/mipsel-4.1.2-nopic`, but you can move it
anywhere, provided that you update the Makefile's `CROSS` variable to point to
it.

## Making the plugin
To make the plugin, `ds2comp.plg`, use the `cd` command to change to the
directory containing your copy of the DS2Compress source, then type
`make clean; make`. `ds2comp.plg` should appear in the same directory.

# Installing

To install the plugin to your storage card after compiling it, copy
`ds2comp.plg`, `ds2comp.ini` and `ds2comp.bmp` to the card's `_dstwoplug`
directory. Then, copy the source directory's DS2COMP subdirectory to the
root of the card.

# Compression levels

By default, the compression level is 1. However, you can adjust it in the
Options menu.

The lowest compression level appears to be 0, but there is no difference
between 0 and 1. These two levels are the fastest, compressing data at
1 MiB/s (more with long stretches of empty data), and they should reduce the
size of most files by a third (1/3, 33%).

The highest compression level is 9. This level is very slow, compressing data
at 64 KiB/s (more with long stretches of empty data), and it should reduce the
size of most files by two fifths (2/5, 40%).

# The font

The font used by DS2Compress is now similar to the Pictochat font. To modify
it, see `source/font/README.txt`.

# Translations

Translations for DS2Compress may be submitted to the author(s) under many
forms, one of which is the Github pull request. To complete a translation, you
will need to do the following:

* Open `DS2COMP/system/language.msg`.
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
under `DS2COMP/system`, and you can now select your new language in
DS2Compress!
