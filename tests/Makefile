all: function sequential simpleloop helloworld switch

sequential: sequential.o
	gcc -O0 -g -o $@ $<
sequential.o: sequential.c
	gcc -O0 -c -g -o $@ $<
	gcc -O0 -S $<

simpleloop: simpleloop.o
	gcc -O0 -g -o $@ $<
simpleloop.o: simpleloop.c
	gcc -O0 -c -g -o $@ $<
	gcc -O0 -S $<

helloworld: helloworld.o
	gcc -O0 -g -o $@ $<
helloworld.o: helloworld.c
	gcc -O0 -c -g -o $@ $<
	gcc -O0 -S $<

switch: switch.o
	gcc -O0 -g -o $@ $<
switch.o: switch.c
	gcc -O0 -c -g -o $@ $<
	gcc -O0 -S $<

function: function.o
	gcc -O0 -g -o $@ $<
function.o: function.c
	gcc -O0 -c -g -o $@ $<
	gcc -O0 -S $<



clean:
	rm -f function sequential switch simpleloop helloworld *.o *.s
