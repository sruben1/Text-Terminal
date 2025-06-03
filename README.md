# Text-Terminal: UTF-8 Text Editor for Linux Shell
## About
Text Terminal is a simple text editor running in your linux shell. It supports `.txt` files in Linux, Mac or Windows format.
It's key features are: 
* Good hanling of large files.
* Realtime word and line count statistics.
* Find and Find & Replace functions.
* Mouse support.
* X11 integrated cut, copy, paste support.


## Requirements
* A 64 bit linux system.
* A linux shell compatible with the ncurses api.
* xClip must be installed for cut, copy and paste.

## Usage
### Setup
To compile from our source in the project root directory use:
```
make build
```
To start *Text-Terminal*:
```
./ProjectBuild.out [mandatory relative path to existing or new file]
```
### In the Application
Ensure that the text file is not moddified while it is open in *Text-Terminal* since it uses the orriginal file in it's current state to provide high efficiency.
Use *up down left* and *right* arrows or *click* with your mouse to move the cursor in the text. 
Use *shift + left* or *shift + right* arrow to select multiple characters and to enlargen selection up or down one line simply use the *page up* or *page down* keys.
As an alternative it is also possible to select multiple characters using a *double mouse click* making a selection form the inital cursor position to this new position.

To use Find simply click with your mouse on the bottom of the text, enter the keyword and confirm with enter. The screen will automatically move the next occurence.
To use Find and Replace activate this option in the menu, and enter first your keyword and then the word that should replace it. Simply use *shift + enter* to cycle and replace the next occurence.

### Save and Exit
To save your new changes to the file use *ctrl + S*. 
To exit the editor use *ctrl + L*.
