.RECIPEPREFIX = +

build:
+ @g++ -std=c++17 interpreter.cpp -o interpreter

run:
+ @./interpreter

clean:
+ @del /f *.o *.exe