# Proof of concept for an X.org 3D animated desktop background
This is a project I worked on back in 2017 to make a 3D animated desktop
background for X.org using OpenGL. As Wayland has since replaced X.org, I can
no longer maintain this but am still showing it as a proof of concept. The code
does contain some issues (such as a data race in the function `kill`) that will
remain unfixed until I get to use X.org again.

## Building
See the `Makefile`.

* `wave.c` creates a window and displays the waves.
* `glx.c` displays the same waves directly in the desktop background.
* `glxnew.c` comments out some code, but I don't remember what it changes; it
  is not automatically compiled by the `Makefile`.

## History
I got the idea when I looked at the source code for
[feh](https://feh.finalrewind.org/) which has a feature to set the desktop
background. For much of my first year in engineering school, I used feh to
display an animated GIF as a desktop background (it was ugly, but since I use
tiling WMs I barely cared).

The first approach was to render the waves in a hidden GLFW window, copy the
framebuffers into memory, convert them into another format suited for X.org,
and transfer that into X.org like feh did. Trying to do that without a single
use of `malloc` was my first stack overflow in C. It was also monstruously
slow.

The second approach was to copy the framebuffers into memory directly in a
format that X.org can use. It became possible to do it with no `malloc` (at
least on my 1080p laptop) and was almost fluid enough, but still very heavy on
the CPU.

I then realized that using GLX, X.org can create an OpenGL context into any
window the program has access to, including "root," the window that contains
the desktop background. Doing that instantly made it viable as a program that
runs in the background. That's pretty much all I remember about how it went.

I worked on this before I knew how to use Git. Trust me, I did do some cleaning
before publishing this code. The original had 7 copies of the code for backup
purposes.

## Oh my god! I see some gotos!
Cleanup gotos are fine. Shut up.
