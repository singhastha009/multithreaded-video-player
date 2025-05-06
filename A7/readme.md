Compile - gcc A7.c -o A7 `pkg-config --cflags --libs gtk4` -lavformat -lavcodec -lavutil -lswscale -lpthread

Run  - ./A7 input.mp4 30