.RECIPEPREFIX = +

build:
+ @g++ -std=c++17 src/interpreter.cpp -o bin/interpreter

run:
+ @./bin/interpreter examples/sock.soq

sock: $(filter-out $@,$(MAKECMDGOALS))
+ @./bin/interpreter $<

clean:
+ @del /q "bin\*"