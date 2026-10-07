# ChickPress Co-Op

ChickPress is a demo game built using raylib, asio, and ECS architecture.
Showcasing a chicken that can be controlled using W/A/S/D and space to jump,
with the ability to turn using A and D, featuring a simple physics system.

Run and Jump around the arena, stepping on buttons to increase your score 
and survival time; beware of the tricky buttons for those will try and kill
you and reduce your time to press the next button!

## Story: Chick gets abducted!

Chick was out enjoying a nice meal when suddenly, he started feeling
nauseous. He took one more look at his food and then everything went black.

When Chick woke up, he found himself in a strange place, Surrounded by a
fence and a letter that read:
>Chick, my delicious meal! I have captured you and have taken you to my
>farm. You are surrounded by my minions and will never escape!
>
>-Farmer Joe

Chick took a look around and saw a strange sign that said you have 5 seconds
to press the next button, but which is the correct one? I shall eat you soon...!

## Installation

You will be required to have raylib installed on your system to run this project, you can find the installation instructions for raylib [here](https://github.com/raysan5/raylib). You will also need CMAKE installed to build the project, you can find CMAKE installation instructions [here](https://cmake.org/getting-started/). Asio is also required which comes from updating the submodules.

## Documentation

Important documentation for ChickPress is contained in multiple files.
Please see them:

* `README.md` - This file
* `LICENSE.txt` - The GNU General Public License, under whose terms ChickPress is licensed.

Building the game once the source has been cloned can be achieved by the following:
1. Fetch git submodules using the following command
```
git submodule update --init --recursive
```
2. Navigate your terminal to the **AS9** folder directory; Create a new build folder
using **mkdir build**. Path into build and run **cmake ..** once that finishes run
**cmake --build .** The executable can then be found inside the build/debug directory.
```
mkdir build
cd build
cmake ..
cmake --build .
```
3. Once the project has been compiled into an executable, it can be ran from the
build/debug directory. Using the following command and
**adjusting based on operating system**. (you should have to terminals open
allowing you to run 2 instances of the game.)
```
./debug/as9
```

## Playing the game

keyboard is only supported. Basically, the only controls you will need
to use in-game are the following: jump, move left, right, forward, and backwards.
Jumping lets you avoid certain buttons if they are in your way and moving lets you
navigate to your desiered button.

Other useful keys include the Esc key, which is used to exit the game.

## Community

In case you need help, feel free to reach out using the following means:

* **Email:** Can email me at: noesc.ri00@gmail.com to get in touch with me.

## Grade Guess

* **With GRADE GUESS EC:** 145
* **Without GRADE Guess EC:** 120