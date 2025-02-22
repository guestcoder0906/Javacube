#include "cubiomes/finders.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h> //for PRIu64

// Biome ID to name mapping
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
    int structureType;
    int minCount;
    int minHeight;
    int maxHeight;
    int biomeId;
    int minBiomeSize;
    int maxBiomeSize;
} StructureRequirement;

typedef struct {
    uint64_t startSeed;
    uint64_t endSeed;
    int mc;
    StructureRequirement *requirements;
    int reqCount;
    pthread_mutex_t *printMutex;
} ScanTaskArg;

void printStructureDetails(Generator *g, Pos pos, int structType, int biomeId, int mc) {
    const char *struct_names[] = {
        "Feature", "Desert_Pyramid", "Jungle_Temple", "Swamp_Hut",
        "Igloo", "Village", "Ocean_Ruin", "Shipwreck", "Monument",
        "Mansion", "Outpost", "Ruined_Portal", "Ruined_Portal_N",
        "Ancient_City", "Treasure", "Mineshaft", "Desert_Well",
        "Geode", "Trail_Ruins", "Trial_Chambers"
    };

    int id = getBiomeAt(g, 4, pos.x>>2, 320>>2, pos.z>>2);
    if (id == -1) { 
        float height[256];
        int w = 16, h = 16;
        SurfaceNoise sn;
        initSurfaceNoise(&sn, DIM_OVERWORLD, g->seed);
        Range r = {4, pos.x>>2, pos.z>>2, w, h, 320>>2, 1};
        mapApproxHeight(height, NULL, g, &sn, r.x, r.z, w, h);

        int lx = (pos.x & 15);
        int lz = (pos.z & 15);
        int surface_y = (int)height[lz * w + lx];
        id = getBiomeAt(g, 4, pos.x>>2, surface_y>>2, pos.z>>2);
    }

    printf("Found %s\n", struct_names[structType]);
    printf("  Position: x:%d z:%d\n", pos.x, pos.z);
    printf("  Biome: %s (ID: %d)\n\n", getBiomeName(id), id);
}

int scanChunkRange(Generator *g, int structType, Pos *pos, 
    int x0, int z0, int x1, int z1, StructureRequirement *req) {

    StructureConfig sconf;
    if (!getStructureConfig(structType, g->mc, &sconf))
        return 0;

    int found = 0;
    double blocksPerRegion = sconf.regionSize * 16.0;
    int rx0 = (int)floor(x0 / blocksPerRegion);
    int rz0 = (int)floor(z0 / blocksPerRegion);
    int rx1 = (int)ceil(x1 / blocksPerRegion);
    int rz1 = (int)ceil(z1 / blocksPerRegion);

    int i, j;
    for (j = rz0; j <= rz1; j++) {
        for (i = rx0; i <= rx1; i++) {
            if (!getStructurePos(structType, g->mc, g->seed, i, j, pos))
                continue;

            if (pos->x < x0 || pos->x > x1 || pos->z < z0 || pos->z > z1)
                continue;

            if (!isViableStructurePos(structType, g, pos->x, pos->z, 0))
                continue;

            if (req->biomeId >= 0) {
                int id = getBiomeAt(g, 4, pos->x>>2, 320>>2, pos->z>>2);
                if (id != req->biomeId)
                    continue;
            }

            found++;
        }
    }
    return found;
}

void* scanTask(void *arg) {
    ScanTaskArg *task = (ScanTaskArg*)arg;
    Generator g;
    setupGenerator(&g, task->mc, 0);

    Pos pos;
    int x0 = -2000, z0 = -2000;
    int x1 = 2000, z1 = 2000;

    for (uint64_t seed = task->startSeed; seed < task->endSeed; seed++) {
        applySeed(&g, DIM_OVERWORLD, seed);
        int allReqsMet = 1;

        for (int i = 0; i < task->reqCount; i++) {
            StructureRequirement *req = &task->requirements[i];
            int count = scanChunkRange(&g, req->structureType, &pos, x0, z0, x1, z1, req);

            if (count < req->minCount) {
                allReqsMet = 0;
                break;
            }
        }

        if (allReqsMet) {
            pthread_mutex_lock(task->printMutex);
            printf("Found matching seed: %" PRIu64 "\n", seed);
            for (int i = 0; i < task->reqCount; i++) {
                scanChunkRange(&g, task->requirements[i].structureType, &pos, x0, z0, x1, z1, &task->requirements[i]);
                printStructureDetails(&g, pos, task->requirements[i].structureType, task->requirements[i].biomeId, task->mc);
            }
            printf("\n");
            pthread_mutex_unlock(task->printMutex);
        }
    }
    return NULL;
}

int main() {
    // Example configuration
    int mc = MC_1_21;
    uint64_t startSeed = 1;
    int numTasks = 1;

    StructureRequirement requirements[] = {
        {Desert_Pyramid, 1, -1, -1, desert, -1, -1},
        {Village, 2, -1, -1, plains, -1, -1}
    };
    int reqCount = sizeof(requirements) / sizeof(StructureRequirement);

    pthread_t *threads = malloc(numTasks * sizeof(pthread_t));
    ScanTaskArg *args = malloc(numTasks * sizeof(ScanTaskArg));
    pthread_mutex_t printMutex = PTHREAD_MUTEX_INITIALIZER;

    uint64_t seedsPerTask = (UINT64_MAX - startSeed) / numTasks;

    for (int i = 0; i < numTasks; i++) {
        args[i].startSeed = startSeed + (i * seedsPerTask);
        args[i].endSeed = (i == numTasks-1) ? UINT64_MAX : args[i].startSeed + seedsPerTask;
        args[i].mc = mc;
        args[i].requirements = requirements;
        args[i].reqCount = reqCount;
        args[i].printMutex = &printMutex;

        pthread_create(&threads[i], NULL, scanTask, &args[i]);
    }

    for (int i = 0; i < numTasks; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(args);
    pthread_mutex_destroy(&printMutex);

    return 0;
}