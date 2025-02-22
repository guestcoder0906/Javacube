#include "cubiomes/finders.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>


typedef struct {
    int structType;
    int minCount;
    int minHeight;
    int maxHeight;
    int biome;
    int minBiomeSize;
    int maxBiomeSize;
} StructureRequirement;

typedef struct {
    uint64_t startSeed;
    uint64_t endSeed;
    StructureRequirement *reqs;
    int reqCount;
    Generator *g;
} ScanTask;

void checkStructureAt(Generator *g, uint64_t seed, StructureRequirement *req, Pos p, int *found) {
    applySeed(g, DIM_OVERWORLD, seed);

    if (!isViableStructurePos(req->structType, g, p.x, p.z, 0))
        return;

    int y = -1; 
    int biomeId = getBiomeAt(g, 4, p.x >> 2, 0, p.z >> 2);
    int biomeSize = 0; 

    
    if ((req->minHeight != -1 && y < req->minHeight) || 
        (req->maxHeight != -1 && y > req->maxHeight))
        return;

    
    if (req->biome != -1) {
        if (biomeId != req->biome)
            return;
        if ((req->minBiomeSize != -1 && biomeSize < req->minBiomeSize) ||
            (req->maxBiomeSize != -1 && biomeSize > req->maxBiomeSize))
            return;
    }

    (*found)++;
}

void* scanSeeds(void *arg) {
    ScanTask *task = (ScanTask*)arg;
    uint64_t seed;

    for (seed = task->startSeed; seed < task->endSeed; seed++) {
        int allReqsMet = 1;

        for (int i = 0; i < task->reqCount; i++) {
            StructureConfig sconf;
            int found = 0;

            if (!getStructureConfig(task->reqs[i].structType, MC_1_19, &sconf))
                continue;

            
            for (int rx = -5; rx <= 5; rx++) {
                for (int rz = -5; rz <= 5; rz++) {
                    Pos p;
                    if (!getStructurePos(task->reqs[i].structType, MC_1_19, seed, rx, rz, &p))
                        continue;

                    checkStructureAt(task->g, seed, &task->reqs[i], p, &found);
                    if (found >= task->reqs[i].minCount)
                        break;
                }
                if (found >= task->reqs[i].minCount)
                    break;
            }

            if (found < task->reqs[i].minCount) {
                allReqsMet = 0;
                break;
            }
        }

        if (allReqsMet) {
            printf("\nFound matching seed: %" PRIu64 "\n", seed);
            
            for (int i = 0; i < task->reqCount; i++) {
                printf("Structure type %d locations:\n", task->reqs[i].structType);
                for (int rx = -5; rx <= 5; rx++) {
                    for (int rz = -5; rz <= 5; rz++) {
                        Pos p;
                        if (!getStructurePos(task->reqs[i].structType, MC_1_19, seed, rx, rz, &p))
                            continue;
                        if (isViableStructurePos(task->reqs[i].structType, task->g, p.x, p.z, 0)) {
                            int biomeId = getBiomeAt(task->g, 4, p.x >> 2, 0, p.z >> 2);
                            printf("  At (%d, %d) Biome: %s\n", p.x, p.z, getBiomeName(biomeId));
                        }
                    }
                }
            }
            printf("\n");
        }
    }
    return NULL;
}

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

int main() {
    
    StructureRequirement reqs[] = {
        {
            .structType = Village,
            .minCount = 1,
            .minHeight = -1, 
            .maxHeight = -1,
            .biome = plains, 
            .minBiomeSize = -1,
            .maxBiomeSize = -1
        },
        {
            .structType = Desert_Pyramid,
            .minCount = 1,
            .minHeight = -1,
            .maxHeight = -1,
            .biome = desert,
            .minBiomeSize = -1,
            .maxBiomeSize = -1
        }
    };

    int numTasks = 1; 
    uint64_t startSeed = 0; 
    int reqCount = sizeof(reqs) / sizeof(StructureRequirement);

    pthread_t *threads = malloc(numTasks * sizeof(pthread_t));
    Generator *generators = malloc(numTasks * sizeof(Generator));
    ScanTask *tasks = malloc(numTasks * sizeof(ScanTask));

    uint64_t seedsPerTask = 1000000 / numTasks;

    for (int i = 0; i < numTasks; i++) {
        setupGenerator(&generators[i], MC_1_19, 0);

        tasks[i].startSeed = startSeed + (i * seedsPerTask);
        tasks[i].endSeed = tasks[i].startSeed + seedsPerTask;
        tasks[i].reqs = reqs;
        tasks[i].reqCount = reqCount;
        tasks[i].g = &generators[i];

        pthread_create(&threads[i], NULL, scanSeeds, &tasks[i]);
    }

    for (int i = 0; i < numTasks; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(generators);
    free(tasks);

    return 0;
}