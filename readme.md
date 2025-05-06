# 🎥 Multithreaded Video Player – FFmpeg + GTK 4 (C)

This is a lightweight multithreaded video player built in C using **FFmpeg**, **GTK 4**, and **Pthreads**. The player decodes frames from a video file in a separate thread, stores them in a circular buffer, and renders them to the screen using GTK's drawing area and Cairo.

---

## 🧠 Features

- 🎞️ Decode video frames using FFmpeg libraries (`libavcodec`, `libavformat`, `libswscale`)
- 🧵 Render frames in a GTK window using a separate decoding thread
- 🔄 Uses a circular buffer for real-time frame delivery
- 🔐 Thread-safe frame queue using mutex and condition variables
- 🎯 Adjustable playback frame rate (e.g., 24, 30, 60 FPS)
- 🖼️ Image rendering via Cairo with `GdkPixbuf`

---

## 🛠 Technologies Used

- **Language:** C  
- **Multimedia:** FFmpeg (libavformat, libavcodec, libswscale)  
- **GUI Toolkit:** GTK 4  
- **Concurrency:** Pthreads (mutexes, condition variables)  
- **Graphics:** Cairo + GdkPixbuf

---

Compile - gcc video_player.c -o video_player \
  $(pkg-config --cflags --libs gtk4) \
  -lavformat -lavcodec -lswscale -lavutil -lm -lpthread
Run - ./video_player sample.mp4 30
