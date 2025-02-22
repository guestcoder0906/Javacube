#include "cubiomes/finders.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h> //for PRId64

// Biome ID to name mapping (from original code)
const char* getBiomeName(int id) {
    switch(id) {
        case 0: return "Ocean";
        case 1: return "Plains";
        case 2: return "Desert";
        case 3: return "Windswept Hills";
        case 4: return "Forest";
        case 5: return "Taiga";
        case 6: return "Swamp";
        case 7: return "River";
        case 10: return "Frozen Ocean";
        case 11: return "Frozen River";
        case 12: return "Snowy Plains";
        case 13: return "Snowy Mountains";
        case 14: return "Mushroom Fields";
        case 15: return "Mushroom Fields Shore";
        case 16: return "Beach";
        case 17: return "Desert Hills";
        case 18: return "Windswept Forest";
        case 19: return "Taiga Hills";
        case 20: return "Mountain Edge";
        case 21: return "Jungle";
        case 22: return "Jungle Hills";
        case 23: return "Sparse Jungle";
        case 24: return "Deep Ocean";
        case 25: return "Stony Shore";
        case 26: return "Snowy Beach";
        case 27: return "Birch Forest";
        case 28: return "Birch Forest Hills";
        case 29: return "Dark Forest";
        case 30: return "Snowy Taiga";
        case 31: return "Snowy Taiga Hills";
        case 32: return "Old Growth Pine Taiga";
        case 33: return "Giant Tree Taiga Hills";
        case 34: return "Wooded Mountains";
        case 35: return "Savanna";
        case 36: return "Savanna Plateau";
        case 37: return "Badlands";
        case 38: return "Wooded Badlands";
        case 39: return "Badlands Plateau";
        case 44: return "Warm Ocean";
        case 45: return "Lukewarm Ocean";
        case 46: return "Cold Ocean";
        case 47: return "Deep Warm Ocean";
        case 48: return "Deep Lukewarm Ocean";
        case 49: return "Deep Cold Ocean";
        case 50: return "Deep Frozen Ocean";
        case 129: return "Sunflower Plains";
        case 130: return "Desert Lakes";
        case 131: return "Windswept Gravelly Hills";
        case 132: return "Flower Forest";
        case 133: return "Taiga Mountains";
        case 134: return "Swamp Hills";
        case 140: return "Ice Spikes";
        case 149: return "Modified Jungle";
        case 151: return "Modified Jungle Edge";
        case 155: return "Old Growth Birch Forest";
        case 156: return "Tall Birch Hills";
        case 157: return "Dark Forest Hills";
        case 158: return "Snowy Taiga Mountains";
        case 160: return "Old Growth Spruce Taiga";
        case 161: return "Giant Spruce Taiga Hills";
        case 162: return "Gravelly Mountains+";
        case 163: return "Windswept Savanna";
        case 164: return "Shattered Savanna Plateau";
        case 165: return "Eroded Badlands";
        case 166: return "Modified Wooded Badlands Plateau";
        case 167: return "Modified Badlands Plateau";
        case 168: return "Bamboo Jungle";
        case 169: return "Bamboo Jungle Hills";
        case 170: return "Underground";
        case 171: return "Underground";
        case 172: return "Underground";
        case 174: return "Dripstone Caves";
        case 175: return "Lush Caves";
        case 177: return "Meadow";
        case 178: return "Grove";
        case 179: return "Snowy Slopes";
        case 180: return "Frozen Peaks";
        case 181: return "Jagged Peaks";
        case 182: return "Stony Peaks";
        case 183: return "Deep Dark";
        case 184: return "Mangrove Swamp";
        case 185: return "Cherry Grove";
        case 186: return "Pale Garden";
        default: return "Unknown Biome";
    }
}

typedef struct {
    int structType;
    int minCount;
    int minHeight;
    int maxHeight;
    int biomeId;
    int minBiomeSize;
    int maxBiomeSize;
    int hasHeightMin;
    int hasHeightMax;
    int hasBiomeSize;
} StructureRequirement;

typedef struct {
    uint64_t startSeed;
    uint64_t endSeed;
    int radius;
    StructureRequirement *reqs;
    int reqCount;
} ScanTask;

typedef struct {
    int structType;
    Pos pos;
    int height;
    int biome;
    int biomeSize;
} FoundStructure;

typedef struct {
    uint64_t seed;
    FoundStructure *structures;
    int structCount;
} SeedResult;

void* scanSeeds(void *arg) {
    ScanTask *task = (ScanTask*)arg;
    Generator g;
    setupGenerator(&g, MC_1_21, 0);

    for (uint64_t seed = task->startSeed; seed < task->endSeed; seed++) {
        applySeed(&g, DIM_OVERWORLD, seed);

        int reqsMet = 1;
        FoundStructure foundStructs[1024];
        int foundCount = 0;

        for (int r = 0; r < task->reqCount; r++) {
            StructureRequirement req = task->reqs[r];
            int count = 0;

            for (int regX = -task->radius; regX <= task->radius && reqsMet; regX++) {
                for (int regZ = -task->radius; regZ <= task->radius && reqsMet; regZ++) {
                    Pos p;
                    if (!getStructurePos(req.structType, MC_1_21, seed, regX, regZ, &p))
                        continue;

                    if (!isViableStructurePos(req.structType, &g, p.x, p.z, 0))
                        continue;

                    int id = getBiomeAt(&g, 4, p.x>>2, p.y>>2, p.z>>2);

                    // Check biome requirement if specified
                    if (req.biomeId >= 0 && id != req.biomeId)
                        continue;

                    // TODO: Implement height check once available
                    int height = 64; // placeholder

                    // Store found structure if all requirements met
                    if (foundCount < 1024) {
                        foundStructs[foundCount].structType = req.structType;
                        foundStructs[foundCount].pos = p;
                        foundStructs[foundCount].height = height;
                        foundStructs[foundCount].biome = id;
                        foundCount++;
                    }
                    count++;
                }
            }

            if (count < req.minCount) {
                reqsMet = 0;
                break;
            }
        }

        if (reqsMet) {
            printf("\nFound matching seed: %" PRId64 "\n", seed);
            for (int i = 0; i < foundCount; i++) {
                const char *struct_names[] = {
                    "Feature", "Desert_Pyramid", "Jungle_Temple", "Swamp_Hut",
                    "Igloo", "Village", "Ocean_Ruin", "Shipwreck", "Monument",
                    "Mansion", "Outpost", "Ruined_Portal", "Ruined_Portal_N",
                    "Ancient_City", "Treasure", "Mineshaft", "Desert_Well",
                    "Geode", "Trail_Ruins", "Trial_Chambers"
                };
                printf("Found %s\n", struct_names[foundStructs[i].structType]);
                printf("  Position: x:%d y:%d z:%d\n", 
                    foundStructs[i].pos.x, foundStructs[i].height, foundStructs[i].pos.z);
                printf("  Biome: %s (ID: %d)\n", getBiomeName(foundStructs[i].biome), 
                    foundStructs[i].biome);
                printf("  Distance from origin: %.1f blocks\n\n",
                    sqrt(foundStructs[i].pos.x * foundStructs[i].pos.x + 
                         foundStructs[i].pos.z * foundStructs[i].pos.z));
            }
        }
    }
    return NULL;
}

int main() {
    // Example requirements setup
    StructureRequirement reqs[] = {
        {17, 3, -60, 100, -1, 0, 0, 1, 1, 0}, // At least 3 geodes
        {19, 1, 0, 0, -1, 0, 0, 0, 0, 0} // At least 1 trial chamber
    };

    // Scan configuration
    int taskCount = 4; // Number of parallel tasks
    uint64_t startSeed = 12345;
    uint64_t seedsPerTask = 1000;
    int radius = 32; // Search radius in regions

    // Create and run tasks
    pthread_t threads[taskCount];
    ScanTask tasks[taskCount];

    for (int i = 0; i < taskCount; i++) {
        tasks[i].startSeed = startSeed + (i * seedsPerTask);
        tasks[i].endSeed = tasks[i].startSeed + seedsPerTask;
        tasks[i].radius = radius;
        tasks[i].reqs = reqs;
        tasks[i].reqCount = sizeof(reqs) / sizeof(reqs[0]);

        pthread_create(&threads[i], NULL, scanSeeds, &tasks[i]);
    }

    // Wait for all tasks to complete
    for (int i = 0; i < taskCount; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}