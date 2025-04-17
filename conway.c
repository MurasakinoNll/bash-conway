#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
// Conway game of life mimic using C, with the terminal being used as a renderer (see https://playgameoflife.com/)

// global variables that hold the size of the grid and pointers to the grids
int rows, cols;      // these are the dimensions for the game grid (all lines except the bottom one)
char **grid;         // this is the current grid showing which cells are alive ('#') or dead ('-')
char **next_grid;    // this grid is used as a workspace to calculate the next generation

// this function allocates memory for the grid arrays and fills them with '-'
// each cell is marked as dead initially (represented by a - character)
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

// this function frees the memory we allocated for the grid arrays, this way the program does not reserve any memory after exiting
void free_grid() {
    for (int i = 0; i < rows; i++) {
        free(grid[i]);
        free(next_grid[i]);
    }
    free(grid);
    free(next_grid);
}

// this function writes the current grid state to the screen
// it loops for each line then prints the contents,  after that it prints a message on the bottom line for instructions
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
// this is the point of conway's game of life as this simple set of rules allows for many possibilities
int count_neighbors(int r, int c) {
    int count = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0)
                continue;  // skip the cell itself so no deadlocks occur
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
// it uses conway's rules: a live cell with 2 or 3 neighbors lives on, and a dead cell with 3 neighbors becomes alive
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
    // copy the computed next generation back into our main grid for next iteration
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            grid[i][j] = next_grid[i][j];
        }
    }
}

// this function saves the current grid state to a file specified by filename
// it writes each row of the grid into the file so it can be reloaded later, there is no file format specifically needed
void save_grid_to_file(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        mvprintw(rows, 0, "error: unable to save file: %s", filename);
        clrtoeol();
        refresh();
        usleep(2000000);
        return;
    }
    for (int i = 0; i < rows; i++) {
        fprintf(fp, "%s\n", grid[i]);
    }
    fclose(fp);
    mvprintw(rows, 0, "grid saved to %s", filename);
    clrtoeol();
    refresh();
    usleep(2000000); // pause a bit to let the user see the message
}

// this function imports a grid from a file specified by filename
// it reads each line from the file and loads it into our grid, marking any non- '#' as dead ('-')
void import_grid_from_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        mvprintw(rows, 0, "error: could not import file: %s", filename);
        clrtoeol();
        refresh();
        usleep(2000000);
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

// the main function sets up the terminal, handles user input for setup and simulation, and cleans up before exiting
// however no calculations are done here
int main(int argc, char **argv) {
    // start ncurses mode so we can control the terminal appearance
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);  // hide the default cursor

    // try an escape sequence to set the font size to 22 (this might not work on windows though)
    printf("\033]50;22\007");

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
    // however i have yet to add a --help arg to list available arguements
    if (argc >= 3 && strcmp(argv[1], "--import") == 0) {
        import_grid_from_file(argv[2]);
    }

    // show the grid and instructions in the setup phase
    draw_grid("setup: click to toggle cell (#). press 's' to start simulation, 'w' to save setup.");

    // this part is where clicks are registered to cell lcoations and their state is modified, where clicking a '#' switches it to '-' and vice versa
    // it also lets the user press 'w' to save the current setup
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
                // only respond if the click is inside our grid area otherwise unexpected errors can happen
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

    // this is the run phase that repeats every 100000 milliseconds, where it updates the grid from each generation
    // i set non-blocking mode so it can check for key presses while the simulation runs
    nodelay(stdscr, TRUE);
    bool paused = false;
    while (1) {
        ch = getch();
        if (ch == 'q')
            break;  // quit the simulation if the user presses 'q'
        if (ch == 'p')
            paused = !paused;  // toggle pause state with 'p'
        
        if (!paused) {
            update_generation();  // calculate the next generation of cells
            draw_grid("simulation running. press 'p' to pause/resume, 'q' to quit.");
            usleep(100000);  // wait a bit (0.1 seconds) before updating again
        } else {
            draw_grid("simulation paused. press 'p' to resume, 'q' to quit.");
            usleep(100000);  // still wait to avoid using too much cpu
        }
    }

    // clean up memory and end ncurses mode so the terminal returns to normal
    free_grid();
    endwin();
    return 0;
}

