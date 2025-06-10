build:
	gcc -std=gnu99 -Wall -Wextra -o textterminal.out ./src/main.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c ./src/debugUtil.c ./src/undoRedoUtilities.c ./src/statistics.c -lncursesw -lm -D_GNU_SOURCE

debug:
	gcc -std=gnu99 -Wall -Wextra -g -fsanitize=address -DDEBUG -DPROFILE -o DebugBuild.out ./src/main.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c ./src/debugUtil.c ./src/undoRedoUtilities.c ./src/statistics.c -lncursesw -lm -D_GNU_SOURCE

profiler:
	gcc -std=gnu99 -Wall -Wextra -g -DPROFILE -o TestBuild.out ./src/tests/mainTest.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c ./src/debugUtil.c ./src/undoRedoUtilities.c ./src/statistics.c -lncursesw -lm -D_GNU_SOURCE

syntaxCheck:
	gcc -std=gnu99 -Wall -Wextra -fsyntax-only ./src/main.c ./src/textStructure.c ./src/guiUtilities.c ./src/fileManager.c ./src/debugUtil.c ./src/undoRedoUtilities.c ./src/statistics.c -lncursesw -lm -D_GNU_SOURCE