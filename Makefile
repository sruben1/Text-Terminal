build:
	gcc -std=gnu99 -Wall -Wextra -o ProjectBuild.out ./src/main.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c ./src/debugUtil.c -lncursesw

debug:
	gcc -std=gnu99 -Wall -Wextra -g -DDEBUG -DPROFILE -o DebugBuild.out ./src/main.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c ./src/debugUtil.c -lncursesw

tests:
	gcc -std=gnu99 -Wall -Wextra -g -DDEBUG -DPROFILE -o TestBuild.out ./src/tests/mainTest.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c ./src/debugUtil.c -lncursesw

syntaxCheck:
	gcc -std=gnu99 -Wall -Wextra -fsyntax-only ./src/main.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c ./src/debugUtil.c -lncursesw