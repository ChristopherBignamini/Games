#include <iostream>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>

using namespace std;

const int WIDTH = 20;
const int HEIGHT = 20;

const int MIN_TIME_LIMIT_SECONDS = 5;
const int MIN_MONSTERS = 1;
const int MIN_BARRIER_SEGMENTS = 1;
const int TIME_DECREASE_PER_LEVEL = 5;

int timeLimitSeconds = 30;
int numMonsters = 3;
int numBarrierSegments = 4;

struct Position {
    int x;
    int y;
};

class RawMode {
private:
    termios original;

public:
    RawMode() {
        tcgetattr(STDIN_FILENO, &original);
        termios raw = original;
        raw.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    ~RawMode() {
        tcsetattr(STDIN_FILENO, TCSANOW, &original);
    }
};

bool samePosition(Position a, Position b) {
    return a.x == b.x && a.y == b.y;
}

bool containsPosition(const vector<Position>& positions, Position p) {
    for (const auto& pos : positions) {
        if (samePosition(pos, p)) return true;
    }
    return false;
}

bool isInsideMap(Position p) {
    return p.x >= 0 && p.x < WIDTH && p.y >= 0 && p.y < HEIGHT;
}

bool inputAvailable() {
    timeval timeout{};
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    return select(STDIN_FILENO + 1, &readfds, nullptr, nullptr, &timeout) > 0;
}

char readInput() {
    char c;
    read(STDIN_FILENO, &c, 1);
    return c;
}

void clearScreen() {
    cout << "\033[2J\033[1;1H";
}

void flushInput() {
    while (inputAvailable()) {
        char dummy;
        read(STDIN_FILENO, &dummy, 1);
    }
}

// void showImageSplash(const string& filename, int durationMs) {
//     clearScreen();

//     ifstream file(filename);
//     string line;

//     if (file.is_open()) {
//         while (getline(file, line)) {
//             cout << line << "\n";
//         }

//         cout.flush();
//         this_thread::sleep_for(chrono::milliseconds(durationMs));
//     }
// }

bool showSplashScreen(
    int& numMonsters,
    int& numBarrierSegments,
    int& timeLimitSeconds
) {
    while (true) {
        clearScreen();

        cout << "=========================================\n";
        cout << "        CACCIA AL TESORO DI EDDIE\n";
        cout << "=========================================\n\n";

        cout << "Obiettivo:\n";
        cout << "Raggiungi il tesoro T prima che scada il tempo.\n";
        cout << "Evita i mostri M e aggira le barriere #.\n\n";

        cout << "Impostazioni iniziali:\n";
        cout << "Mostri:   " << numMonsters << "\n";
        cout << "Barriere: " << numBarrierSegments << " segmenti\n";
        cout << "Tempo:    " << timeLimitSeconds << " secondi\n\n";

        cout << "Modifica difficolta':\n";
        cout << "M = aumenta mostri\n";
        cout << "N = diminuisci mostri\n";
        cout << "B = aumenta barriere\n";
        cout << "V = diminuisci barriere\n";
        cout << "T = diminuisci tempo di 5 secondi\n";
        cout << "Y = aumenta tempo di 1 secondo\n\n";
        
        cout << "Comandi:\n";
        cout << "W/A/S/D = muovi\n";
        cout << "Q = esci\n\n";

        cout << "Premi INVIO per iniziare oppure Q per uscire...";

        char input = readInput();

        if (input == 'm' || input == 'M') {
            numMonsters++;
        } else if (input == 'n' || input == 'N') {
            if (numMonsters > MIN_MONSTERS) {
                numMonsters--;
            }
        } else if (input == 'b' || input == 'B') {
            numBarrierSegments++;
        } else if (input == 'v' || input == 'V') {
            if (numBarrierSegments > MIN_BARRIER_SEGMENTS) {
                numBarrierSegments--;
            }
        } else if (input == 't' || input == 'T') {
            if (timeLimitSeconds > MIN_TIME_LIMIT_SECONDS) {
                timeLimitSeconds -= 5;
                if (timeLimitSeconds < MIN_TIME_LIMIT_SECONDS) {
                    timeLimitSeconds = MIN_TIME_LIMIT_SECONDS;
                }
            }
        } else if (input == 'y' || input == 'Y') {
            timeLimitSeconds++;
        } else if (input == '\n' || input == '\r') {
            return true;
        } else if (input == 'q' || input == 'Q') {
            return false;
        }
    }
}

void drawMap(
    Position player,
    Position treasure,
    const vector<Position>& monsters,
    const vector<Position>& barriers,
    int timeLeft,
    int level,
    const string& message = ""
) {
    clearScreen();

    cout << "=== Caccia al tesoro di Eddie ===\n";
    cout << "Livello: " << level << "\n";
    cout << "Tempo rimanente: " << timeLeft << " secondi\n\n";

    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            Position current{x, y};

            if (samePosition(current, player)) {
                cout << "E ";
            } else if (samePosition(current, treasure)) {
                cout << "T ";
            } else if (containsPosition(monsters, current)) {
                cout << "M ";
            } else if (containsPosition(barriers, current)) {
                cout << "# ";
            } else {
                cout << ". ";
            }
        }
        cout << "\n";
    }

    cout << "\nW/A/S/D = muovi | Q = esci\n";

    if (!message.empty()) {
        cout << "\n" << message << "\n";
    }
}

Position randomStep(Position p, const vector<Position>& barriers) {
    int direction = rand() % 4;
    Position next = p;

    switch (direction) {
        case 0: next.y--; break;
        case 1: next.y++; break;
        case 2: next.x--; break;
        case 3: next.x++; break;
    }

    if (isInsideMap(next) && !containsPosition(barriers, next)) {
        return next;
    }

    return p;
}

bool playerCaught(Position player, const vector<Position>& monsters) {
    return containsPosition(monsters, player);
}

int getLevelTimeLimit(int baseTime, int level) {
    int result = baseTime - (level - 1) * TIME_DECREASE_PER_LEVEL;

    if (result < MIN_TIME_LIMIT_SECONDS) {
        return MIN_TIME_LIMIT_SECONDS;
    }

    return result;
}

void generateBarriers(
    vector<Position>& barriers,
    Position player,
    Position treasure,
    int barrierSegments
) {
    barriers.clear();

    for (int i = 0; i < barrierSegments; i++) {
        bool placed = false;
        int attempts = 0;

        while (!placed && attempts < 1000) {
            attempts++;

            Position start{rand() % WIDTH, rand() % HEIGHT};
            int length = 3 + rand() % 3;
            bool horizontal = rand() % 2;

            vector<Position> temp;

            for (int j = 0; j < length; j++) {
                Position p = start;

                if (horizontal) {
                    p.x += j;
                } else {
                    p.y += j;
                }

                if (
                    !isInsideMap(p) ||
                    samePosition(p, player) ||
                    samePosition(p, treasure) ||
                    containsPosition(barriers, p)
                ) {
                    temp.clear();
                    break;
                }

                temp.push_back(p);
            }

            if (!temp.empty()) {
                barriers.insert(barriers.end(), temp.begin(), temp.end());
                placed = true;
            }
        }
    }
}

void generateMonsters(
    vector<Position>& monsters,
    Position player,
    Position treasure,
    const vector<Position>& barriers,
    int monsterCount
) {
    monsters.clear();

    int attempts = 0;

    while (
        monsters.size() < static_cast<size_t>(monsterCount) &&
        attempts < 10000
    ) {
        attempts++;

        Position p{rand() % WIDTH, rand() % HEIGHT};

        if (
            samePosition(p, player) ||
            samePosition(p, treasure) ||
            containsPosition(barriers, p) ||
            containsPosition(monsters, p)
        ) {
            continue;
        }

        monsters.push_back(p);
    }
}

void countdownBeforeLevel(
    Position player,
    Position treasure,
    const vector<Position>& monsters,
    const vector<Position>& barriers,
    int timeLimit,
    int level
) {
    for (int i = 3; i >= 1; i--) {
        drawMap(
            player,
            treasure,
            monsters,
            barriers,
            timeLimit,
            level,
            "Il livello inizia tra " + to_string(i) + "..."
        );

        this_thread::sleep_for(chrono::seconds(1));
    }
    flushInput();
}

enum class LevelResult {
    Victory,
    Defeat,
    Quit
};

LevelResult playLevel(
    int level,
    int monsterCount,
    int barrierSegments,
    int timeLimit
) {
    Position player{0, 0};
    Position treasure{WIDTH - 1, HEIGHT - 1};

    vector<Position> barriers;
    vector<Position> monsters;

    generateBarriers(barriers, player, treasure, barrierSegments);
    generateMonsters(monsters, player, treasure, barriers, monsterCount);

    countdownBeforeLevel(
        player,
        treasure,
        monsters,
        barriers,
        timeLimit,
        level
    );

    bool running = true;
    bool victory = false;
    bool defeat = false;
    bool timeExpired = false;

    int tick = 0;

    auto startTime = chrono::steady_clock::now();

    while (running) {
        auto currentTime = chrono::steady_clock::now();

        int elapsedSeconds = chrono::duration_cast<chrono::seconds>(
            currentTime - startTime
        ).count();

        int timeLeft = timeLimit - elapsedSeconds;

        if (timeLeft <= 0) {
            timeLeft = 0;
            defeat = true;
            timeExpired = true;
            running = false;
        }

        if (inputAvailable()) {
            char input = readInput();

            Position next = player;
            bool validMove = true;

            switch (input) {
                case 'w':
                case 'W':
                    next.y--;
                    break;

                case 's':
                case 'S':
                    next.y++;
                    break;

                case 'a':
                case 'A':
                    next.x--;
                    break;

                case 'd':
                case 'D':
                    next.x++;
                    break;

                case 'q':
                case 'Q':
                    return LevelResult::Quit;

                default:
                    validMove = false;
                    break;
            }

            if (
                validMove &&
                isInsideMap(next) &&
                !containsPosition(barriers, next)
            ) {
                player = next;
            }
        }

        if (tick % 5 == 0) {
            for (auto& monster : monsters) {
                monster = randomStep(monster, barriers);
            }
        }

        if (samePosition(player, treasure)) {
            victory = true;
            running = false;
        }

        if (playerCaught(player, monsters)) {
            defeat = true;
            running = false;
        }

        drawMap(player, treasure, monsters, barriers, timeLeft, level);

        tick++;

        this_thread::sleep_for(chrono::milliseconds(100));
    }

    clearScreen();

    if (victory) {
        cout << "Livello " << level << " completato!\n";
        cout << "Eddie, preparati al prossimo livello...\n";
        this_thread::sleep_for(chrono::seconds(2));
        return LevelResult::Victory;
    }

    if (timeExpired) {
        cout << "Tempo scaduto Eddie! Hai perso al livello " << level << ".\n";
    } else if (defeat) {
        cout << "Eddie, un mostro ti ha catturato, sei un babbo! Hai perso al livello " << level << ".\n";
    }

    return LevelResult::Defeat;
}

int main() {
    srand(time(nullptr));

    RawMode rawMode;

    // showImageSplash("splash.txt", 3000);

    bool startGame = showSplashScreen(
        numMonsters,
        numBarrierSegments,
        timeLimitSeconds
    );

    if (!startGame) {
        clearScreen();
        cout << "Hai abbandonato.\n";
        return 0;
    }

    int level = 1;

    while (true) {
        int currentMonsterCount = numMonsters + level - 1;
        int currentBarrierSegments = numBarrierSegments + level - 1;
        int currentTimeLimit = getLevelTimeLimit(timeLimitSeconds, level);

        LevelResult result = playLevel(
            level,
            currentMonsterCount,
            currentBarrierSegments,
            currentTimeLimit
        );

        if (result == LevelResult::Victory) {
            level++;
            continue;
        }

        if (result == LevelResult::Quit) {
            clearScreen();
            cout << "Hai abbandonato la partita.\n";
            return 0;
        }

        break;
    }

    cout << "\nGame over.\n";
    cout << "Livello raggiunto: " << level << "\n";

    return 0;
}