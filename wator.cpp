#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <SFML/Graphics.hpp>
#include <time.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <boost/program_options.hpp>
#include <filesystem>
#include <condition_variable>

namespace po = boost::program_options;
namespace fs = std::filesystem;

const int SHARK_TYPE = 1;
const int FISH_TYPE = 2;
const sf::Color emptyCell(3, 23, 252);
const sf::Color fishColor(255, 110, 204);
const sf::Color sharkColor(255, 238, 0);
const int animDelay = 100;
const std::string outputDir = "images-output";

int gridX = 1920;
int gridY = 1080; 
int cellSize = 5;
int Nx = gridX / cellSize;
int Ny = gridY / cellSize;
int simulationDuration = 100;
int sharksN = 8000;
int fishesN = 50000;
int fishReplicate = 15;
int sharkReplicate = 8;
int sharkStarve = 10;
int simulationAge = 0;
int threads = 1;
bool isDryRun = false;
bool makeGif = false;

class Cell {
public:
    int type;
    int age = 0;
    sf::Color color;

    Cell(int _type, sf::Color _color) : type(_type), color(_color) {
        age = simulationAge;
    };

    virtual void update(std::vector<std::vector<Cell*>>&, int, int) = 0;
};

class Shark : public Cell {
public:
    static const std::vector<std::pair<int, int>> directions;
    int starveTime;
    int replicateTime;
    int replicateCounter = 0;
    int starveCounter = 0;

    Shark(int _starveTime, int _replicateTime) : Cell(SHARK_TYPE, sharkColor), starveTime(_starveTime), replicateTime(_replicateTime) { };

    void update(std::vector<std::vector<Cell*>> &grid, int x, int y) {
        if (age > simulationAge) {
            return;
        }

        std::vector<std::pair<int, int>> fishSpaces;
        std::vector<std::pair<int, int>> freeSpaces;

        //for (int i = x - 1; i <= x + 1; i++) {
            //for (int k = y - 1; k <= y + 1; k++) {
            for (auto it = directions.begin(); it != directions.end(); ++it) {
                /*if (abs(i-x) == abs(k-y)) {
                    continue;
                }*/
                int dx = x + (*it).first;
                int dy = y + (*it).second;

                int iClamped = dx == -1 ? Nx - 1 : dx % Nx;
                int kClamped = dy == -1 ? Ny - 1 : dy % Ny;

                if (grid[iClamped][kClamped] == NULL) {
                    freeSpaces.push_back(std::pair<int, int>(iClamped, kClamped));
                } else if (grid[iClamped][kClamped]->type == FISH_TYPE) {
                    fishSpaces.push_back(std::pair<int, int>(iClamped, kClamped));
                }
            }
        //} 

        replicateCounter++;
        starveCounter++;

        if (fishSpaces.size() > 0) {
            int i = rand() % fishSpaces.size();
            grid[fishSpaces[i].first][fishSpaces[i].second] = grid[x][y];
            grid[x][y] = NULL;
            starveCounter = 0;
        } else if (starveCounter == starveTime) {
            grid[x][y] = NULL;
        } else if (freeSpaces.size() > 0) {
            int i = rand() % freeSpaces.size();
            grid[freeSpaces[i].first][freeSpaces[i].second] = grid[x][y];
            grid[x][y] = NULL;
        }

        if (replicateCounter > replicateTime) {
            grid[x][y] = new Shark(sharkStarve, sharkReplicate);
            replicateCounter = 0;
        }

        age++;
    }
};

class Fish : public Cell {
public:
    static const std::vector<std::pair<int, int>> directions;
    int replicateTime;
    int replicateCounter = 0;

    Fish(int _replicateTime) : Cell(FISH_TYPE, fishColor), replicateTime(_replicateTime) {};

    void update(std::vector<std::vector<Cell*>> &grid, int x, int y) {
        if (age > simulationAge) {
            return;
        }

        std::vector<std::pair<int, int>> freeSpaces;

        //for (int i = x - 1; i <= x + 1; i++) {
            //for (int k = y - 1; k <= y + 1; k++) {
            for (auto it = directions.begin(); it != directions.end(); ++it) {
                //std::cout << abs(i-x) << ' ' << abs(k-y) << std::endl;
                /*if (abs(i-x) == abs(k-y)) {
                    continue;
                }*/
                int dx = x + (*it).first;
                int dy = y + (*it).second;

                int iClamped = dx == -1 ? Nx - 1 : dx % Nx;
                int kClamped = dy == -1 ? Ny - 1 : dy % Ny;
                //std::cout << "indexes " << i << ' ' << k << std::endl;
                //std::cout << iClamped << ' ' << kClamped << std::endl;

                if (grid[iClamped][kClamped] == NULL) {
                    freeSpaces.push_back(std::pair<int, int>(iClamped, kClamped));
                }
            }
        //} 

        replicateCounter++;

        //std::cout << freeSpaces.size() << std::endl;
        if (freeSpaces.size() == 0) {
            return;
        }

        int i = rand() % freeSpaces.size();
        grid[freeSpaces[i].first][freeSpaces[i].second] = grid[x][y];
        grid[x][y] = NULL;
        if (replicateCounter > replicateTime) {
            grid[x][y] = new Fish(fishReplicate);
            replicateCounter = 0;
        }

        age++;
    }
};
const std::vector<std::pair<int,int>> Shark::directions = {std::pair<int, int>(0, 1), std::pair<int, int>(1, 0), std::pair<int, int>(0, -1), std::pair<int, int>(-1, 0) };
const std::vector<std::pair<int,int>> Fish::directions = {std::pair<int, int>(0, 1), std::pair<int, int>(1, 0), std::pair<int, int>(0, -1), std::pair<int, int>(-1, 0) };

std::vector<std::vector<Cell*>> grid;
void initializeSimulation() {

    grid.reserve(Nx);

    for (int i = 0; i < Nx; i++) {
        std::vector<Cell*> row (Ny, NULL);
        grid.push_back(row);
    }

    for (int i = 0; i < fishesN; i++) {
        int x = rand() % Nx;
        int y = rand() % Ny;

        if (grid[x][y] == NULL) {
            grid[x][y] = new Fish(fishReplicate);
        } else {
            i--;
        }
    }

    for (int i = 0; i < sharksN; i++) {
        int x = rand() % Nx;
        int y = rand() % Ny;

        if (grid[x][y] == NULL) {
            grid[x][y] = new Shark(sharkStarve, fishReplicate);
        } else {
            i--;
        }
    }

    //std::cout << "Setup finished" << std::endl;
}

std::vector<int> threadAges;
std::vector<std::condition_variable> thread_signals;
std::vector<std::mutex> rowMutexes;
std::vector<std::mutex> updatedMutexes;

std::mutex globalMutex;
std::unique_lock globalLk(globalMutex);
std::condition_variable globalCv;
int completedThreads = 0;

void computeRegion(int index, sf::RenderWindow *window) 
{
    int regionWidth = Nx / threads;
    int startRow = regionWidth * index;
    int endRow = index == threads - 1 ? Nx : startRow + regionWidth;

    //std::cout << "Thread " << index << " computing from row " << startRow << " to " << endRow << std::endl;
    sf::Vector2u windowSizes = window->getSize();
    int prevIndex = index-1 == -1 ? threads - 1 : index-1;
    int nextIndex = index+1 == threads ? 0 : index+1;

    for (int age = 0; age < simulationDuration; age++) {
        rowMutexes[index].lock();
        for (int y = 0; y < Ny; y++) {
            if (grid[startRow][y] != NULL) {
                grid[startRow][y]->update(grid, startRow, y);
            }
        }
        //updatedMutexes[index].unlock();
        rowMutexes[index].unlock();
        /*std::cout << "Thread " << index << " completed first row. " << std::endl;
        lkPrev.unlock();
        thread_signals[prevIndex].notify_one();
        std::cout << index << " Signaled " << ((index-1 == -1) ? threads - 1 : index-1) << std::endl;*/


        for (int i = startRow + 1; i < endRow - 1; i++) {
            for (int y = 0; y < Ny; y++) {
                if (grid[i][y] != NULL) {
                    grid[i][y]->update(grid, i, y);
                }
            }
        }

        threadAges[index]++;
        //std::cout << index << " locking for last row" << std::endl;
        /*std::cout << index << " " << threadAges[prevIndex] << " " << simulationAge << std::endl;
        //lk.lock();
        thread_signals[index].wait(lk, [prevIndex]{ return threadAges[prevIndex] == simulationAge + 1; });

        std::cout << index << " updating last row" << std::endl;*/

        rowMutexes[nextIndex].lock();
        //updatedMutexes[nextIndex].lock();
        for (int y = 0; y < Ny; y++) {
            if (grid[endRow - 1][y] != NULL) {
                grid[endRow - 1][y]->update(grid, endRow - 1, y);
            }
        }
        rowMutexes[nextIndex].unlock();

        //std::cout << index << " locking global update" << std::endl;
        globalMutex.lock();
        //std::cout << index << " locking global update" << std::endl;
        completedThreads++;

        //std::cout << index << " is the " << completedThreads << " completed thread" << std::endl;
        if(completedThreads == threads) {
            completedThreads = 0;

            /*if (!makeGif && isDryRun) {
                continue;
            }*/

            // SFML logic
            sf::Event event;
            while (window->pollEvent(event)) {
                if (event.type == sf::Event::Closed)
                    window->close();
            }

            window->clear(emptyCell);
            for (int i = 0; i < Nx; i++) {
                for (int y = 0; y < Ny; y++) {
                    if (grid[i][y] != NULL) {
                        sf::RectangleShape cell(sf::Vector2f(cellSize, cellSize));
                        cell.setFillColor(grid[i][y]->color);
                        cell.setPosition(i * cellSize, y * cellSize);

                        window->draw(cell);
                    }
                }
            }

            window->display();

            // Make Gif
            if (!makeGif) {
                continue;
            }

            sf::Texture texture;
            texture.create(windowSizes.x, windowSizes.y);
            texture.update(*window);
            sf::Image image = texture.copyToImage();
            image.saveToFile(outputDir + "/" + std::to_string(age) + ".png");
            //std::this_thread::sleep_for(std::chrono::milliseconds(animDelay));*/

            //globalCv.notify_all();
            //sleep(1);
            simulationAge++;
            //std::cout << simulationAge << std::endl;
        }
        globalMutex.unlock();

        while (simulationAge == age) {

        }

       //std::cout << index << " starting second iteration" << std::endl;
    }
    return;
}

bool handleOptions(int argc, char *argv[]) {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "Show help description")
        ("chronons,c", po::value<int>()->default_value(300), "Set simulation chronons (iterations).")
        ("width,w", po::value<int>()->default_value(2000), "Set simulation width in pixels.")
        ("height,h", po::value<int>()->default_value(1000), "Set simulation height in pixels.")
        ("cell-size,z", po::value<int>()->default_value(5), "Set square cell size in pixels. Must devide both width and height of simulation.")
        ("threads,t", po::value<int>()->default_value(1), "Set number of threads")
        ("fishes,f", po::value<int>()->default_value(5000), "Set number of fishes")
        ("sharks,s", po::value<int>()->default_value(1000), "Set number of sharks")
        ("fish-replicate,F", po::value<int>()->default_value(10), "Set fish replicate time in chronons")
        ("shark-replicate,S", po::value<int>()->default_value(8), "Set shark replicate time in chronons")
        ("shark-starve,R", po::value<int>()->default_value(7), "Set shark starve time in chronons")
        ("dry-run", "Run the simulation without visualization and unecessary output.")
        ("make-gif", "Produce a gif animation from the simulation.")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return false;
    }

    threads = vm["threads"].as<int>();
    gridX = vm["width"].as<int>();
    gridY = vm["height"].as<int>(); 
    cellSize = vm["cell-size"].as<int>();
    Nx = gridX / cellSize;
    Ny = gridY / cellSize;
    simulationDuration = vm["chronons"].as<int>();
    sharksN = vm["sharks"].as<int>();
    fishesN = vm["fishes"].as<int>();
    fishReplicate = vm["fish-replicate"].as<int>();
    sharkReplicate = vm["shark-replicate"].as<int>();
    sharkStarve = vm["shark-starve"].as<int>();
    isDryRun = vm.count("dry-run");
    makeGif = vm.count("make-gif");

    if (gridX % cellSize != 0 || gridY % cellSize != 0) {
        std::cout << "Cell size cannot fit in grid dimensions" << std::endl;
        return false;
    }

    if (sharksN + fishesN > Nx * Ny) {
        std::cout << "Too much fishes and sharks. Cannot fit grid" << std::endl;
        return false;
    }

    std::cout << "Running simulation with parameters:" << std::endl
        << "Threads: " << threads << std::endl
        << "Width: " << Nx << std::endl
        << "Height: " << Ny << std::endl
        << "Cell Size: " << cellSize << std::endl
        << "Sharks: " << sharksN << std::endl
        << "Fishes: " << fishesN << std::endl
        << "Fish Replicate Time: " << fishReplicate << std::endl
        << "Shark Replicate Time: " << sharkReplicate << std::endl
        << "Shark Starve Time: " << sharkStarve << std::endl
        << "Chronons: " << simulationDuration << std::endl;

    return true;
}

int main(int argc, char *argv[])
{
    globalMutex.unlock();

    if(! handleOptions(argc, argv)) {
        return 1;
    }
    initializeSimulation();

    sf::RenderWindow window(sf::VideoMode(gridX, gridY), "WaTor");
    window.setVisible(!isDryRun);
    sf::Vector2u windowSizes = window.getSize();

    if (makeGif && ! fs::exists(outputDir)) {
        if(! fs::create_directory(outputDir)) {
            std::cout << "Could not create gif" << std::endl;
            return -1;
        }
    }

    std::vector<std::thread> workers;

    rowMutexes = std::vector<std::mutex>(threads);
    updatedMutexes = std::vector<std::mutex>(threads);
    thread_signals = std::vector<std::condition_variable>(threads);
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < threads; i++) {
        threadAges.push_back(0);
        workers.push_back(std::thread(computeRegion, i, &window));
    }
    for (int i = 0; i < threads; i++) {
        workers[i].join();
    }

    
    auto end = std::chrono::steady_clock::now();

    std::cout << "Simulation elapsed time in microseconds: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl
              << "Threads: " << threads << ", Chronons: " << simulationDuration << std::endl
              << "Dry-run: " << isDryRun << ", Make gif:" << makeGif << std::endl;

    if (window.isOpen()) {
        window.close();
    }

    return 0;
}