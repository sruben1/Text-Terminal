# Text-Terminal: UTF-8 Text Editor for Linux Shell

## About

Text Terminal is a simple text editor running in your Linux shell. It supports `.txt` files in Linux, Mac or Windows format.
Its key features are:

* Good handling of large files.
* Efficient Undo and Redo implementation.
* Real time word and line count statistics.
* Find and Find & Replace functions.
* Mouse support.
* X11 integrated cut, copy, paste support.

## Requirements

* A 64-bit Linux system.
* A Linux shell compatible with the ncurses API.
* xclip must be installed for cut, copy and paste.
* ncursesw version 6 and dependencies

## Usage

### Setup

To compile from source, in the project root directory use:

```bash
make build
```
To start *Text-Terminal* use a relative path to existing or new file to create. As  file standard if  0: Linux, 1: Windows 2: Mac:
```bash
./textterminal.out [mandatory relative path to existing or new file] [optional 0,1, or 2]
```

### In the Application

Ensure that the text file is not modified elsewhere while it is open in *Text-Terminal* since it uses the original file in its current state to provide high efficiency.

Use <kbd>&uarr;</kbd>, <kbd>&darr;</kbd>, <kbd>&larr;</kbd>, <kbd>&rarr;</kbd> or simply *click* with your mouse to move the cursor to a position in the text.
Use <kbd>Shift</kbd> + <kbd>&larr;</kbd> or <kbd>Shift</kbd> + <kbd>&rarr;</kbd> to enlarge your selection over multiple characters. Similarly, to enlarge the selection up or down one line simply use the <kbd>PageUp</kbd> or <kbd>PageDown</kbd> keys.
As an alternative it is also possible to select multiple characters using a *double mouse click*: this makes a selection from the initial cursor position to your new *double-clicked* position.

Use <kbd>Ctrl</kbd> + <kbd>Z</kbd> to undo the last text modification, use <kbd>Ctrl</kbd> + <kbd>Y</kbd> to redo it.

To use Find, *click* with your mouse on the menu button at the bottom of the screen. Then enter the keyword and confirm with <kbd>Enter</kbd>. The screen will automatically move to the next occurrence of this keyword.

To use Find and Replace, *click* on the button in the menu. First type your keyword, hit <kbd>Enter</kbd>, then type the word that should replace it and hit <kbd>Enter</kbd> again. If any occurrence is found, the first occurrence will be replaced immediately - use <kbd>Ctrl</kbd> + <kbd>Z</kbd> to revert. Simply use <kbd>Shift</kbd> + <kbd>Enter</kbd> to cycle again and replace the next occurrence.

### Save and Exit

To save your new changes to the file use <kbd>Ctrl</kbd> + <kbd>S</kbd>.
To exit the editor use <kbd>Ctrl</kbd> + <kbd>L</kbd>.
