.RECIPEPREFIX = +

build:
+ @g++ -std=c++17 src/interpreter.cpp -o bin/interpreter

run:
+ @./bin/interpreter examples/sock.soq

clean:
+ @del /q "bin\*"