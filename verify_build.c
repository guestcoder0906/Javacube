#include "cubiomes/finders.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <limits.h>
#include <string.h> // For memcpy

// -----------------------------------------------------------------------------
// Global variables for seed finding
// main: Starts scanning tasks.
#define MAX_SEEDS_TO_FIND 1
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
// Biome ID to name mapping (unchanged)
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
// (Not used for center calculations below)
int getBiomePatchSize(Generator *g, int x, int z, int biome_id) {
    int radius = 128;
    int sx = (x - radius) >> 2;
    int sz = (z - radius) >> 2;
    int w = radius >> 1;
    int h = radius >> 1;
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
                break;
            }
        }
    }
    if (!found) return 0;
    int sizeX = (maxX - minX + 1) * 4;
    int sizeZ = (maxZ - minZ + 1) * 4;
    return (sizeX + sizeZ) / 2;
}

// -----------------------------------------------------------------------------
// Structure for an individual required structure condition.
typedef struct {
    int structureType;   // e.g. Village (5)
    int minCount;        // Minimum number of such structures
    int minHeight;       // Minimum allowed height
    int maxHeight;       // Maximum allowed height
    int requiredBiome;   // Required biome ID (or -1 if not applicable)
    int minBiomeSize;    // Minimum biome patch size (or -1 if not applicable)
    int maxBiomeSize;    // Maximum biome patch size (or -1 if not applicable)
} StructureRequirement;

// -----------------------------------------------------------------------------
// Per-biome size configuration for required patches.
typedef struct {
    int biomeId;
    int minSize; // Minimum cell count for this biome patch (-1 if no minimum)
    int maxSize; // Maximum cell count for this biome patch (-1 if no maximum)
} BiomeSizeConfig;

// -----------------------------------------------------------------------------
// Dynamic Biome Requirements Structures
// For required biomes, each patch is determined by grouping cells that are touching and are of the same biome.
// Each required group carries an array of per-biome size configurations.
typedef struct {
    int *biomeIds;
    int biomeCount;
    BiomeSizeConfig *sizeConfigs;  // Array of size configurations (one per biome)
    int configCount;               // Number of configurations provided
    int logCenters;                // If nonzero, print the patch center and cell count when found
} BiomeRequirement;

// For clustered biomes, cells are grouped together regardless of individual type as long as they belong to the cluster group.
typedef struct {
    int *biomeIds;
    int biomeCount;
    int minSize;         // Minimum total cell count for the cluster (-1 if not set)
    int maxSize;         // Maximum total cell count for the cluster (-1 if not set)
    int logCenters;      // If nonzero, print the cluster center and cell count when found
} BiomeCluster;

// The BiomeSearch holds arrays for required and clustered groups.
typedef struct {
    BiomeRequirement *required;
    int requiredCount;
    BiomeCluster *clusters;
    int clusterCount;
} BiomeSearch;

// --- Dynamic initialization for required biomes ---
// In this example, we require a patch for Plains with a maximum size of 50 cells.
// (Uncomment and modify as needed for your requirements.)
static int reqGroup0[] = {0}; // Empty array with one element to avoid zero-size array
static BiomeSizeConfig reqSizeConfigs[] = {{0, 0, 0}}; // Empty config with one element
static BiomeRequirement reqGroup;

/*static int reqGroup0[] = {1}; // 1 is plains biome ID
    static BiomeSizeConfig reqSizeConfigs[] = {
        { 1, -1, -1 }   // Plains: min 20, max 100 cells per patch
    };
    static const BiomeRequirement reqGroup = {
        .biomeIds   = reqGroup0,
        .biomeCount = sizeof(reqGroup0) / sizeof(reqGroup0[0]),
        .sizeConfigs = reqSizeConfigs,
        .configCount = sizeof(reqSizeConfigs) / sizeof(reqSizeConfigs[0]),
        .logCenters = 1
    };*/

// --- Dynamic initialization for clustered biomes ---
// For clustered biomes, all cells that belong to the group are merged regardless of individual type.
// For example, define one cluster group that contains Plains and Cherry Grove with no size filtering.
static int clusterGroup0[] = {};//{129, 185};
static const BiomeCluster clustGroup0 = {
    .biomeIds   = NULL,//clusterGroup0,
    .biomeCount = 0,//sizeof(clusterGroup0) / sizeof(clusterGroup0[0]),
    .minSize    = -1,//2, // Minimum 2 cells for a cluster
    .maxSize    = -1,
    .logCenters = 1
};


// By default, we initialize with one required group and one cluster group.
static BiomeRequirement requiredBiomes[1]; // Initialize with plains requirement
static int requiredBiomesCount = 1; // Set count to 1

static const BiomeCluster biomeClusters[] = { clustGroup0 };
static const int biomeClustersCount = sizeof(biomeClusters) / sizeof(biomeClusters[0]);

static BiomeSearch biomeSearch = {
    .required = (BiomeRequirement *) requiredBiomes,
    .requiredCount = sizeof(requiredBiomes) / sizeof(requiredBiomes[0]),
    .clusters = (BiomeCluster *) biomeClusters,
    .clusterCount = sizeof(biomeClusters) / sizeof(biomeClusters[0])
};

// -----------------------------------------------------------------------------
// Structure for cluster requirement (for structures).
typedef struct {
    bool enabled;
    int clusterDistance;
    int *structureTypes;  // Dynamically provided list of structure types to cluster.
    int count;
    int minClusterSize;   // NEW: Minimum number of structures required in a cluster.
} ClusterRequirement;

int clusterTypesArray[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 16, 18 };
ClusterRequirement clusterReq = {
    .enabled = true,      // Change to true to enable structure clustering check.
    .clusterDistance = 32,
    .structureTypes = clusterTypesArray,
    .count = sizeof(clusterTypesArray) / sizeof(clusterTypesArray[0]),
    .minClusterSize  = 2 // Default is 2, can change to 3, 4, etc. as desired
};

// -----------------------------------------------------------------------------
// Structure to hold found positions (for biomes or structures).
typedef struct {
    int structureType; // For biomes, this is the biome ID.
    int x;
    int z;
} StructurePos;

// -----------------------------------------------------------------------------
// (Optional) Structure to define an invalid combination for structures.
typedef struct {
    int *types;
    int count;
} InvalidCombination;

#define NUM_INVALID_COMBINATIONS 2
int invComb1Arr[] = {16, 11}; 
int invComb2Arr[] = {16, 6};  
InvalidCombination invalidCombinations[NUM_INVALID_COMBINATIONS] = {
    { invComb1Arr, sizeof(invComb1Arr) / sizeof(invComb1Arr[0]) },
    { invComb2Arr, sizeof(invComb2Arr) / sizeof(invComb2Arr[0]) }
};

// -----------------------------------------------------------------------------
// Comparison function for qsort.
int compareInts(const void *a, const void *b) {
    int intA = *(const int *)a;
    int intB = *(const int *)b;
    return intA - intB;
}

// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
StructureRequirement structureRequirements[] = {
    //{ 5, 1, -10000, 10000, 1, -1, -1 },  // Village (5) in Plains (1), patch must have at least 50 cells
    // EXAMPLE: { 7, 1, -10000, 10000, 24, 20, -1 }   // Shipwreck (7) in Deep Ocean (24), patch must have at least 20 cells
};

int NUM_STRUCTURE_REQUIREMENTS = sizeof(structureRequirements) / sizeof(structureRequirements[0]);

// -----------------------------------------------------------------------------
// scanBiomes: Scans region (x0,z0) to (x1,z1) for required and clustered biomes.
// For required biomes, cells that are directly touching and of the same biome type are grouped into patches.
// Then, for each patch the cell count is compared to the per-biome configuration (if provided).
// For clustered biomes, cells are grouped together regardless of individual type.
bool scanBiomes(Generator *g, int x0, int z0, int x1, int z1, BiomeSearch *bs) {
    // If both required and clustered arrays are empty, then error out.
    if (bs->requiredCount == 0 && bs->clusterCount == 0) {
        fprintf(stderr, "Error: At least one requirement should be set.\n");
        return false;
    }

    int step = 4;  // Cells are "touching" if abs(dx) <= step and abs(dz) <= step
    bool requiredFound = true; // If no required groups are set, this remains true.

    // Process required biome groups if any.
    if(bs->requiredCount > 0) {
        requiredFound = false;
        for (int i = 0; i < bs->requiredCount; i++) {
            BiomeRequirement *req = &bs->required[i];
            int capacity = 128, count = 0;
            StructurePos *positions = malloc(capacity * sizeof(StructurePos));
            if (!positions) { perror("malloc"); exit(1); }
            for (int z = z0; z <= z1; z += step) {
                for (int x = x0; x <= x1; x += step) {
                    int biome = getBiomeAt(g, 4, x >> 2, 0, z >> 2);
                    for (int j = 0; j < req->biomeCount; j++) {
                        if (biome == req->biomeIds[j]) {
                            if (count == capacity) {
                                capacity *= 2;
                                positions = realloc(positions, capacity * sizeof(StructurePos));
                                if (!positions) { perror("realloc"); exit(1); }
                            }
                            positions[count].structureType = biome;
                            positions[count].x = x;
                            positions[count].z = z;
                            count++;
                            break;
                        }
                    }
                }
            }
            // Group cells that are directly touching and of the same biome.
            if (count > 0) {
                int *parent = malloc(count * sizeof(int));
                for (int k = 0; k < count; k++)
                    parent[k] = k;
                for (int k = 0; k < count; k++) {
                    for (int l = k + 1; l < count; l++) {
                        int dx = abs(positions[k].x - positions[l].x);
                        int dz = abs(positions[k].z - positions[l].z);
                        if (dx <= step && dz <= step &&
                            positions[k].structureType == positions[l].structureType)
                            unionSets(parent, k, l);
                    }
                }
                bool *processed = calloc(count, sizeof(bool));
                for (int k = 0; k < count; k++) {
                    int root = findSet(parent, k);
                    if (processed[root])
                        continue;
                    processed[root] = true;
                    double sumX = 0, sumZ = 0;
                    int compCount = 0;
                    for (int m = 0; m < count; m++) {
                        if (findSet(parent, m) == root) {
                            sumX += positions[m].x;
                            sumZ += positions[m].z;
                            compCount++;
                        }
                    }
                    double centerX = sumX / compCount;
                    double centerZ = sumZ / compCount;
                    // Look up per-biome configuration for this patch.
                    int patchBiome = positions[k].structureType;
                    bool sizeOk = true;
                    for (int c = 0; c < req->configCount; c++) {
                        if (req->sizeConfigs[c].biomeId == patchBiome) {
                            if (req->sizeConfigs[c].minSize > -1 && compCount < req->sizeConfigs[c].minSize)
                                sizeOk = false;
                            if (req->sizeConfigs[c].maxSize > -1 && compCount > req->sizeConfigs[c].maxSize)
                                sizeOk = false;
                            break;
                        }
                    }
                    if (sizeOk) {
                        printf("Required biome patch (group %d, biome %s): center at (%.1f, %.1f), cell count %d\n",
                               i, getBiomeName(patchBiome), centerX, centerZ, compCount);
                        requiredFound = true;
                        seedsFound++;
                    }
                }
                free(processed);
                free(parent);
            }
            free(positions);
        }
    }

    // -----------------------------------------------------------------------------
    // Process clustered biome groups.
    bool clustersFound = true;
    if (bs->clusterCount > 0)
    {
        for (int i = 0; i < bs->clusterCount; i++)
        {
            BiomeCluster *cluster = &bs->clusters[i];

            int capacity = 128, count = 0;
            StructurePos *positions = malloc(capacity * sizeof(StructurePos));
            if (!positions)
            {
                perror("malloc");
                exit(1);
            }

            // 1) Collect all cells belonging to any of the cluster's biome IDs.
            for (int z = z0; z <= z1; z += step)
            {
                for (int x = x0; x <= x1; x += step)
                {
                    int biome = getBiomeAt(g, 4, x >> 2, 0, z >> 2);
                    bool foundBiomeInCluster = false;
                    for (int j = 0; j < cluster->biomeCount; j++)
                    {
                        if (biome == cluster->biomeIds[j])
                        {
                            foundBiomeInCluster = true;
                            break;
                        }
                    }

                    if (foundBiomeInCluster)
                    {
                        // Expand array if needed.
                        if (count == capacity)
                        {
                            capacity *= 2;
                            positions = realloc(positions, capacity * sizeof(StructurePos));
                            if (!positions)
                            {
                                perror("realloc");
                                exit(1);
                            }
                        }
                        positions[count].structureType = biome; // store biome
                        positions[count].x = x;
                        positions[count].z = z;
                        count++;
                    }
                }
            }

            if (count == 0)
            {
                // We found no cells of this cluster's biome IDs at all.
                clustersFound = false;
            }
            else
            {
                // 2) Use union-find to group *connected* cells of any allowed biome type.
                int *parent = malloc(count * sizeof(int));
                for (int k = 0; k < count; k++)
                    parent[k] = k;

                // Union sets if within step distance (and already known to be valid cluster biomes).
                for (int k = 0; k < count; k++)
                {
                    for (int l = k + 1; l < count; l++)
                    {
                        int dx = abs(positions[k].x - positions[l].x);
                        int dz = abs(positions[k].z - positions[l].z);

                        if (dx <= step && dz <= step)
                        {
                            unionSets(parent, k, l);
                        }
                    }
                }

                // Track which roots we have processed so we don't print duplicates.
                bool *processed = calloc(count, sizeof(bool));

                // 3) For each connected component, gather stats and see if it has exactly two distinct biomes.
                for (int k = 0; k < count; k++)
                {
                    int root = findSet(parent, k);
                    if (processed[root])
                        continue;
                    processed[root] = true;

                    double sumX = 0, sumZ = 0;
                    int compCount = 0;

                    // We'll track which biome IDs are present in this cluster.
                    // Enough space for typical Java Edition biome IDs in [0..255] range (adjust if needed).
                    bool usedBiome[256];
                    memset(usedBiome, false, sizeof(usedBiome));

                    // 4) Aggregate all cells in this connected component.
                    for (int m = 0; m < count; m++)
                    {
                        if (findSet(parent, m) == root)
                        {
                            sumX += positions[m].x;
                            sumZ += positions[m].z;
                            compCount++;

                            int bId = positions[m].structureType;
                            if (bId >= 0 && bId < 256)
                            {
                                usedBiome[bId] = true;
                            }
                        }
                    }

                    // 5) Count how many distinct biomes are in this component.
                    int distinctBiomeCount = 0;
                    int distinctIDs[256];  // store which IDs we found
                    int dIdx = 0;

                    for (int bId = 0; bId < 256; bId++)
                    {
                        if (usedBiome[bId])
                        {
                            distinctIDs[dIdx++] = bId;
                            distinctBiomeCount++;
                        }
                    }

                    // We ONLY print if it has exactly 2 unique biome types.
                    if (distinctBiomeCount == 2)
                    {
                        double centerX = sumX / compCount;
                        double centerZ = sumZ / compCount;

                        // Check cluster size constraints (if needed).
                        bool sizeOk = true;
                        if (cluster->minSize > -1 && compCount < cluster->minSize)
                            sizeOk = false;
                        if (cluster->maxSize > -1 && compCount > cluster->maxSize)
                            sizeOk = false;

                        if (sizeOk)
                        {
                            // Build the "BiomeA + BiomeB" string
                            // (distinctIDs[0] and distinctIDs[1])
                            const char *bname1 = getBiomeName(distinctIDs[0]);
                            const char *bname2 = getBiomeName(distinctIDs[1]);

                            // Print result: e.g. "Forest + Plains"
                            printf("Clustered biome cluster %d: %s + %s, "
                                   "center at (%.1f, %.1f), total cell count %d\n",
                                   i + 1,
                                   bname1,
                                   bname2,
                                   centerX,
                                   centerZ,
                                   compCount);
                        }
                    }
                    // else skip printing for 1-biome or 3+-biome clusters
                }

                free(processed);
                free(parent);
            }

            free(positions);
        }
    }

    return requiredFound && clustersFound;
}

// -----------------------------------------------------------------------------
// Global configuration parameters.
uint64_t starting_seed = 5031130760383654775;
int searchRadius = 1000;
int useSpawn = 1;      // 1 = use spawn point; 0 = use custom coordinates.
int customX = 0;
int customZ = 0;
int tasksCount = 1;    // Number of parallel scanning tasks.

// Global variables for multithreading.
volatile bool foundValidSeed = false;
pthread_mutex_t seedMutex = PTHREAD_MUTEX_INITIALIZER;
uint64_t validSeed = 0;
volatile uint64_t currentSeed;

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
// scanSeed: Scans a single seed for both structure and biome requirements.
bool scanSeed(uint64_t seed) {
    bool hasAnyRequirements = false;
    bool allRequirementsMet = true;

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

    // ---- Structure Cluster Scanning (if enabled) ----
    if (clusterReq.enabled)
    {
        hasAnyRequirements = true;

        // 1) Collect all structure positions of the clusterReq.structureTypes into one array.
        int capacity = 128;         // initial capacity
        int clusterCount = 0;       // how many structure positions we actually store
        StructurePos *clusterPositions = malloc(capacity * sizeof(StructurePos));
        if (!clusterPositions)
        {
            perror("malloc");
            exit(1);
        }

        // For each structure type we want to cluster
        for (int i = 0; i < clusterReq.count; i++)
        {
            int stype = clusterReq.structureTypes[i];
            StructureConfig sconf;
            if (!getStructureConfig(stype, MC_1_21, &sconf))
                continue; // skip if not valid for this MC version

            // Decide which generator to use
            Generator *curr_gen = &g;
            SurfaceNoise *curr_sn = &sn;
            if (sconf.dim == DIM_NETHER)
            {
                curr_gen = &ng;
                curr_sn  = NULL; // if not needed for nether
            }
            else if (sconf.dim == DIM_END)
            {
                curr_gen = &eg;
                curr_sn  = &esn;
            }

            // Convert our (x0,z0,x1,z1) bounding box to structure-region coords
            double blocksPerRegion = sconf.regionSize * 16.0;
            int rx0 = (int) floor(x0 / blocksPerRegion);
            int rz0 = (int) floor(z0 / blocksPerRegion);
            int rx1 = (int) ceil(x1 / blocksPerRegion);
            int rz1 = (int) ceil(z1 / blocksPerRegion);

            // Scan each region for the given structure type
            for (int rz = rz0; rz <= rz1; rz++)
            {
                for (int rx = rx0; rx <= rx1; rx++)
                {
                    Pos pos;
                    if (!getStructurePos(stype, MC_1_21, seed, rx, rz, &pos))
                        continue; // structure not in this region
                    if (pos.x < x0 || pos.x > x1 || pos.z < z0 || pos.z > z1)
                        continue; // out of bounding box
                    if (!isViableStructurePos(stype, curr_gen, pos.x, pos.z, 0))
                        continue; // structure generation fails biome/height checks

                    // We have a valid structure in range; add to array
                    if (clusterCount == capacity)
                    {
                        capacity *= 2;
                        clusterPositions = realloc(clusterPositions, capacity * sizeof(StructurePos));
                        if (!clusterPositions)
                        {
                            perror("realloc");
                            exit(1);
                        }
                    }
                    clusterPositions[clusterCount].structureType = stype;
                    clusterPositions[clusterCount].x = pos.x;
                    clusterPositions[clusterCount].z = pos.z;
                    clusterCount++;
                }
            }
        } // end for each structure type


        // 2) If no structures or only one was found, you can’t form a cluster
        bool atLeastOneValidCluster = false;
        if (clusterCount >= 2)
        {
            // 3) Union–find (by distance)
            int *parent = malloc(clusterCount * sizeof(int));
            for (int i = 0; i < clusterCount; i++)
                parent[i] = i;

            // Merge if they’re within clusterDistance
            for (int i = 0; i < clusterCount; i++)
            {
                for (int j = i + 1; j < clusterCount; j++)
                {
                    int dx = clusterPositions[i].x - clusterPositions[j].x;
                    int dz = clusterPositions[i].z - clusterPositions[j].z;
                    // Euclidean distance or squared distance check
                    if (dx * dx + dz * dz <= clusterReq.clusterDistance * clusterReq.clusterDistance)
                    {
                        unionSets(parent, i, j);
                    }
                }
            }

            // 4) For each root, gather its members
            bool *processed = calloc(clusterCount, sizeof(bool));
            for (int i = 0; i < clusterCount; i++)
            {
                int root = findSet(parent, i);
                if (processed[root])
                    continue; // skip if already processed

                // Collect all structure indices in this same union-find group
                int groupSize = 0;
                int indicesCap = 16;
                int *indices = malloc(indicesCap * sizeof(int));
                for (int j = 0; j < clusterCount; j++)
                {
                    if (findSet(parent, j) == root)
                    {
                        if (groupSize == indicesCap)
                        {
                            indicesCap *= 2;
                            indices = realloc(indices, indicesCap * sizeof(int));
                        }
                        indices[groupSize++] = j;
                    }
                }
                processed[root] = true;

                // 5) If the cluster size is smaller than minClusterSize, skip it
                if (groupSize < clusterReq.minClusterSize)
                {
                    // Not big enough; skip and do NOT invalidate the seed
                    free(indices);
                    continue;
                }

                // Build a sorted array of structure types in this cluster:
                int *groupTypes = malloc(groupSize * sizeof(int));
                for (int n = 0; n < groupSize; n++)
                    groupTypes[n] = clusterPositions[indices[n]].structureType;
                qsort(groupTypes, groupSize, sizeof(int), compareInts);

                // 6) Check if this cluster has an invalid combination
                bool clusterIsInvalid = isInvalidClusterDynamic(groupTypes, groupSize);
                if (clusterIsInvalid)
                {
                    printf("Skipping invalid cluster at seed %llu (contains an invalid combination)\n",
                           (unsigned long long) seed);
                    free(groupTypes);
                    free(indices);
                    continue; // Skip just this cluster; do NOT fail the entire seed
                }

                // 7) We have a valid cluster that meets minClusterSize and is not invalid
                atLeastOneValidCluster = true;

                // Optionally print out info:
                printf("== Seed %llu: Found cluster of size %d ==\n", 
                       (unsigned long long) seed, groupSize);
                for (int n = 0; n < groupSize; n++)
                {
                    int idx = indices[n];
                    printf("   Type %d at (%d, %d)\n",
                           clusterPositions[idx].structureType,
                           clusterPositions[idx].x,
                           clusterPositions[idx].z);
                }
                seedsFound++;
                printf("\n");

                free(groupTypes);
                free(indices);
            }
            free(processed);
            free(parent);
        } // end if (clusterCount >= 2)

        free(clusterPositions);

        // 7) If no valid cluster found, fail the requirement
        if (!atLeastOneValidCluster)
            allRequirementsMet = false;  // or handle however you prefer
    }

    // ---- DYNAMIC BIOME REQUIREMENT CHECK ----
    if (biomeSearch.requiredCount > 0 || biomeSearch.clusterCount > 0) {
        hasAnyRequirements = true;
        if (!scanBiomes(&g, x0, z0, x1, z1, (BiomeSearch *)&biomeSearch)) {
            allRequirementsMet = false;
        }
    }

    // ---- Structure Scanning ----
    if (NUM_STRUCTURE_REQUIREMENTS > 0) {
        hasAnyRequirements = true;
        for (int rIndex = 0; rIndex < NUM_STRUCTURE_REQUIREMENTS; rIndex++) {
            StructureRequirement req = structureRequirements[rIndex];
            int foundCount = 0;
            StructureConfig sconf;
        if (!getStructureConfig(req.structureType, MC_1_21, &sconf))
            continue;
        Generator *curr_gen = &g;
        SurfaceNoise *curr_sn = &sn;
        if (sconf.dim == DIM_NETHER)
            curr_gen = &ng;
        else if (sconf.dim == DIM_END) {
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
                int biome_id = getBiomeAt(curr_gen, 4, pos.x >> 2, pos.z >> 2, 320 >> 2);
                if (biome_id == -1) {
                    float heightArr[256];
                    int w = 16, h = 16;
                    Range r_range = {4, pos.x >> 2, pos.z >> 2, w, h, 320 >> 2, 1};
                    mapApproxHeight(heightArr, NULL, curr_gen, curr_sn, r_range.x, r_range.z, w, h);
                    int lx = pos.x & 15;
                    int lz = pos.z & 15;
                    int surface_y = (int)heightArr[lz * w + lx];
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
                printf("Seed %llu: Found structure %d at (%d, %d) in biome %s\n",
                       seed, req.structureType, pos.x, pos.z, getBiomeName(biome_id));
                seedsFound++;
                hasAnyRequirements = true;
            }
        }
    }

    if (!hasAnyRequirements) {
        printf("Warning: No requirements set, skipping validation\n");
        return false;
    }

    if (allRequirementsMet) {
        printf("Valid seed found: %llu\n", seed);
        return true;
    }
    return false;
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
            if (seedsFound >= MAX_SEEDS_TO_FIND) {
                foundValidSeed = true;
                pthread_mutex_unlock(&seedMutex);
                break;
            }
            pthread_mutex_unlock(&seedMutex);
            break;
        }
    }
    return NULL;
}

// -----------------------------------------------------------------------------

int main() {
    printf("Checking cubiomes library...\n");

    // Initialize generator for MC 1.21
    Generator g;
    setupGenerator(&g, MC_1_21, 0);

    // Set search parameters
    uint64_t start_seed = 5031130760383654775;
    uint64_t end_seed = start_seed + 1000; // Check 1000 seeds
    int searchRadius = 1000; // Search within 500 blocks

    // Initialize reqGroup and requiredBiomes in main
    reqGroup.biomeIds = reqGroup0;
    reqGroup.biomeCount = sizeof(reqGroup0) / sizeof(reqGroup0[0]);
    reqGroup.sizeConfigs = reqSizeConfigs;
    reqGroup.configCount = sizeof(reqSizeConfigs) /sizeof(reqSizeConfigs[0]);
    reqGroup.logCenters = 1;
    requiredBiomes[0] = reqGroup;


    for (uint64_t seed = start_seed; seedsFound <= MAX_SEEDS_TO_FIND; seed++) {
        // Apply seed to generator
        applySeed(&g, DIM_OVERWORLD, seed);
        printf("Checking seed: %llu\n", (unsigned long long) seed);

        // Call scanSeed to check if this seed contains required structures
        bool structureRequirementsEmpty = (NUM_STRUCTURE_REQUIREMENTS == 0);
        if (!structureRequirementsEmpty || biomeSearch.requiredCount != 0 || clusterReq.enabled) {
       if (scanSeed(seed)) {
            printf("Valid seed found: %llu (meets structure requirements)\n", (unsigned long long) seed);
            return 0;
        }
        }

        // Define search range at biome scale (1:4)
        Range r = {
            .scale = 4,
            .x = -searchRadius / 4,
            .z = -searchRadius / 4,
            .sx = searchRadius / 2,
            .sz = searchRadius / 2,
            .y = 0,
            .sy = 1
        };

        // Allocate cache for biome generation
        int *biomeIds = allocCache(&g, r);
        if (!biomeIds) {
            printf("Failed to allocate cache\n");
            return 1;
        }

        // Generate biomes for the area
        if (genBiomes(&g, biomeIds, r)) {
            printf("Failed to generate biomes\n");
            free(biomeIds);
            return 1;
        }

        // Count connected Cherry Grove biomes
        int *visited = calloc(r.sx * r.sz, sizeof(int));
        int groupCount = 0;

        for (int z = 0; z < r.sz; z++) {
            for (int x = 0; x < r.sx; x++) {
                int idx = z * r.sx + x;
                if (visited[idx]) {
                    continue;
                }

                // Check for neighboring cells with different valid biomes first
                int hasMultipleBiomes = 0;
                int currentBiome = biomeIds[idx];
                const int dx[] = {-1, 1, 0, 0};
                const int dz[] = {0, 0, -1, 1};

                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d];
                    int nz = z + dz[d];
                    if (nx < 0 || nx >= r.sx || nz < 0 || nz >= r.sz) {
                        continue;
                    }
                    int nidx = nz * r.sx + nx;
                    int neighborBiome = biomeIds[nidx];

                    // Check if both current and neighbor are valid biomes and different
                    int currentValid = 0, neighborValid = 0;
                    for (int i = 0; i < sizeof(clusterGroup0)/sizeof(int); i++) {
                        if (currentBiome == clusterGroup0[i]) currentValid = 1;
                        if (neighborBiome == clusterGroup0[i]) neighborValid = 1;
                    }
                    if (currentValid && neighborValid && currentBiome != neighborBiome) {
                        hasMultipleBiomes = 1;
                        break;
                    }
                }

                if (!hasMultipleBiomes) {
                    continue;
                }

                // Found a new cherry grove group
                groupCount++;
                int cellCount = 0;
                double sumX = 0, sumZ = 0;

                // Simple flood fill to find connected cells
                int *stack = malloc(r.sx * r.sz * sizeof(int));
                int stackSize = 0;
                stack[stackSize++] = idx;
                visited[idx] = 1;

                while (stackSize > 0) {
                    int curr = stack[--stackSize];
                    int cx = curr % r.sx;
                    int cz = curr / r.sx;
                    cellCount++;
                    sumX += cx;
                    sumZ += cz;

                    // Check adjacent cells
                    for (int d = 0; d < 4; d++) {
                        int nx = cx + dx[d];
                        int nz = cz + dz[d];
                        if (nx < 0 || nx >= r.sx || nz < 0 || nz >= r.sz) {
                            continue;
                        }
                        int nidx = nz * r.sx + nx;
                        int biomeid = biomeIds[nidx];
                        int isValidBiome = 0;
                        for (int i = 0; i < sizeof(clusterGroup0)/sizeof(int); i++) {
                            if (biomeid == clusterGroup0[i]) {
                                isValidBiome = 1;
                                break;
                            }
                        }
                        if (!visited[nidx] && isValidBiome) {
                            stack[stackSize++] = nidx;
                            visited[nidx] = groupCount + 1;
                        }
                    }
                }
                free(stack);

                // Build cluster biome name string
                char clusterBiomes[256] = "";
                int seenBiomes[256] = {0};
                int validBiomeFound = 0;

                // Only check biomes that are in clusterGroup0
                for (int i = 0; i < sizeof(clusterGroup0)/sizeof(int); i++) {
                    int biomeId = clusterGroup0[i];
                    for (int z = 0; z < r.sz; z++) {
                        for (int x = 0; x < r.sx; x++) {
                            int idx = z * r.sx + x;
                            if (!visited[idx]) continue;
                            if (biomeIds[idx] == biomeId && !seenBiomes[biomeId]) {
                                seenBiomes[biomeId] = 1;
                                validBiomeFound = 1;
                                if (strlen(clusterBiomes) > 0) {
                                    strcat(clusterBiomes, " + ");
                                }
                                strcat(clusterBiomes, getBiomeName(biomeId));
                            }
                        }
                    }
                }

                if (!validBiomeFound) continue; // Skip if no valid biomes found

                // Calculate true center of entire cluster
                double centerX = (sumX / cellCount) * 4 + r.x * 4;
                double centerZ = (sumZ / cellCount) * 4 + r.z * 4;
                printf("Clustered biome cluster %d: %s, center at (%.1f, %.1f), total cell count %d\n",
                       groupCount, clusterBiomes, centerX, centerZ, cellCount);
            }
        }

        // Log if any valid clusters were found (cell count > 1)
        if (groupCount > 0) {
            printf("Valid seed %llu found with %d biome clusters\n", seed, groupCount);
            printf("Search area: (%d,%d) to (%d,%d)\n",
                   r.x * 4, r.z * 4,
                   (r.x + r.sx) * 4, (r.z + r.sz) * 4);

            // Exit if we found enough seeds (1 in this case)
            free(visited);
            free(biomeIds);
            return 0;
        }

        free(visited);
        free(biomeIds);
    }

    printf("Finished searching, no valid seeds found.\n");
    return 0;
}