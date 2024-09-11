# Train Simulation

This project is a lightweight simulation of four trains moving on interconnected tracks, demonstrating the use of mutexes and non-binary semaphores for thread synchronization in C.

## Purpose

The main goal of this simulation is to showcase how mutexes and non-binary semaphores can be used to prevent collisions between multiple trains sharing tracks and intersections. It serves as an educational tool for understanding concurrent programming concepts in a visual and interactive manner.

## Features

- Four trains moving on separate tracks with shared sections
- Visual representation using ncurses library
- Real-time speed control for each train
- Pause functionality for detailed state inspection
- Collision prevention using mutexes and semaphores

## Requirements

- GCC compiler
- ncurses library
- POSIX threads library

## Compilation

To compile the program, use the following command in your terminal:

```
gcc -o train_simulation train_simulation.c -lncurses -lpthread -lm
```

Make sure to include the `-lncurses`, `-lpthread`, and `-lm` flags as they are necessary for linking the required libraries.

## Running the Simulation

After compilation, run the program using:

```
./train_simulation
```

## Controls

- Use keys '1', '2', '3', '4' to select a train
- '+' to increase the selected train's speed
- '-' to decrease the selected train's speed
- 'p' to pause/unpause the simulation
- 'q' to quit the program

## How it Works

1. The simulation creates four tracks represented as rectangles on the screen.
2. Each train is controlled by a separate thread.
3. Mutexes are used to control access to shared track sections.
4. A non-binary semaphore is used to limit the number of trains in the central intersection.
5. Trains automatically slow down when approaching occupied shared tracks or a full intersection.
6. The main thread handles user input and updates the display.

## Code Structure

- Main loop: Handles user input and initializes threads
- Render thread: Updates the visual representation of trains and tracks
- Train threads: Control individual train movement and handle synchronization
- Helper functions: Draw tracks, trains, and handle collision detection

## Learning Outcomes

By studying and interacting with this simulation, you can gain insights into:

- Thread creation and management in C
- Use of mutexes for protecting shared resources
- Implementation of non-binary semaphores for resource counting
- Collision avoidance in a multi-threaded environment
- Real-time user interaction with a concurrent system

Feel free to modify the code and experiment with different synchronization strategies!
