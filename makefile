.RECIPEPREFIX = +

build:
+ @g++ -std=c++17 sock.cpp -o sock

run:
+ @./sock

clean:
+ @del /f *.o *.exe