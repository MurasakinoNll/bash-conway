#ifdef _WIN32
    #include <curses.h>    // using pdcurses on windows
    #include <windows.h>   // for Sleep
#else
    #include <ncurses.h>   // using ncurses on unix-like systems
    #include <unistd.h>    // for usleep
#endif

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// define a sleep helper function that sleeps for the given microseconds
#ifdef _WIN32
void sleep_us(long microseconds) {
    Sleep((unsigned int)(microseconds / 1000));  // Sleep() takes milliseconds
}
#else
void sleep_us(long microseconds) {
    usleep((useconds_t)microseconds);
}
#endif

// global variables that hold the size of the grid and pointers to the grids
int rows, cols;      // these are the dimensions for the game grid (all lines except the bottom one)
char **grid;         // this is the current grid showing which cells are alive ('#') or dead ('-')
char **next_grid;    // this grid is used as a workspace to calculate the next generation

// this function allocates memory for the grid arrays and fills them with '-'
// each cell is marked as dead initially (represented by a '-' character)
void init_grid() {
    grid = malloc(rows * sizeof(char *));
    next_grid = malloc(rows * sizeof(char *));
    for (int i = 0; i < rows; i++) {
        grid[i] = malloc((cols + 1) * sizeof(char));
        next_grid[i] = malloc((cols + 1) * sizeof(char));
        for (int j = 0; j < cols; j++) {
            grid[i][j] = '-';
            next_grid[i][j] = '-';
        }
        grid[i][cols] = '\0';       // mark the end of the string in this row
        next_grid[i][cols] = '\0';  // do the same for the next generation buffer
    }
}

// this function frees the memory we allocated for the grid arrays so the program doesn't reserve memory after exiting
void free_grid() {
    for (int i = 0; i < rows; i++) {
        free(grid[i]);
        free(next_grid[i]);
    }
    free(grid);
    free(next_grid);
}

// this function writes the current grid state to the screen
// it loops through each line and prints the contents, then prints an instruction message on the bottom line
void draw_grid(const char *instruction) {
    for (int i = 0; i < rows; i++) {
        mvaddnstr(i, 0, grid[i], cols);
    }
    mvprintw(rows, 0, "%s", instruction);
    clrtoeol();
    refresh();
}

// this function counts how many alive neighbors a given cell at (r, c) has
// it checks the 8 surrounding cells and counts a neighbor only if it contains a '#'
int count_neighbors(int r, int c) {
    int count = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0)
                continue;  // skip the cell itself
            int nr = r + dr;
            int nc = c + dc;
            if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
                if (grid[nr][nc] == '#')
                    count++;
            }
        }
    }
    return count;
}

// this function calculates the next generation of cells for the entire grid
// it applies conway's rules: a live cell with 2 or 3 neighbors lives on, and a dead cell with 3 neighbors becomes alive
void update_generation() {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int neighbors = count_neighbors(i, j);
            if (grid[i][j] == '#') {
                if (neighbors == 2 || neighbors == 3)
                    next_grid[i][j] = '#';  // cell stays alive
                else
                    next_grid[i][j] = '-';  // cell dies
            } else {
                if (neighbors == 3)
                    next_grid[i][j] = '#';  // cell becomes alive
                else
                    next_grid[i][j] = '-';  // cell remains dead
            }
        }
    }
    // copy the computed next generation back into our main grid for the next iteration
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            grid[i][j] = next_grid[i][j];
        }
    }
}

// this function saves the current grid state to a file specified by filename
// it writes each row of the grid into the file so it can be reloaded later without a specific format requirement
void save_grid_to_file(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        mvprintw(rows, 0, "error: unable to save file: %s", filename);
        clrtoeol();
        refresh();
        sleep_us(2000000);
        return;
    }
    for (int i = 0; i < rows; i++) {
        fprintf(fp, "%s\n", grid[i]);
    }
    fclose(fp);
    mvprintw(rows, 0, "grid saved to %s", filename);
    clrtoeol();
    refresh();
    sleep_us(2000000); // pause a bit to let the user see the message
}

// this function imports a grid from a file specified by filename
// it reads each line from the file and loads it into our grid, marking any non-'#' as dead ('-')
void import_grid_from_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        mvprintw(rows, 0, "error: could not import file: %s", filename);
        clrtoeol();
        refresh();
        sleep_us(2000000);
        return;
    }
    char buffer[1024];
    for (int i = 0; i < rows; i++) {
        if (fgets(buffer, sizeof(buffer), fp) == NULL)
            break;
        int len = strlen(buffer);
        // remove newline if present
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }
        for (int j = 0; j < cols; j++) {
            if (j < len) {
                if (buffer[j] == '#')
                    grid[i][j] = '#';
                else
                    grid[i][j] = '-';
            } else {
                grid[i][j] = '-';
            }
        }
        grid[i][cols] = '\0';
    }
    fclose(fp);
}

// the main function sets up the terminal, handles user input for both setup and simulation, and cleans up before exiting
int main(int argc, char **argv) {
    // start curses mode so we can control the terminal appearance
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);  // hide the default cursor

    // try an escape sequence to set the font size to 22 (this might not work on windows)
#ifndef _WIN32
    printf("\033]50;22\007");
#endif

    // enable mouse events so clicks can be captured
    mousemask(ALL_MOUSE_EVENTS, NULL);

    // get the size of the terminal window and set our grid size
    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);
    rows = screen_rows - 1;  // use all rows except the bottom one for the grid
    cols = screen_cols;      // use the full width

    // allocate and initialize our grid of cells (all starting as dead '-')
    init_grid();

    // if the user provided an import file parameter, try to load it into the grid now
    // (note: currently there is no --help argument; available arguments: --import <filename>)
    if (argc >= 3 && strcmp(argv[1], "--import") == 0) {
        import_grid_from_file(argv[2]);
    }

    // show the grid and instructions in the setup phase
    draw_grid("setup: click to toggle cell (#). press 's' to start simulation, 'w' to save setup.");

    // setup phase: register clicks to modify cell states and allow saving the setup
    int ch;
    while ((ch = getch()) != 's') {
        if (ch == 'w') {
            echo();
            char filename[256];
            mvprintw(rows, 0, "enter filename to save: ");
            clrtoeol();
            refresh();
            getnstr(filename, 255);
            noecho();
            save_grid_to_file(filename);
            draw_grid("setup: click to toggle cell (#). press 's' to start simulation, 'w' to save setup.");
        } else if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK) {
                // only respond if the click is inside our grid area
                if (event.y >= 0 && event.y < rows && event.x >= 0 && event.x < cols) {
                    // toggle the cell: if it's dead ('-'), make it alive ('#'); if alive, set to dead ('-')
                    if (grid[event.y][event.x] == '-')
                        grid[event.y][event.x] = '#';
                    else
                        grid[event.y][event.x] = '-';
                    draw_grid("setup: click to toggle cell (#). press 's' to start simulation, 'w' to save setup.");
                }
            }
        }
    }

    // simulation phase: update the grid based on conway's game of life rules
    // use non-blocking input so the simulation can check for key presses during the run
    nodelay(stdscr, TRUE);
    bool paused = false;
    while (1) {
        ch = getch();
        if (ch == 'q')
            break;  // quit simulation if the user presses 'q'
        if (ch == 'p')
            paused = !paused;  // toggle pause state with 'p'
        
        if (!paused) {
            update_generation();  // calculate the next generation of cells
            draw_grid("simulation running. press 'p' to pause/resume, 'q' to quit.");
            sleep_us(100000);  // wait 0.1 seconds before updating again
        } else {
            draw_grid("simulation paused. press 'p' to resume, 'q' to quit.");
            sleep_us(100000);  // still wait a bit to reduce CPU usage
        }
    }

    // clean up memory and end curses mode so the terminal returns to normal
    free_grid();
    endwin();
    return 0;
}

