build:
	gcc -std=gnu99 -Wall -Wextra -o ProjectBuild.out ./src/main.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c -lncurses

buildDebug:
	gcc -std=gnu99 -Wall -Wextra -g -DDEBUG -DPROFILE -o DebugBuild.out ./src/main.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c -lncurses

syntaxCheck:
	gcc -std=gnu99 -Wall -Wextra -fsyntax-only ./src/main.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c -lncurses