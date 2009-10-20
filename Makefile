all:
	gcc -o ssh-askpass-fullscreen ssh-askpass-fullscreen.c `pkg-config --libs gtk+-2.0` `pkg-config --cflags gtk+-2.0`

clean:
	rm ssh-askpass-fullscreen