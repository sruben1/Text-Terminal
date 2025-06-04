# Text-Terminal: UTF-8 Text Editor for Linux Shell
## About
Text Terminal is a simple text editor running in your linux shell. It supports `.txt` files in Linux, Mac or Windows format.
It's key features are: 
* Good handling of large files.
* Efficient Undo and Redo implementation.
* Realtime word and line count statistics.
* Find and Find & Replace functions.
* Mouse support.
* X11 integrated cut, copy, paste support.


## Requirements
* A 64 bit linux system.
* A linux shell compatible with the ncurses api.
* xClip must be installed for cut, copy and paste.
to build form source:
* ncursesw version 6 and dependencies 

## Usage
### Setup
To compile from our source, in the project root directory use:
```
make build
```
To start *Text-Terminal*:
```
./textterminal.out [mandatory relative path to existing or new file]
```
### In the Application
Ensure that the text file is not modified while it is open in *Text-Terminal* since it uses the original file in it's current state to provide high efficiency.

Use *up, down, left,* and *right* arrows or simply *click* with your mouse to move the cursor to a position in the text. 
Use *shift + left* or *shift + right* arrow to enlarge your selection over multiple characters. Similarly to enlarge the selection up or down one line simply use the *page up* or *page down* keys.
As an alternative it is also possible to select multiple characters using a *double mouse click*: this makes a selection form the initial cursor position to your new *double clicked* position.

Use *ctrl + Z* to undo the last text modification, use *ctrl + Y* to redo it. 

To use Find simply click with your mouse on the menu button at the bottom of the screen. Then enter the keyword and confirm with enter. The screen will automatically move the next occurrence of this keyword.

To use Find and Replace, click on it in the menu. First enter your keyword, hit *enter*, then the word that should replace it, hit *enter*. If any occurrence is found; the first occurrence will now already be replaced; use *ctrl + Z* to revert. Simply use *shift + enter* to cycle again and replace the next occurrence.

### Save and Exit
To save your new changes to the file use *ctrl + S*. 
To exit the editor use *ctrl + L*.
