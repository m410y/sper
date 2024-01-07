#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_video.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <argp.h>
#include <unistd.h>

const char* argp_program_version = "SPer v0.1";
const char* argp_program_bug_address = "<m410y@proton.me>";
static char documentation[] = "Simple minesweeper.";

#define supAt(i, j) supField[(i + 1) * (fieldWidth + 2) + (j + 1)]
#define at(i, j) minefield[(i) * fieldWidth + (j)]

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

#define SIZE 32
#define MINE 9
#define STEP 10
#define NEWGAME 0
#define GAME 1
#define LOST 2
#define WIN 3

struct Arguments
{
    int fieldWidth;
    int fieldHeight;
    int maxMines;
    int safeRadius;
};

static struct argp_option programOptions[] = {
    {"width", 'w', "8", OPTION_ARG_OPTIONAL, "Minefield width"},
    {"height", 'h', "8", OPTION_ARG_OPTIONAL, "Minefield height"},
    {"mines", 'm', "10", OPTION_ARG_OPTIONAL, "Total mines on minefield"},
    {"safe", 's', "2", OPTION_ARG_OPTIONAL, "Minimal possible distance to mines from start point"}
};

static error_t parseOptions(int key, char* arg, struct argp_state *state)
{
    struct Arguments* arguments = state->input;

    switch (key)
    {
    case 'w':
        arguments->fieldWidth = atoi(arg);
        break;
    case 'h':
        arguments->fieldHeight = atoi(arg);
        break;
    case 'm':
        arguments->maxMines = atoi(arg);
        break;
    case 's':
        arguments->safeRadius = atof(arg);
        break;
    }

    return 0;
}

static struct argp parser = {
    .options = programOptions,
    .parser = parseOptions,
    .doc = documentation,
    .args_doc = NULL,
};

static struct Arguments arguments = {
    .fieldHeight = 8,
    .fieldWidth = 8,
    .maxMines = 10,
    .safeRadius = 2,
};

static bool running;
static int gameState;
static char *minefield;
static int remaindMines;
static int foundMines;
static struct timespec startTime;

static int fieldWidth;
static int fieldHeight;
static int maxMines;
static float safeRadius;

static int width;
static int height;
static SDL_Window* window;
static SDL_Renderer* renderer;

void rightclick(int i, int j);
void leftclick(int i, int j);

void generateField(int restrictI, int restrictJ)
{
    char *supField = malloc((fieldHeight + 2) * (fieldWidth + 2));
    memset(supField, 0, (fieldHeight + 2)*(fieldWidth + 2));

    int safeTiles = 0;
    for (int i = max(0, floor(restrictI - safeRadius)); i <= min(fieldHeight - 1, ceil(restrictI + safeRadius)); ++i)
        for (int j = max(0, floor(restrictJ - safeRadius)); j <= min(fieldWidth - 1, ceil(restrictJ + safeRadius)); ++j)
        {
            if ((i - restrictI) * (i - restrictI) + (j - restrictJ) * (j - restrictJ) <= safeRadius * safeRadius)
            {
                supAt(i, j) += MINE;
                ++safeTiles;
            }
        }

    if (fieldHeight * fieldWidth - safeTiles < maxMines)
    {
        maxMines = fieldWidth * fieldHeight - safeTiles;
        printf("Number of mines was adjusted to fit in safe area.\n");
        for (int i = 0; i < fieldHeight; ++i)
            for (int j = 0; j < fieldWidth; ++j)
            {
                if (supAt(i, j) >= MINE)
                    continue;

                supAt(i, j) += MINE;

                ++supAt(i - 1, j - 1);
                ++supAt(i - 1, j + 0);
                ++supAt(i - 1, j + 1);
                ++supAt(i + 0, j - 1);
                ++supAt(i + 0, j + 1);
                ++supAt(i + 1, j - 1);
                ++supAt(i + 1, j + 0);
                ++supAt(i + 1, j + 1);
            }
    }
    else
    {
        for (int planted = 0; planted < maxMines; ++planted)
        {
            int i, j;
            do
            {
                i = rand() % fieldHeight;
                j = rand() % fieldWidth;
            } while (supAt(i, j) >= MINE);

            supAt(i, j) += MINE;

            ++supAt(i - 1, j - 1);
            ++supAt(i - 1, j + 0);
            ++supAt(i - 1, j + 1);
            ++supAt(i + 0, j - 1);
            ++supAt(i + 0, j + 1);
            ++supAt(i + 1, j - 1);
            ++supAt(i + 1, j + 0);
            ++supAt(i + 1, j + 1);
        }
    }

    for (int i = max(0, floor(restrictI - safeRadius)); i <= min(fieldHeight - 1, ceil(restrictI + safeRadius)); ++i)
        for (int j = max(0, floor(restrictJ - safeRadius)); j <= min(fieldWidth - 1, ceil(restrictJ + safeRadius)); ++j)
        {
            if ((i - restrictI) * (i - restrictI) + (j - restrictJ) * (j - restrictJ) <= safeRadius * safeRadius)
                supAt(i, j) -= MINE;
        }

    for (int i = 0; i < fieldHeight; ++i)
        for (int j = 0; j < fieldWidth; ++j)
        {
            if (supAt(i, j) > MINE) supAt(i, j) = MINE;
            at(i, j) = supAt(i, j) + STEP;
        }

    free(supField);
}

void openTile(int i, int j)
{
    if (at(i, j) <= MINE || 19 <= at(i, j))
        return;

    at(i, j) -= STEP;

    if (at(i, j) != 0)
        return;

    if (i > 0 && at(i - 1, j)) openTile(i - 1, j);
    if (i < fieldHeight - 1 && at(i + 1, j)) openTile(i + 1, j);
    if (j > 0 && at(i, j - 1)) openTile(i, j - 1);
    if (j < fieldWidth - 1 && at(i, j + 1)) openTile(i, j + 1);

    if (i > 0 && j > 0 && at(i - 1, j) < MINE && at(i, j - 1) < MINE) openTile(i - 1, j - 1);
    if (i < fieldHeight - 1 && j > 0 && at(i + 1, j) < MINE && at(i, j - 1) < MINE) openTile(i + 1, j - 1);
    if (i > 0 && j < fieldWidth - 1 && at(i - 1, j) < MINE && at(i, j + 1) < MINE) openTile(i - 1, j + 1);
    if (i < fieldHeight - 1 && j < fieldWidth - 1 && at(i + 1, j) < MINE && at(i, j + 1) < MINE) openTile(i + 1, j + 1);
}

void autoOpen(int I, int J)
{
    int freeTiles = 0;
    int minedTiles = 0;

    for (int i = max(0, I - 1); i <= min(I + 1, fieldHeight - 1); ++i)
        for (int j = max(0, J - 1); j <= min(J + 1, fieldWidth - 1); ++j)
        {
            if (i == I && j == J)
                continue;

            if (at(i, j) >= 3 * STEP)
                return;
            else if (at(i, j) >= 2 * STEP)
                ++minedTiles;
            else if (at(i, j) >= STEP)
                ++freeTiles;
        }

    for (int i = max(0, I - 1); i <= min(I + 1, fieldHeight - 1); ++i)
        for (int j = max(0, J - 1); j <= min(J + 1, fieldWidth - 1); ++j)
        {
            if (i == I && j == J)
                continue;

            if (STEP <= at(i, j) && at(i, j) < 2 * STEP)
            {
                if (at(I, J) == minedTiles)
                    leftclick(i, j);
                else if (at(I, J) == freeTiles + minedTiles)
                    rightclick(i, j);
            }
        }
}

void printGameParams(void)
{
    struct timespec endTime;
    clock_gettime(CLOCK_REALTIME, &endTime);
    float minesDensity =  (float)(fieldHeight * fieldWidth) / (float)maxMines;
    float elapsedTime = (endTime.tv_sec - startTime.tv_sec) + (endTime.tv_nsec - startTime.tv_nsec) * 1e-9;
    printf("Game parameters:\n\tField width: %d\n\tField height %d\n\tTotal mines: %d\n\tSafe start distance: %f\n\tMine density: %f tiles/mine\n\tElapsed time: %f seconds\n", fieldWidth, fieldHeight, maxMines, safeRadius, minesDensity, elapsedTime);
}

void startNewGame(void)
{
    gameState = NEWGAME;

    maxMines = arguments.maxMines;

    bool needRealloc = fieldHeight != arguments.fieldHeight || fieldWidth != arguments.fieldWidth;

    if (needRealloc)
    {
        fieldHeight = arguments.fieldHeight;
        fieldWidth = arguments.fieldWidth;

        minefield = realloc(minefield, fieldHeight * fieldWidth);

        width = SIZE * fieldWidth;
        height = SIZE * fieldHeight;
        SDL_SetWindowSize(window, width, height);
    }

    memset(minefield, STEP, fieldWidth * fieldHeight);
    foundMines = 0;
    remaindMines = maxMines;
}

void changeArguments(void)
{
    char* line;
    printf("Which value you want to change? [M/w/h/d]: ");
    scanf("%ms", &line);
    switch (tolower(line[0]))
    {
    case 'm':
        printf("Enter new mines count (last %d): ", arguments.maxMines);
        scanf("%d", &arguments.maxMines);
        break;

    case 'w':
        printf("Enter new field width (last %d): ", arguments.fieldWidth);
        scanf("%d", &arguments.fieldWidth);
        break;

    case 'h':
        printf("Enter new field height (last %d): ", arguments.fieldHeight);
        scanf("%d", &arguments.fieldHeight);
        break;

    case 'd':
        printf("Enter new mine density (last %f): ", (float)(arguments.fieldHeight * arguments.fieldWidth) / (float)arguments.maxMines);
        float minesDensity;
        scanf("%f", &minesDensity);
        arguments.maxMines = arguments.fieldHeight * arguments.fieldWidth / minesDensity;
        break;
    }

    free(line);
    printf("Value was updated! Starting new game\n");
    startNewGame();
}

void rightclick(int i, int j)
{
    if (gameState == WIN || gameState == LOST || gameState == NEWGAME)
        return;

    switch (at(i, j) / STEP)
    {
    case 0:
        autoOpen(i, j);
        break;
    case 1:
        if (remaindMines)
        {
            --remaindMines;
            if (at(i, j) % STEP == MINE)
                ++foundMines;

            at(i, j) += STEP;
        }
        else
            at(i, j) += 2 * STEP;

        break;
    case 2:
        ++remaindMines;
        if (at(i, j) % STEP == MINE)
            --foundMines;

        at(i, j) += STEP;
        break;
    case 3:
        at(i, j) -= 2 * STEP;
        break;
    }
}

void leftclick(int i, int j)
{
    if (gameState == WIN || gameState == LOST)
        return;

    if (gameState == NEWGAME)
    {
        generateField(i, j);
        foundMines = 0;
        remaindMines = maxMines;
        openTile(i, j);
        gameState = GAME;
        clock_gettime(CLOCK_REALTIME, &startTime);
        return;
    }

    switch (at(i, j) / STEP)
    {
    case 0:
        break;
    case 1:
        if (at(i, j) == MINE + STEP)
        {
            for (int i = 0; i < fieldHeight; ++i)
                for (int j = 0; j < fieldWidth; ++j)
                    if (at(i, j) == MINE + STEP || at(i, j) == MINE + 3 * STEP)
                        at(i, j) = MINE;

            gameState = LOST;
            printf("You lost!\n");
        }
        else
            openTile(i, j);
        break;
    case 2:
        ++remaindMines;
        if (at(i, j) % STEP == MINE)
            --foundMines;

        at(i, j) -= STEP;
        break;
    case 3:
        if (remaindMines)
        {
            --remaindMines;
            if (at(i, j) % STEP == MINE)
                ++foundMines;

            at(i, j) -= STEP;
        }
        else
            at(i, j) -= 2 * STEP;
        break;
    }
}

void processKeysym(SDL_Keycode sym)
{
    switch (sym)
    {
    case SDLK_ESCAPE:
        running = false;
        break;

    case SDLK_m:
        printf("%d Mine flags remaining\n", remaindMines);
        break;

    case SDLK_c:
        changeArguments();
        break;

    case SDLK_r:
        startNewGame();
        break;

    case SDLK_p:
        printGameParams();
    }
}

int main(int argc, char** argv)
{
    argp_parse(&parser, argc, argv, 0, NULL, &arguments);

    srand(time(NULL));

    maxMines = arguments.maxMines;
    safeRadius = arguments.safeRadius;
    fieldWidth = arguments.fieldWidth;
    fieldHeight = arguments.fieldHeight;
    minefield = malloc(fieldWidth * fieldHeight);
    memset(minefield, STEP, fieldHeight * fieldWidth);

    width = fieldWidth * SIZE;
    height = fieldHeight * SIZE;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(width, height, 0, &window, &renderer);
    SDL_SetWindowTitle(window, "SPer");

    SDL_Surface* sprites = IMG_Load("sprites.png");
    SDL_Rect spriteRects[] = {
        {.x = SIZE*0, .y = SIZE*2, .w = SIZE, .h = SIZE},
        {.x = SIZE*0, .y = SIZE*0, .w = SIZE, .h = SIZE},
        {.x = SIZE*1, .y = SIZE*0, .w = SIZE, .h = SIZE},
        {.x = SIZE*2, .y = SIZE*0, .w = SIZE, .h = SIZE},
        {.x = SIZE*3, .y = SIZE*0, .w = SIZE, .h = SIZE},
        {.x = SIZE*0, .y = SIZE*1, .w = SIZE, .h = SIZE},
        {.x = SIZE*1, .y = SIZE*1, .w = SIZE, .h = SIZE},
        {.x = SIZE*2, .y = SIZE*1, .w = SIZE, .h = SIZE},
        {.x = SIZE*3, .y = SIZE*1, .w = SIZE, .h = SIZE},
        {.x = SIZE*1, .y = SIZE*2, .w = SIZE, .h = SIZE},
        {.x = SIZE*2, .y = SIZE*2, .w = SIZE, .h = SIZE},
        {.x = SIZE*3, .y = SIZE*2, .w = SIZE, .h = SIZE},
        {.x = SIZE*0, .y = SIZE*3, .w = SIZE, .h = SIZE},
        {.x = SIZE*1, .y = SIZE*3, .w = SIZE, .h = SIZE},
        {.x = SIZE*2, .y = SIZE*3, .w = SIZE, .h = SIZE},
        {.x = SIZE*3, .y = SIZE*3, .w = SIZE, .h = SIZE},
    };

    gameState = NEWGAME;
    running = true;
    while (running)
    {
        int mouseState = 0;
        int mouseI, mouseJ;
        if (SDL_GetMouseFocus() == window)
        {
            int mouseX, mouseY;
            mouseState = SDL_GetMouseState(&mouseX, &mouseY);
            mouseState &= ~2;
            mouseJ = mouseX / SIZE;
            mouseI = mouseY / SIZE;
        }
        else
        {
            mouseI = -1;
            mouseJ = -1;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                running = false;

            if (event.key.type == SDL_KEYDOWN)
                processKeysym(event.key.keysym.sym);

            if (event.type == SDL_MOUSEBUTTONUP)
            {
                if ( !(0 <= mouseI && mouseI < fieldHeight) || !(0 <= mouseJ && mouseJ < fieldWidth) )
                    continue;

                if (event.button.button == SDL_BUTTON_LEFT)
                    leftclick(mouseI, mouseJ);
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    rightclick(mouseI, mouseJ);
            }
        }

        if (foundMines == maxMines && gameState == GAME)
        {
            gameState = WIN;
            printf("You won!\n");
            printGameParams();
            for (int i = 0; i < fieldHeight; ++i)
                for (int j = 0; j < fieldWidth; ++j)
                    if (at(i, j) % STEP < MINE)
                        at(i, j) %= STEP;
        }

        for (int i = 0; i < fieldHeight; ++i)
            for (int j = 0; j < fieldWidth; ++j)
            {
                SDL_Rect windowRect = {
                    .x = SIZE*j,
                    .y = SIZE*i,
                    .w = SIZE,
                    .h = SIZE,
                };

                int spriteNum = at(i, j);
                switch (spriteNum / STEP)
                {
                case 0:
                    break;
                case 1:
                    spriteNum = 10;
                    break;
                case 2:
                    spriteNum = 12;
                    break;
                case 3:
                    spriteNum = 14;
                    break;
                }
                if (spriteNum > MINE && mouseI == i && mouseJ == j)
                    ++spriteNum;

                SDL_BlitSurface(sprites, &spriteRects[spriteNum], SDL_GetWindowSurface(window), &windowRect);
            }
        SDL_UpdateWindowSurface(window);

        if (running) SDL_WaitEvent(NULL);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    free(minefield);

    return 0;
}
