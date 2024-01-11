# CMDNOTIFY - Notify of command completion or errors

## Usage

``cmdnotify <command> <args ...>``

## Demo
![Demo](https://github.com/sigsegv7/cmdnotify/blob/main/screenshots/demo.png?raw=true)
![Demo1](https://github.com/sigsegv7/cmdnotify/blob/main/screenshots/demo_fail.png?raw=true)

Upon command exit, the notification can be seen on the top right of the screen.
The program that is being used within this screenshot for notifications is called
``dunst``.

cmdnotify will utilize ``notify-send`` to cause e.g ``dunst`` to display a notification.
