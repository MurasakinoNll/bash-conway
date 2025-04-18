#ifdef _WIN32
#define NCURSES_MOUSE_VERSION
#define PDC_WIDE
#define PDC_FORCE_UTF8
#include <pdcurses.h>
#include <windows.h>
#else
#include <ncurses.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
void sleep_us(long microseconds) {
    Sleep((unsigned int)(microseconds / 1000));
}
#else
void sleep_us(long microseconds) {
    usleep((useconds_t)microseconds);
}
#endif

int rows, cols;
char **grid;
char **next_grid;

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
        grid[i][cols] = '\0';
        next_grid[i][cols] = '\0';
    }
}

void free_grid() {
    for (int i = 0; i < rows; i++) {
        free(grid[i]);
        free(next_grid[i]);
    }
    free(grid);
    free(next_grid);
}

void draw_grid(const char *instruction) {
    for (int i = 0; i < rows; i++) {
        mvaddnstr(i, 0, grid[i], cols);
    }
    mvprintw(rows, 0, "%s", instruction);
    clrtoeol();
    refresh();
}

int count_neighbors(int r, int c) {
    int count = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0)
                continue;
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

void update_generation() {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int neighbors = count_neighbors(i, j);
            if (grid[i][j] == '#') {
                if (neighbors == 2 || neighbors == 3)
                    next_grid[i][j] = '#';
                else
                    next_grid[i][j] = '-';
            } else {
                if (neighbors == 3)
                    next_grid[i][j] = '#';
                else
                    next_grid[i][j] = '-';
            }
        }
    }
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            grid[i][j] = next_grid[i][j];
        }
    }
}

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
    sleep_us(2000000);
}

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

int main(int argc, char **argv) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

#ifndef _WIN32
    printf("\033]50;22\007");
#endif

    mousemask(ALL_MOUSE_EVENTS, NULL);

    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);
    rows = screen_rows - 1;
    cols = screen_cols;

    init_grid();

    if (argc >= 3 && strcmp(argv[1], "--import") == 0) {
        import_grid_from_file(argv[2]);
    }

    draw_grid("setup: click to toggle cell (#). press 's' to start simulation, 'w' to save setup.");

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
                if (event.y >= 0 && event.y < rows && event.x >= 0 && event.x < cols) {
                    if (grid[event.y][event.x] == '-')
                        grid[event.y][event.x] = '#';
                    else
                        grid[event.y][event.x] = '-';
                    draw_grid("setup: click to toggle cell (#). press 's' to start simulation, 'w' to save setup.");
                }
            }
        }
    }

    nodelay(stdscr, TRUE);
    bool paused = false;
    while (1) {
        ch = getch();
        if (ch == 'q')
            break;
        if (ch == 'p')
            paused = !paused;

        if (!paused) {
            update_generation();
            draw_grid("simulation running. press 'p' to pause/resume, 'q' to quit.");
            sleep_us(100000);
        } else {
            draw_grid("simulation paused. press 'p' to resume, 'q' to quit.");
            sleep_us(100000);
        }
    }

    free_grid();
    endwin();
    return 0;
}

