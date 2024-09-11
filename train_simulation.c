#include <ncurses.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include <math.h>

#define TRACK_CHAR '-'
#define TRAIN_CHAR '*'
#define TRAIN_LENGTH 4
#define NUM_TRAINS 4
#define NUM_SHARED_TRACKS 4
#define NUM_TRACKS 4

// Track dimensions
const int track_width = 10;
const int track_height = 5;
const int track_len = 2 * (track_width + track_height);
bool paused = false;

// Structure to represent train position
typedef struct {
    int track_id;
    float position;
} TrainPosition;

// Global variables for train positions and velocities
TrainPosition train_pos[NUM_TRAINS] = {
    {0, 0.75f * track_len},
    {1, 0.0f},
    {2, track_len / 2.0f},
    {3, track_width}
};
float train_vel[NUM_TRAINS] = {1.0f, 1.0f, 1.0f, 1.0f};
int active_train = 0;  // Index of the currently selected train
                       
float original_train_vel[NUM_TRAINS] = {1.0f, 1.0f, 1.0f, 1.0f};

// Synchronization primitives
pthread_mutex_t main_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
bool quit = false;

// Mutexes for shared tracks
pthread_mutex_t shared_track_mutex[NUM_SHARED_TRACKS];

// Semaphore for intersection
sem_t intersection_semaphore;
#define MAX_TRAINS_IN_INTERSECTION 3

// Function to check if a position is in a shared track
int is_shared_track(int track_id, int pos) {
    if (track_id == 0) {
        if (pos > track_width && pos < track_width + track_height) 
            return 0;
        else if (pos > track_width + track_height && pos < 2*track_width + track_height)
            return 3;
    }
    
    if (track_id == 1) {
        if (pos > track_width + track_height && pos < 2*track_width + track_height) 
            return 1;
        else if (pos > 2*track_width + track_height)
            return 0;
    }
    
    if (track_id == 2) {
        if (pos < track_width) 
            return 3;
        else if (pos > track_width && pos < track_width + track_height)
            return 2;
    }
 
    if (track_id == 3) {
        if (pos < track_width) 
            return 1;
        else if (pos > 2*track_width + track_height)
            return 2;
    }
 
    return -1;  // Not in a shared track
}

// Function to check if a position is in the intersection
int is_in_intersection(int track_id, int pos) {
    if (track_id == 0 && pos > track_width && pos < 2*track_width + track_height) return 1;
    if (track_id == 1 && pos > track_width + track_height) return 1;
    if (track_id == 2 && pos < track_width + track_height) return 1;
    if (track_id == 3 && (pos < track_width || pos > 2*track_width + track_height)) return 1;
    return 0;
}

void draw_rectangular_track(WINDOW *win, int y, int x, int width, int height) {
    mvwhline(win, y, x, TRACK_CHAR, width);
    mvwhline(win, y + height, x, TRACK_CHAR, width);
    mvwvline(win, y, x, TRACK_CHAR, height);
    mvwvline(win, y, x + width, TRACK_CHAR, height + 1);
}

void draw_train(WINDOW *win, int y, int x, int width, int height, float pos) {
    int train_y = y, train_x = x;
    int int_pos = (int)pos;
    if (int_pos < width) {
        train_x += int_pos;
    } else if (int_pos < width + height) {
        train_x += width;
        train_y += int_pos - width;
    } else if (int_pos < 2 * width + height) {
        train_x += 2 * width + height - int_pos - 1;
        train_y += height;
    } else {
        train_y += 2 * width + 2 * height - int_pos - 1;
    }
    mvwaddch(win, train_y, train_x, TRAIN_CHAR);
}

void draw_control_panel(WINDOW *win) {
    mvwprintw(win, 15, 2, "Control Panel:");
    for (int i = 0; i < NUM_TRAINS; i++) {
        mvwprintw(win, 16 + i, 2, "Train %d: Track %d, Pos %.1f, Vel %.2f", 
                  i + 1, train_pos[i].track_id + 1, train_pos[i].position, train_vel[i]);
    }
    mvwprintw(win, 21, 2, "Use 1-4 to select train, +/- to change speed");
    
    // Highlight the active train
    mvwprintw(win, 16 + active_train, 0, ">");
}

void* render_thread(void* arg) {
    while (!quit) {
        if (!paused) {
            clear();
            // Draw tracks
            draw_rectangular_track(stdscr, 2, 2, track_width, track_height);   // Track 1
            draw_rectangular_track(stdscr, 2, 15, track_width, track_height);  // Track 2
            draw_rectangular_track(stdscr, 9, 2, track_width, track_height);   // Track 3
            draw_rectangular_track(stdscr, 9, 15, track_width, track_height);  // Track 4

            pthread_mutex_lock(&main_mutex);
            // Draw trains
            for (int i = 0; i < NUM_TRAINS; i++) {
                int y = (train_pos[i].track_id / 2) * 7 + 2;
                int x = (train_pos[i].track_id % 2) * 13 + 2;
                draw_train(stdscr, y, x, track_width, track_height, train_pos[i].position);
            }
            // Draw control panel
            draw_control_panel(stdscr);
            pthread_mutex_unlock(&main_mutex);

            refresh();
        }
        usleep(100000);  // Sleep for 100ms
    }
    return NULL;
}

void* train_thread(void* arg) {
    int train_id = *(int*)arg;
    int current_shared_track = -1;
    int next_shared_track;
    bool in_intersection = false;
    bool is_slowed = false;
    
    while (!quit) {
        pthread_mutex_lock(&main_mutex);
        
        if (!paused) {
            float next_pos = fmodf(train_pos[train_id].position + train_vel[train_id] + track_len, track_len);
            int int_next_pos = (int)next_pos;
            next_shared_track = is_shared_track(train_pos[train_id].track_id, int_next_pos);
            bool next_in_intersection = is_in_intersection(train_pos[train_id].track_id, int_next_pos);
            
            if (next_in_intersection && !in_intersection) {
                // Trying to enter the intersection
                if (sem_trywait(&intersection_semaphore) == 0) {
                    // Successfully acquired the semaphore
                    in_intersection = true;
                    // Now try to acquire the mutex for the specific shared track
                    if (next_shared_track != -1 && pthread_mutex_trylock(&shared_track_mutex[next_shared_track]) == 0) {
                        if (current_shared_track != -1) {
                            pthread_mutex_unlock(&shared_track_mutex[current_shared_track]);
                        }
                        current_shared_track = next_shared_track;
                        train_pos[train_id].position = next_pos;
                        if (is_slowed) {
                            train_vel[train_id] = original_train_vel[train_id];
                            is_slowed = false;
                        }
                    } else {
                        // Failed to acquire mutex, release semaphore and slow down
                        sem_post(&intersection_semaphore);
                        in_intersection = false;
                        if (!is_slowed) {
                            original_train_vel[train_id] = train_vel[train_id];
                            train_vel[train_id] *= 0.5f;
                            is_slowed = true;
                        }
                    }
                } else {
                    // Failed to acquire semaphore, slow down the train
                    if (!is_slowed) {
                        original_train_vel[train_id] = train_vel[train_id];
                        train_vel[train_id] *= 0.5f;
                        is_slowed = true;
                    }
                }
            } else if (next_shared_track != -1 && next_shared_track != current_shared_track) {
                // Trying to enter a new shared track (but not the intersection)
                if (pthread_mutex_trylock(&shared_track_mutex[next_shared_track]) == 0) {
                    if (current_shared_track != -1) {
                        pthread_mutex_unlock(&shared_track_mutex[current_shared_track]);
                    }
                    current_shared_track = next_shared_track;
                    train_pos[train_id].position = next_pos;
                    if (is_slowed) {
                        train_vel[train_id] = original_train_vel[train_id];
                        is_slowed = false;
                    }
                } else {
                    // Failed to acquire mutex, slow down
                    if (!is_slowed) {
                        original_train_vel[train_id] = train_vel[train_id];
                        train_vel[train_id] *= 0.5f;
                        is_slowed = true;
                    }
                }
            } else {
                // Not entering a new shared track or intersection, move normally
                train_pos[train_id].position = next_pos;
                
                // If exiting a shared track
                if (current_shared_track != -1 && is_shared_track(train_pos[train_id].track_id, (int)train_pos[train_id].position) == -1) {
                    pthread_mutex_unlock(&shared_track_mutex[current_shared_track]);
                    current_shared_track = -1;
                }
                
                // If exiting the intersection
                if (in_intersection && !next_in_intersection) {
                    sem_post(&intersection_semaphore);
                    in_intersection = false;
                }
                
                // Gradually increase speed back to original if slowed
                if (is_slowed) {
                    train_vel[train_id] += (original_train_vel[train_id] - train_vel[train_id]) * 0.1f;
                    if (fabsf(train_vel[train_id] - original_train_vel[train_id]) < 0.01f) {
                        train_vel[train_id] = original_train_vel[train_id];
                        is_slowed = false;
                    }
                }
            }
        }
        
        pthread_mutex_unlock(&main_mutex);
        usleep(100000);  // Sleep for 100ms
    }
    
    // Ensure all synchronization primitives are released when quitting
    if (current_shared_track != -1) {
        pthread_mutex_unlock(&shared_track_mutex[current_shared_track]);
    }
    if (in_intersection) {
        sem_post(&intersection_semaphore);
    }
    return NULL;
}

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(100);  // Set getch() timeout to 100ms

    // Initialize shared track mutexes
    for (int i = 0; i < NUM_SHARED_TRACKS; i++) {
        pthread_mutex_init(&shared_track_mutex[i], NULL);
    }

    // Initialize the intersection semaphore
    sem_init(&intersection_semaphore, 0, MAX_TRAINS_IN_INTERSECTION);

    pthread_t render_tid, train_tid[NUM_TRAINS];
    int train_ids[NUM_TRAINS] = {0, 1, 2, 3};

    // Create render thread
    pthread_create(&render_tid, NULL, render_thread, NULL);

    // Create train threads
    for (int i = 0; i < NUM_TRAINS; i++) {
        pthread_create(&train_tid[i], NULL, train_thread, &train_ids[i]);
    }

    while (1) {
        int ch = getch();
        pthread_mutex_lock(&main_mutex);
        switch (ch) {
            case '1': case '2': case '3': case '4':
                active_train = ch - '1';
                break;
            case '+':
                if (train_vel[active_train] < 5.0f) {
                    train_vel[active_train] += 0.1f;
                    original_train_vel[active_train] = train_vel[active_train];
                }
                break;
            case '-':
                if (train_vel[active_train] > -5.0f) {
                    train_vel[active_train] -= 0.1f;
                    original_train_vel[active_train] = train_vel[active_train];
                }
                break;
            case 'p':
                paused = !paused;
                break;
            case 'q':
                quit = true;
                pthread_mutex_unlock(&main_mutex);
                goto cleanup;
        }
        pthread_mutex_unlock(&main_mutex);

        if (paused) {
            clear();
            
            // Draw tracks
            draw_rectangular_track(stdscr, 2, 2, track_width, track_height);   // Track 1
            draw_rectangular_track(stdscr, 2, 15, track_width, track_height);  // Track 2
            draw_rectangular_track(stdscr, 9, 2, track_width, track_height);   // Track 3
            draw_rectangular_track(stdscr, 9, 15, track_width, track_height);  // Track 4

            // Draw trains
            for (int i = 0; i < NUM_TRAINS; i++) {
                int y = (train_pos[i].track_id / 2) * 7 + 2;
                int x = (train_pos[i].track_id % 2) * 13 + 2;
                draw_train(stdscr, y, x, track_width, track_height, train_pos[i].position);
            }

            // Display pause information
            mvprintw(0, 0, "GAME PAUSED");
            for (int i = 0; i < NUM_TRAINS; i++) {
                float next_pos = fmodf(train_pos[i].position + train_vel[i] + track_len, track_len);
                int next_shared = is_shared_track(train_pos[i].track_id, (int)next_pos);
                mvprintw(17+i, 0, "Train %d: Track %d, Pos %.1f, Vel %.2f, Next Shared: %d",
                         i+1, train_pos[i].track_id+1, train_pos[i].position, train_vel[i],
                         next_shared);
            }
            mvprintw(22, 0, "Press 'p' to unpause");
            refresh();
        }
    }

cleanup:
    // Wait for threads to finish
    pthread_join(render_tid, NULL);
    for (int i = 0; i < NUM_TRAINS; i++) {
        pthread_join(train_tid[i], NULL);
    }

    // Destroy shared track mutexes
    for (int i = 0; i < NUM_SHARED_TRACKS; i++) {
        pthread_mutex_destroy(&shared_track_mutex[i]);
    }

    // Destroy the intersection semaphore
    sem_destroy(&intersection_semaphore);

    endwin();
    return 0;
}
