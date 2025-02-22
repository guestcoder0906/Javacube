#include "cubiomes/finders.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <limits.h>

// Global variables for seed finding
int maxSeeds = 1;
int seedsFound = 0;

// -----------------------------------------------------------------------------
// Union–find functions for clustering
int findSet(int parent[], int i) {
    if (parent[i] != i)
        parent[i] = findSet(parent, parent[i]);
    return parent[i];
}

void unionSets(int parent[], int x, int y) {
    int rootX = findSet(parent, x);
    int rootY = findSet(parent, y);
    if (rootX != rootY)
        parent[rootY] = rootX;
}

// -----------------------------------------------------------------------------
// Biome ID to name mapping (unchanged from the original)
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
        default:  return "Unknown Biome";
    }
}

// -----------------------------------------------------------------------------
// Stub for biome patch size calculation (if needed)
int getBiomePatchSize(Generator *g, int x, int z, int biome_id) {
    // Create a 256x256 area to scan around the point
    int radius = 128;
    int sx = (x - radius) >> 2;
    int sz = (z - radius) >> 2;
    int w = radius >> 1;
    int h = radius >> 1;
    
    // Find min/max extents of this biome patch
    int minX = INT_MAX, maxX = INT_MIN;
    int minZ = INT_MAX, maxZ = INT_MIN;
    int found = 0;
    
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            int bx = sx + i;
            int bz = sz + j;
            int id = getBiomeAt(g, 4, bx, 0, bz);
            
            if (id == biome_id) {
                found = 1;
                if (bx < minX) minX = bx;
                if (bx > maxX) maxX = bx;
                if (bz < minZ) minZ = bz;
                if (bz > maxZ) maxZ = bz;
            } else if (found && (bx > maxX + 2 || bz > maxZ + 2)) {
                // We've moved beyond the biome patch
                break;
            }
        }
    }
    
    if (!found) return 0;
    
    // Calculate average size in blocks
    int sizeX = (maxX - minX + 1) * 4; // Convert from scale 4 to blocks
    int sizeZ = (maxZ - minZ + 1) * 4;
    return (sizeX + sizeZ) / 2;
}

// -----------------------------------------------------------------------------
// Structure for an individual required structure condition.
typedef struct {
    int structureType;
    int minCount;
    int minHeight;
    int maxHeight;
    int requiredBiome;
    int minBiomeSize;
    int maxBiomeSize;
} StructureRequirement;

#define NUM_REQUIREMENTS 0
StructureRequirement requirements[NUM_REQUIREMENTS] = {
    Example: {5, 1, -10000, 10000, 1, 50, 50}
};

// -----------------------------------------------------------------------------
// Structure for cluster requirement. Note: structureTypes is now a pointer.
typedef struct {
    bool enabled;
    int clusterDistance;
    int *structureTypes;  // Dynamically provided list.
    int count;           // Number of structure types in the list.
} ClusterRequirement;

// Define cluster types dynamically.
int clusterTypesArray[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 16, 18};
ClusterRequirement clusterReq = {
    .enabled = false,
    .clusterDistance = 16,  // 16 blocks or less.
    .structureTypes = clusterTypesArray,
    .count = sizeof(clusterTypesArray) / sizeof(clusterTypesArray[0])
};

// -----------------------------------------------------------------------------
// Structure to hold found structure positions for cluster checking.
typedef struct {
    int structureType;
    int x;
    int z;
} StructurePos;

// -----------------------------------------------------------------------------
// Structure to define an invalid combination (as a multiset of structure types).
typedef struct {
    int *types;  // Dynamically provided list (should be sorted if needed).
    int count;
} InvalidCombination;

// Define invalid combinations dynamically.
#define NUM_INVALID_COMBINATIONS 1
int invComb1Arr[] = {6, 7};  // Example: Ocean Ruin (6) + Shipwreck (7)
InvalidCombination invalidCombinations[NUM_INVALID_COMBINATIONS] = {
    { invComb1Arr, sizeof(invComb1Arr) / sizeof(invComb1Arr[0]) }
};

// -----------------------------------------------------------------------------
// Comparison function for qsort.
int compareInts(const void *a, const void *b) {
    int intA = *(const int *)a;
    int intB = *(const int *)b;
    return intA - intB;
}

// Helper function to compare two sorted arrays.
bool arraysEqual(int a[], int aCount, int b[], int bCount) {
    if (aCount != bCount)
        return false;
    for (int i = 0; i < aCount; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

// Function to check if a cluster (of any size) is invalid via a subset check.
bool isInvalidClusterDynamic(int *groupTypes, int groupSize) {
    for (int ic = 0; ic < NUM_INVALID_COMBINATIONS; ic++) {
        int *invalidSet = invalidCombinations[ic].types;
        int invalidCount = invalidCombinations[ic].count;
        bool isSubset = true;
        for (int k = 0; k < invalidCount; k++) {
            bool found = false;
            for (int j = 0; j < groupSize; j++) {
                if (groupTypes[j] == invalidSet[k]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                isSubset = false;
                break;
            }
        }
        if (isSubset)
            return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Global configuration parameters.
uint64_t starting_seed = 12345;
int searchRadius = 500;
int useSpawn = 1;      // 1 = use spawn point; 0 = use custom coordinates.
int customX = 0;
int customZ = 0;
int tasksCount = 1;    // Number of parallel scanning tasks.

// Global variables for multithreading.
volatile bool foundValidSeed = false;
pthread_mutex_t seedMutex = PTHREAD_MUTEX_INITIALIZER;
uint64_t validSeed = 0;
volatile uint64_t currentSeed;

// -----------------------------------------------------------------------------
// scanSeed: Scans a single seed for both individual requirements and cluster requirements.
bool scanSeed(uint64_t seed) {
    bool individualValid = true;
    bool clusterValid = true; // If clustering is disabled, treat as passing.

    // Set up Overworld generator and get spawn.
    Generator g;
    setupGenerator(&g, MC_1_21, 0);
    applySeed(&g, DIM_OVERWORLD, seed);
    Pos spawn = {0, 0};
    int x0, z0;
    if (useSpawn) {
        spawn = getSpawn(&g);
        x0 = spawn.x - searchRadius;
        z0 = spawn.z - searchRadius;
    } else {
        x0 = customX - searchRadius;
        z0 = customZ - searchRadius;
    }
    int x1 = x0 + (searchRadius * 2);
    int z1 = z0 + (searchRadius * 2);

    // Set up Nether and End generators.
    Generator ng, eg;
    setupGenerator(&ng, MC_1_21, 0);
    applySeed(&ng, DIM_NETHER, seed);
    setupGenerator(&eg, MC_1_21, 0);
    applySeed(&eg, DIM_END, seed);

    SurfaceNoise sn, esn;
    initSurfaceNoise(&sn, DIM_OVERWORLD, seed);
    initSurfaceNoise(&esn, DIM_END, seed);

    // Check individual structure requirements (if any are defined).
    for (int rIndex = 0; rIndex < NUM_REQUIREMENTS; rIndex++) {
        StructureRequirement req = requirements[rIndex];
        int foundCount = 0;
        StructureConfig sconf;
        if (!getStructureConfig(req.structureType, MC_1_21, &sconf))
            continue;
        Generator *curr_gen = &g;
        SurfaceNoise *curr_sn = &sn;
        if (sconf.dim == DIM_NETHER) {
            curr_gen = &ng;
        } else if (sconf.dim == DIM_END) {
            curr_gen = &eg;
            curr_sn = &esn;
        }
        double blocksPerRegion = sconf.regionSize * 16.0;
        int rx0 = (int)floor(x0 / blocksPerRegion);
        int rz0 = (int)floor(z0 / blocksPerRegion);
        int rx1 = (int)ceil(x1 / blocksPerRegion);
        int rz1 = (int)ceil(z1 / blocksPerRegion);

        for (int j = rz0; j <= rz1; j++) {
            for (int i = rx0; i <= rx1; i++) {
                Pos pos;
                if (!getStructurePos(req.structureType, MC_1_21, seed, i, j, &pos))
                    continue;
                if (pos.x < x0 || pos.x > x1 || pos.z < z0 || pos.z > z1)
                    continue;
                if (!isViableStructurePos(req.structureType, curr_gen, pos.x, pos.z, 0))
                    continue;

                int id = getBiomeAt(curr_gen, 4, pos.x >> 2, pos.z >> 2, 320 >> 2);
                if (id == -1) {
                    float heightArr[256];
                    int w = 16, h = 16;
                    Range r_range = {4, pos.x >> 2, pos.z >> 2, w, h, 320 >> 2, 1};
                    mapApproxHeight(heightArr, NULL, curr_gen, curr_sn, r_range.x, r_range.z, w, h);
                    int lx = pos.x & 15;
                    int lz = pos.z & 15;
                    int surface_y = (int)heightArr[lz * w + lx];
                    id = getBiomeAt(curr_gen, 4, pos.x >> 2, surface_y >> 2, pos.z >> 2);
                }

                StructureVariant sv;
                getVariant(&sv, req.structureType, MC_1_21, seed, pos.x, pos.z, id);

                float heightArr[256];
                int w = 16, h = 16;
                Range r_range = {4, pos.x >> 2, pos.z >> 2, w, h, 320 >> 2, 1};
                mapApproxHeight(heightArr, NULL, curr_gen, curr_sn, r_range.x, r_range.z, w, h);
                int lx = pos.x & 15;
                int lz = pos.z & 15;
                int surface_y = (int)heightArr[lz * w + lx];
                int y_pos = (sv.y != 320 && sv.y >= -64) ? sv.y : surface_y;

                if (y_pos < req.minHeight || y_pos > req.maxHeight)
                    continue;

                int biome_id = getBiomeAt(curr_gen, 1, pos.x + sv.x, y_pos, pos.z + sv.z);
                if (biome_id == 170 || biome_id == 171 || biome_id == 172) {
                    biome_id = getBiomeAt(curr_gen, 4, pos.x >> 2, surface_y >> 2, pos.z >> 2);
                }
                if (req.requiredBiome != -1 && biome_id != req.requiredBiome)
                    continue;

                if (req.requiredBiome != -1 && (req.minBiomeSize != -1 || req.maxBiomeSize != -1)) {
                    int patchSize = getBiomePatchSize(curr_gen, pos.x, pos.z, biome_id);
                    if ((req.minBiomeSize != -1 && patchSize < req.minBiomeSize) ||
                        (req.maxBiomeSize != -1 && patchSize > req.maxBiomeSize))
                        continue;
                }
                foundCount++;

                const char *struct_names[] = {
                    "Feature", "Desert_Pyramid", "Jungle_Temple", "Swamp_Hut",
                    "Igloo", "Village", "Ocean_Ruin", "Shipwreck", "Monument",
                    "Mansion", "Outpost", "Ruined_Portal", "Ruined_Portal_N",
                    "Ancient_City", "Treasure", "Mineshaft", "Desert_Well",
                    "Geode", "Trail_Ruins", "Trial_Chambers"
                };
                printf("Seed %llu: Found %s\n", seed, struct_names[req.structureType]);
                printf("  Position: x:%d y:%d z:%d\n", pos.x + sv.x, y_pos, pos.z + sv.z);
                printf("  Biome: %s (ID: %d)\n", getBiomeName(biome_id), biome_id);
                printf("  Distance from %s: %.1f blocks\n\n",
                       useSpawn ? "spawn" : "origin",
                       sqrt((pos.x - spawn.x) * (pos.x - spawn.x) +
                            (pos.z - spawn.z) * (pos.z - spawn.z)));
            }
        }
        if (foundCount < req.minCount)
            individualValid = false;
    }

    // Cluster requirement.
    if (clusterReq.enabled) {
        // Dynamically allocate an array for found cluster positions.
        int capacity = 64, clusterCount = 0;
        StructurePos *clusterPositions = malloc(capacity * sizeof(StructurePos));
        if (!clusterPositions) { perror("malloc"); exit(1); }

        for (int i = 0; i < clusterReq.count; i++) {
            int stype = clusterReq.structureTypes[i];
            StructureConfig sconf;
            if (!getStructureConfig(stype, MC_1_21, &sconf))
                continue;
            Generator *curr_gen = &g;
            SurfaceNoise *curr_sn = &sn;
            if (sconf.dim == DIM_NETHER) {
                curr_gen = &ng;
            } else if (sconf.dim == DIM_END) {
                curr_gen = &eg;
                curr_sn = &esn;
            }
            double blocksPerRegion = sconf.regionSize * 16.0;
            int rx0 = (int)floor(x0 / blocksPerRegion);
            int rz0 = (int)floor(z0 / blocksPerRegion);
            int rx1 = (int)ceil(x1 / blocksPerRegion);
            int rz1 = (int)ceil(z1 / blocksPerRegion);

            for (int j = rz0; j <= rz1; j++) {
                for (int k = rx0; k <= rx1; k++) {
                    Pos pos;
                    if (!getStructurePos(stype, MC_1_21, seed, k, j, &pos))
                        continue;
                    if (pos.x < x0 || pos.x > x1 || pos.z < z0 || pos.z > z1)
                        continue;
                    if (!isViableStructurePos(stype, curr_gen, pos.x, pos.z, 0))
                        continue;
                    if (clusterCount == capacity) {
                        capacity *= 2;
                        clusterPositions = realloc(clusterPositions, capacity * sizeof(StructurePos));
                        if (!clusterPositions) { perror("realloc"); exit(1); }
                    }
                    clusterPositions[clusterCount].structureType = stype;
                    clusterPositions[clusterCount].x = pos.x;
                    clusterPositions[clusterCount].z = pos.z;
                    clusterCount++;
                }
            }
        }

        bool atLeastOneValidCluster = false;
        if (clusterCount >= 2) {
            int *parent = malloc(clusterCount * sizeof(int));
            for (int i = 0; i < clusterCount; i++)
                parent[i] = i;
            for (int i = 0; i < clusterCount; i++) {
                for (int j = i + 1; j < clusterCount; j++) {
                    int dx = clusterPositions[i].x - clusterPositions[j].x;
                    int dz = clusterPositions[i].z - clusterPositions[j].z;
                    if (sqrt(dx * dx + dz * dz) <= clusterReq.clusterDistance)
                        unionSets(parent, i, j);
                }
            }
            bool *clusterProcessed = calloc(clusterCount, sizeof(bool));
            for (int i = 0; i < clusterCount; i++) {
                int root = findSet(parent, i);
                if (clusterProcessed[root])
                    continue;
                int groupSize = 0;
                int indicesCapacity = 8;
                int *indices = malloc(indicesCapacity * sizeof(int));
                for (int j = 0; j < clusterCount; j++) {
                    if (findSet(parent, j) == root) {
                        if (groupSize == indicesCapacity) {
                            indicesCapacity *= 2;
                            indices = realloc(indices, indicesCapacity * sizeof(int));
                        }
                        indices[groupSize++] = j;
                    }
                }
                clusterProcessed[root] = true;
                if (groupSize >= 2) {
                    int *groupTypes = malloc(groupSize * sizeof(int));
                    for (int j = 0; j < groupSize; j++) {
                        groupTypes[j] = clusterPositions[indices[j]].structureType;
                    }
                    qsort(groupTypes, groupSize, sizeof(int), compareInts);
                    // Dynamic subset check for invalid combinations.
                    bool clusterIsInvalid = isInvalidClusterDynamic(groupTypes, groupSize);
                    if (!clusterIsInvalid) {
                        atLeastOneValidCluster = true;
                        printf("Seed %llu: Valid cluster found (size %d):\n", seed, groupSize);
                        const char *struct_names[] = {
                            "Feature", "Desert_Pyramid", "Jungle_Temple", "Swamp_Hut",
                            "Igloo", "Village", "Ocean_Ruin", "Shipwreck", "Monument",
                            "Mansion", "Outpost", "Ruined_Portal", "Ruined_Portal_N",
                            "Ancient_City", "Treasure", "Mineshaft", "Desert_Well",
                            "Geode", "Trail_Ruins", "Trial_Chambers"
                        };
                        for (int j = 0; j < groupSize; j++) {
                            int idx = indices[j];
                            printf("  %s at (%d, %d)\n", struct_names[clusterPositions[idx].structureType],
                                   clusterPositions[idx].x, clusterPositions[idx].z);
                        }
                        printf("\n");
                    }
                    free(groupTypes);
                }
                free(indices);
            }
            free(clusterProcessed);
            free(parent);
        }
        free(clusterPositions);
        clusterValid = atLeastOneValidCluster;
    }

    if (individualValid && clusterValid) {
        printf("Valid seed found: %llu\n", seed);
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// scanTask: Thread function to check seeds.
void *scanTask(void *arg) {
    while (!foundValidSeed) {
        pthread_mutex_lock(&seedMutex);
        uint64_t seed = currentSeed;
        currentSeed++;
        pthread_mutex_unlock(&seedMutex);

        if (scanSeed(seed)) {
            pthread_mutex_lock(&seedMutex);
            seedsFound++;
            validSeed = seed;
            if (seedsFound >= maxSeeds) {
                foundValidSeed = true;
                break;
            }
            pthread_mutex_unlock(&seedMutex);
            break;
        }
    }
    return NULL;
}

// -----------------------------------------------------------------------------
// main: Starts scanning tasks.
#define MAX_SEEDS_TO_FIND 1 // Default max seeds to find

int main() {
    if (NUM_REQUIREMENTS == 0 && !clusterReq.enabled) {
        printf("Error: At least one requirement must be specified.\n");
        return 1;
    }
    int maxSeeds = MAX_SEEDS_TO_FIND;
    int seedsFound = 0;
    currentSeed = starting_seed;
    pthread_t threads[tasksCount];
    for (int i = 0; i < tasksCount; i++) {
        pthread_create(&threads[i], NULL, scanTask, NULL);
    }
    for (int i = 0; i < tasksCount; i++) {
        pthread_join(threads[i], NULL);
    }
    if (seedsFound > 0)
        printf("Found %d valid seed(s). Last seed: %llu\n", seedsFound, validSeed);
    else
        printf("No valid seeds found.\n");
    return 0;
}