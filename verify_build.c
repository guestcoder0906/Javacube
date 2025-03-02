// verify_build.c
#include "cubiomes/finders.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <limits.h>
#include <string.h> // For memcpy, strcmp, etc.

// -----------------------------------------------------------------------------
// Global configuration / defaults
#define MAX_SEEDS_TO_FIND 1 
#define STRUCTURE_TYPE_SPAWN 20  
int seedsFound = 0;          // Tracks how many seeds have been found so far

// Range for seed scanning:
uint64_t starting_seed = 1;
uint64_t end_seed      = 0ULL;  // Will be set in main (starting_seed + some offset)

/* Uses this:
===== Scanning options =====
Starting seed: 0
Search radius: 1000
Use spawn: false
Custom z: 0
Custom x: 0
    */
// Search radius for scanning
int searchRadius = 1000;  // e.g., +/- 1000 around spawn
int useSpawn     = 1;     // 1 = use real spawn, 0 = use (customX, customZ)
int customX      = 0;
int customZ      = 0;

// Number of threads (can be set by user)
int tasksCount = 50;

// -----------------------------------------------------------------------------
// Union–find utility for clustering
int findSet(int parent[], int i)
{
    if (parent[i] != i)
        parent[i] = findSet(parent, parent[i]);
    return parent[i];
}
void unionSets(int parent[], int x, int y)
{
    int rx = findSet(parent, x);
    int ry = findSet(parent, y);
    if (rx != ry) parent[ry] = rx;
}

// -----------------------------------------------------------------------------
// Biome ID -> Name helper
const char* getBiomeName(int id)
{
    switch(id) {
        case 0:   return "Ocean";
        case 1:   return "Plains";
        case 2:   return "Desert";
        case 3:   return "Windswept Hills";
        case 4:   return "Forest";
        case 5:   return "Taiga";
        case 6:   return "Swamp";
        case 7:   return "River";
        case 10:  return "Frozen Ocean";
        case 11:  return "Frozen River";
        case 12:  return "Snowy Plains";
        case 13:  return "Snowy Mountains";
        case 14:  return "Mushroom Fields";
        case 15:  return "Mushroom Fields Shore";
        case 16:  return "Beach";
        case 17:  return "Desert Hills";
        case 18:  return "Windswept Forest";
        case 19:  return "Taiga Hills";
        case 20:  return "Mountain Edge";
        case 21:  return "Jungle";
        case 22:  return "Jungle Hills";
        case 23:  return "Sparse Jungle";
        case 24:  return "Deep Ocean";
        case 25:  return "Stony Shore";
        case 26:  return "Snowy Beach";
        case 27:  return "Birch Forest";
        case 28:  return "Birch Forest Hills";
        case 29:  return "Dark Forest";
        case 30:  return "Snowy Taiga";
        case 31:  return "Snowy Taiga Hills";
        case 32:  return "Old Growth Pine Taiga";
        case 33:  return "Giant Tree Taiga Hills";
        case 34:  return "Wooded Mountains";
        case 35:  return "Savanna";
        case 36:  return "Savanna Plateau";
        case 37:  return "Badlands";
        case 38:  return "Wooded Badlands";
        case 39:  return "Badlands Plateau";
        case 44:  return "Warm Ocean";
        case 45:  return "Lukewarm Ocean";
        case 46:  return "Cold Ocean";
        case 47:  return "Deep Warm Ocean";
        case 48:  return "Deep Lukewarm Ocean";
        case 49:  return "Deep Cold Ocean";
        case 50:  return "Deep Frozen Ocean";
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
        case 187: return "Island";  // <-- New custom island biome ID
        default:  return "Unknown";
    }
}

// Structure type -> Name helper
const char* getStructureName(int id)
{
    switch(id) {
        case 0:  return "Feature";
        case 1:  return "Desert Pyramid";
        case 2:  return "Jungle Temple";
        case 3:  return "Swamp Hut";
        case 4:  return "Igloo";
        case 5:  return "Village";
        case 6:  return "Ocean Ruin";
        case 7:  return "Shipwreck";
        case 8:  return "Monument";
        case 9:  return "Mansion";
        case 10: return "Outpost";
        case 11: return "Ruined Portal";
        case 12: return "Ruined Portal N";
        case 13: return "Ancient City";
        case 14: return "Treasure";
        case 15: return "Mineshaft";
        case 16: return "Desert Well";
        case 17: return "Geode";
        case 18: return "Trail Ruins";
        case 19: return "Trial Chambers";
        case 20: return "Spawn Point";
        default: return "Unknown Structure";
    }
}

// -----------------------------------------------------------------------------
// Found positions
typedef struct {
    int structureType; 
    int x;
    int z;
} StructurePos;

// -----------------------------------------------------------------------------
// New helper functions for island biome detection

// Returns true if the given biome ID corresponds to an ocean-type biome.
bool isOceanBiome(int id)
{
    return (id == 0 || id == 10 || id == 24 || id == 44 || id == 45 ||
            id == 46 || id == 47 || id == 48 || id == 49 || id == 50);
}

// Given a group of candidate cells (from union-find) and the search bounds, returns true
// if every cell at the group’s boundary has an ocean neighbor.
bool isIslandPatch(Generator *g, StructurePos *group, int groupCount, int step, int x0, int z0, int x1, int z1)
{
    if (!g || !group || groupCount <= 0)
        return false;

    for (int i = 0; i < groupCount; i++) {
         int cx = group[i].x;
         int cz = group[i].z;
         int directions[4][2] = { {step, 0}, {-step, 0}, {0, step}, {0, -step} };
         for (int d = 0; d < 4; d++) {
              int nx = cx + directions[d][0];
              int nz = cz + directions[d][1];
              if (nx < x0 || nx > x1 || nz < z0 || nz > z1)
                  return false;
              bool inGroup = false;
              for (int j = 0; j < groupCount; j++) {
                   if (group[j].x == nx && group[j].z == nz) {
                       inGroup = true;
                       break;
                   }
              }
              if (!inGroup) {
                  int neighborBiome = getBiomeAt(g, 4, nx >> 2, 0, nz >> 2);
                  if (!isOceanBiome(neighborBiome))
                      return false;
              }
         }
    }
    return true;
}

// Flood-fill a candidate island patch starting at (x,z) within the search bounds,
// returning its patch size (number of cells) and center coordinates via centerX/centerZ.
// Also outputs the bounding box of the island patch (minPatchX, maxPatchX, minPatchZ, maxPatchZ).
// Returns 0 if the patch is not a valid island (i.e. not fully surrounded by ocean).
int getIslandPatchInfo(Generator *g, int x, int z, int x0, int z0, int x1, int z1,
                         double *centerX, double *centerZ,
                         int *minPatchX, int *maxPatchX, int *minPatchZ, int *maxPatchZ)
{
    if (!g || !centerX || !centerZ)
        return 0;

    int stepSize = 4;
    typedef struct { int x, z; } Pos2;
    const int maxCells = 50000;
    Pos2 *stack = malloc(maxCells * sizeof(Pos2));
    if (!stack) { perror("malloc"); exit(1); }

    int stackSize = 0;
    Pos2 *group = malloc(maxCells * sizeof(Pos2));
    if (!group) { perror("malloc"); free(stack); exit(1); }

    int groupCount = 0;
    int groupMinX = INT_MAX, groupMaxX = INT_MIN, groupMinZ = INT_MAX, groupMaxZ = INT_MIN;
    stack[stackSize++] = (Pos2){x, z};

    while (stackSize > 0 && stackSize < maxCells && groupCount < maxCells) {
        Pos2 cur = stack[--stackSize];
        bool inGroup = false;
        for (int i = 0; i < groupCount; i++) {
            if (group[i].x == cur.x && group[i].z == cur.z) { inGroup = true; break; }
        }
        if (inGroup) continue;

        group[groupCount++] = cur;
        if (cur.x < groupMinX) groupMinX = cur.x;
        if (cur.x > groupMaxX) groupMaxX = cur.x;
        if (cur.z < groupMinZ) groupMinZ = cur.z;
        if (cur.z > groupMaxZ) groupMaxZ = cur.z;

        int dx[4] = {stepSize, -stepSize, 0, 0};
        int dz[4] = {0, 0, stepSize, -stepSize};

        for (int d = 0; d < 4; d++) {
            int nx = cur.x + dx[d];
            int nz = cur.z + dz[d];
            if (nx < x0 || nx > x1 || nz < z0 || nz > z1) continue;
            int nb = getBiomeAt(g, 4, nx >> 2, 0, nz >> 2);
            if (!isOceanBiome(nb)) {
                bool already = false;
                for (int k = 0; k < groupCount; k++) {
                    if (group[k].x == nx && group[k].z == nz) { already = true; break; }
                }
                if (!already && stackSize < maxCells) {
                    stack[stackSize++] = (Pos2){nx, nz};
                }
            }
        }
    }

    free(stack);
    bool isIsland = true;
    double sumX = 0, sumZ = 0;

    for (int i = 0; i < groupCount; i++) {
        int cx = group[i].x;
        int cz = group[i].z;
        sumX += cx;
        sumZ += cz;
        int dx[4] = {stepSize, -stepSize, 0, 0};
        int dz[4] = {0, 0, stepSize, -stepSize};

        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d];
            int nz = cz + dz[d];
            if (nx < x0 || nx > x1 || nz < z0 || nz > z1) continue;
            bool inGroup = false;
            for (int j = 0; j < groupCount; j++) {
                if (group[j].x == nx && group[j].z == nz) { inGroup = true; break; }
            }
            if (!inGroup) {
                int nb = getBiomeAt(g, 4, nx >> 2, 0, nz >> 2);
                if (!isOceanBiome(nb)) {
                    isIsland = false;
                    break;
                }
            }
        }
        if (!isIsland) break;
    }

    if (!isIsland || groupCount == 0) {
        free(group);
        return 0;
    }

    *centerX = sumX / groupCount;
    *centerZ = sumZ / groupCount;
    if (minPatchX) *minPatchX = groupMinX;
    if (maxPatchX) *maxPatchX = groupMaxX;
    if (minPatchZ) *minPatchZ = groupMinZ;
    if (maxPatchZ) *maxPatchZ = groupMaxZ;
    int patchSize = groupCount;
    free(group);
    return patchSize;
}

// -----------------------------------------------------------------------------
// Approx. function to measure the "patch size" of a given biome around (x,z).
int getBiomePatchSize(Generator *g, int x, int z, int biome_id)
{
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
            }
            else if (found && (bx > maxX + 2 || bz > maxZ + 2)) {
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
// Required structure conditions
typedef struct {
    int structureType;   // e.g. 5 -> Village, etc. (Cubiomes structure ID)
    int minCount;
    int minHeight;
    int maxHeight;
    int requiredBiome;   // -1 => skip biome check
    int minBiomeSize;    // -1 => no minimum
    int maxBiomeSize;    // -1 => no maximum
    int *proximityBiomes; // Array of biome IDs to check proximity
    int proximityBiomeCount; // Number of biomes in proximityBiomes
    int biomeProximity;  // -1 => skip proximity check, otherwise max distance
} StructureRequirement;

// Per-biome size config for required patches
typedef struct {
    int biomeId;
    int minSize; // -1 if no minimum
    int maxSize; // -1 if no maximum
} BiomeSizeConfig;

// For "required" biomes
typedef struct {
    int *biomeIds;
    int biomeCount;
    BiomeSizeConfig *sizeConfigs;
    int configCount;
    int logCenters; // whether to print out patch centers
} BiomeRequirement;

// For "clustered" biomes
typedef struct {
    int *biomeIds;
    int biomeCount;
    int minSize;
    int maxSize;
    int logCenters; // whether to print out cluster centers
} BiomeCluster;

// Top-level container for biome searches
typedef struct {
    BiomeRequirement *required;
    int requiredCount;
    BiomeCluster *clusters;
    int clusterCount;
} BiomeSearch;

// We will dynamically build these from the parameter file
BiomeRequirement *g_requiredBiomes   = NULL; 
int g_requiredBiomesCount            = 0;

BiomeCluster    *g_biomeClusters     = NULL;
int g_biomeClustersCount             = 0;

BiomeSearch biomeSearch = {
    .required      = NULL,
    .requiredCount = 0,
    .clusters      = NULL,
    .clusterCount  = 0
};

// -----------------------------------------------------------------------------
// Structure cluster requirement
typedef struct {
    bool enabled;
    int clusterDistance;
    int *structureTypes;
    int count;
    int minClusterSize;  
} ClusterRequirement;

// We will fill this from the parameter file
ClusterRequirement clusterReq = {
    .enabled         = false,
    .clusterDistance = 32,
    .structureTypes  = NULL,
    .count           = 0,
    .minClusterSize  = 2
};

// -----------------------------------------------------------------------------
// Optional invalid combination
typedef struct {
    int *types;
    int count;
} InvalidCombination;

// We'll store them dynamically as well
InvalidCombination *invalidCombinations = NULL;
int numInvalidCombinations = 0;

// -----------------------------------------------------------------------------
// Our array of required structures (dynamically built)
StructureRequirement *structureRequirements = NULL;
int NUM_STRUCTURE_REQUIREMENTS = 0;

// -----------------------------------------------------------------------------
// Helper compare function
int compareInts(const void *a, const void *b)
{
    int A = *(const int*)a;
    int B = *(const int*)b;
    return (A - B);
}

bool isInvalidClusterDynamic(int *groupTypes, int groupSize)
{
    for (int ic = 0; ic < numInvalidCombinations; ic++) {
        int *invalidSet   = invalidCombinations[ic].types;
        int invalidCount  = invalidCombinations[ic].count;

        if (groupSize != invalidCount)
            continue;

        qsort(groupTypes, groupSize, sizeof(int), compareInts);
        qsort(invalidSet, invalidCount, sizeof(int), compareInts);

        bool exactMatch = true;
        for (int k = 0; k < groupSize; k++) {
            if (groupTypes[k] != invalidSet[k]) {
                exactMatch = false;
                break;
            }
        }
        if (exactMatch)
            return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// scanBiomes: For required & clustered biome conditions (if any)
bool scanBiomes(Generator *g, int x0, int z0, int x1, int z1, BiomeSearch *bs)
{
    if (bs->requiredCount == 0 && bs->clusterCount == 0)
        return true;
    bool success = true; 
    int step = 4;

    // 1) Required biome patches
    if (bs->requiredCount > 0) {
        bool allRequirementsFound = true;
        for (int i = 0; i < bs->requiredCount; i++) {
            BiomeRequirement *req = &bs->required[i];
            bool foundPatchForThisReq = false;
            int capacity = 128, count = 0;
            StructurePos *positions = malloc(capacity * sizeof(StructurePos));
            if (!positions) { perror("malloc"); exit(1); }

            for (int zz = z0; zz <= z1; zz += step) {
                for (int xx = x0; xx <= x1; xx += step) {
                    int biome = getBiomeAt(g, 4, xx >> 2, 0, zz >> 2);
                    for (int b = 0; b < req->biomeCount; b++) {
                        bool match = false;
                        if (req->biomeIds[b] == 187) {
                            if (!isOceanBiome(biome))
                                match = true;
                        } else {
                            if (biome == req->biomeIds[b])
                                match = true;
                        }
                        if (match) {
                            if (count == capacity) {
                                capacity *= 2;
                                positions = realloc(positions, capacity * sizeof(StructurePos));
                                if (!positions) { perror("realloc"); exit(1); }
                            }
                            positions[count].structureType = biome;
                            positions[count].x = xx;
                            positions[count].z = zz;
                            count++;
                            break; 
                        }
                    }
                }
            }

            if (count == 0) {
                free(positions);
                allRequirementsFound = false;
                continue;
            }

            int *parent = malloc(count * sizeof(int));
            if (!parent) { perror("malloc"); exit(1); }
            for (int c = 0; c < count; c++)
                parent[c] = c;

            for (int c = 0; c < count; c++) {
                for (int d = c + 1; d < count; d++) {
                    int dx = abs(positions[c].x - positions[d].x);
                    int dz = abs(positions[c].z - positions[d].z);
                    if (dx <= step && dz <= step &&
                        positions[c].structureType == positions[d].structureType)
                    {
                        unionSets(parent, c, d);
                    }
                }
            }

            bool *processed = calloc(count, sizeof(bool));
            if (!processed) { perror("calloc"); exit(1); }

            for (int c = 0; c < count; c++) {
                int root = findSet(parent, c);
                if (processed[root]) continue;
                processed[root] = true;

                double sumX = 0, sumZ = 0;
                int compCount = 0;
                int theBiome = positions[c].structureType;

                for (int e = 0; e < count; e++) {
                    if (findSet(parent, e) == root) {
                        sumX += positions[e].x;
                        sumZ += positions[e].z;
                        compCount++;
                    }
                }
                double centerX = sumX / compCount;
                double centerZ = sumZ / compCount;

                bool patchOk = true;
                for (int b = 0; b < req->biomeCount; b++) {
                    if (req->biomeIds[b] == 187) {
                        StructurePos *groupCells = malloc(compCount * sizeof(StructurePos));
                        if (!groupCells) { perror("malloc"); exit(1); }
                        int idx = 0;
                        for (int e = 0; e < count; e++) {
                            if (findSet(parent, e) == root) {
                                groupCells[idx++] = positions[e];
                            }
                        }
                        if (!isIslandPatch(g, groupCells, compCount, step, x0, z0, x1, z1))
                            patchOk = false;
                        free(groupCells);
                        break;
                    } else {
                        for (int sc = 0; sc < req->configCount; sc++) {
                            if (req->sizeConfigs[sc].biomeId == theBiome) {
                                int mn = req->sizeConfigs[sc].minSize;
                                int mx = req->sizeConfigs[sc].maxSize;
                                if (mn > -1 && compCount < mn) patchOk = false;
                                if (mx > -1 && compCount > mx) patchOk = false;
                                break;
                            }
                        }
                    }
                }
                if (patchOk) {
                    foundPatchForThisReq = true;
                    if (req->logCenters) {
                        printf("Required biome patch (reqIndex=%d, biome=%s): center (%.1f,%.1f), cells=%d\n",
                               i, (req->biomeIds[0] == 187 ? "Island" : getBiomeName(theBiome)),
                               centerX, centerZ, compCount);
                    }
                }
            }
            free(processed);
            free(parent);
            free(positions);

            if (!foundPatchForThisReq)
                allRequirementsFound = false;
        }

        if (!allRequirementsFound)
            success = false;
    }

    // 2) Clustered biomes
    if (bs->clusterCount > 0) {
        for (int i = 0; i < bs->clusterCount; i++) {
            BiomeCluster *cl = &bs->clusters[i];
            int capacity = 128, count = 0;
            StructurePos *positions = malloc(capacity * sizeof(StructurePos));
            if (!positions) { perror("malloc"); exit(1); }

            for (int zz = z0; zz <= z1; zz += step) {
                for (int xx = x0; xx <= x1; xx += step) {
                    int biome = getBiomeAt(g, 4, xx >> 2, 0, zz >> 2);
                    for (int b = 0; b < cl->biomeCount; b++) {
                        if (biome == cl->biomeIds[b]) {
                            if (count == capacity) {
                                capacity *= 2;
                                positions = realloc(positions, capacity*sizeof(StructurePos));
                                if (!positions) { perror("realloc"); exit(1); }
                            }
                            positions[count].structureType = biome;
                            positions[count].x = xx;
                            positions[count].z = zz;
                            count++;
                            break; 
                        }
                    }
                }
            }

            if (count == 0) {
                free(positions);
                continue;
            }

            int *parent = malloc(count * sizeof(int));
            if (!parent) { perror("malloc"); exit(1); }
            for (int c = 0; c < count; c++)
                parent[c] = c;

            for (int c = 0; c < count; c++) {
                for (int d = c+1; d < count; d++) {
                    int dx = abs(positions[c].x - positions[d].x);
                    int dz = abs(positions[c].z - positions[d].z);
                    if (dx <= step && dz <= step)
                        unionSets(parent, c, d);
                }
            }

            bool *processed = calloc(count, sizeof(bool));
            if (!processed) { perror("calloc"); exit(1); }

            for (int c = 0; c < count; c++) {
                int root = findSet(parent, c);
                if (processed[root]) continue;
                processed[root] = true;

                double sumX = 0, sumZ = 0;
                int compCount = 0;
                bool usedBiome[256];
                memset(usedBiome, false, sizeof(usedBiome));

                for (int e = 0; e < count; e++) {
                    if (findSet(parent, e) == root) {
                        sumX += positions[e].x;
                        sumZ += positions[e].z;
                        compCount++;
                        int bId = positions[e].structureType;
                        if (bId >= 0 && bId < 256)
                            usedBiome[bId] = true;
                    }
                }

                int distinctIDs[256];
                int dIdx = 0;
                for (int bId = 0; bId < 256; bId++) {
                    if (usedBiome[bId])
                        distinctIDs[dIdx++] = bId;
                }

                bool sizeOk = true;
                if (cl->minSize > -1 && compCount < cl->minSize) sizeOk = false;
                if (cl->maxSize > -1 && compCount > cl->maxSize) sizeOk = false;

                if (sizeOk && cl->logCenters) {
                    double cx = sumX / compCount;
                    double cz = sumZ / compCount;
                    printf("Clustered biome group %d: (distinct biome count=%d), center(%.1f,%.1f), count=%d\n",
                           i+1, dIdx, cx, cz, compCount);
                }
            }
            free(processed);
            free(parent);
            free(positions);
        }
    }
    return success;
}

// Structure to track biome blocks
typedef struct {
    int x;
    int z;
    int biomeId;
} BiomeBlock;

// -----------------------------------------------------------------------------
// Check if a structure is close enough to any of the required biomes
int checkBiomeProximity(Generator *g, int structX, int structZ, int *biomesToCheck, int biomeCount, 
                        int maxDistance, int *outDistance, int *outBiomeId) 
{
    if (outDistance) *outDistance = -1;
    if (outBiomeId) *outBiomeId = -1;

    if (biomeCount <= 0 || maxDistance < 0)
        return 1; // Skip check if disabled

    int searchRadius = maxDistance + 8;
    int x0 = structX - searchRadius;
    int z0 = structZ - searchRadius;
    int x1 = structX + searchRadius;
    int z1 = structZ + searchRadius;

    int capacity = 1000;
    BiomeBlock *biomeBlocks = malloc(capacity * sizeof(BiomeBlock));
    if (!biomeBlocks) { perror("malloc"); exit(1); }
    int blockCount = 0;

    for (int z = z0; z <= z1; z += 4) {
        for (int x = x0; x <= x1; x += 4) {
            int biome = getBiomeAt(g, 4, x >> 2, 0, z >> 2);
            for (int b = 0; b < biomeCount; b++) {
                if (biome == biomesToCheck[b]) {
                    if (blockCount >= capacity) {
                        capacity *= 2;
                        biomeBlocks = realloc(biomeBlocks, capacity * sizeof(BiomeBlock));
                        if (!biomeBlocks) { perror("realloc"); exit(1); }
                    }
                    biomeBlocks[blockCount].x = x;
                    biomeBlocks[blockCount].z = z;
                    biomeBlocks[blockCount].biomeId = biome;
                    blockCount++;
                    break;
                }
            }
        }
    }

    if (blockCount == 0) {
        free(biomeBlocks);
        return 0;
    }

    int minDistance = INT_MAX;
    int closestBiomeId = -1;
    for (int i = 0; i < blockCount; i++) {
        int dx = structX - biomeBlocks[i].x;
        int dz = structZ - biomeBlocks[i].z;
        int distance = (int)sqrt(dx*dx + dz*dz);
        if (distance < minDistance) {
            minDistance = distance;
            closestBiomeId = biomeBlocks[i].biomeId;
        }
    }
    free(biomeBlocks);
    if (minDistance <= maxDistance) {
        if (outDistance) *outDistance = minDistance;
        if (outBiomeId) *outBiomeId = closestBiomeId;
        return 1;
    }
    return 0;
}

// -----------------------------------------------------------------------------
// Shared concurrency variables
volatile bool foundValidSeed    = false;
pthread_mutex_t seedMutex       = PTHREAD_MUTEX_INITIALIZER;
uint64_t validSeed              = 0;
volatile uint64_t currentSeed   = 0; // threads will increment this

// -----------------------------------------------------------------------------
// Main seed scanning logic (structures + biome checks)
int scanSeed(uint64_t seed)
{
    bool hasAnyRequirements = false;
    bool allRequirementsMet = true;

    Generator g;
    setupGenerator(&g, MC_1_21, 0);
    applySeed(&g, DIM_OVERWORLD, seed);

    Pos spawn = {0,0};
    if (useSpawn)
        spawn = getSpawn(&g);
    int x0 = (useSpawn ? (spawn.x - searchRadius) : (customX - searchRadius));
    int z0 = (useSpawn ? (spawn.z - searchRadius) : (customZ - searchRadius));
    int x1 = x0 + (searchRadius * 2);
    int z1 = z0 + (searchRadius * 2);

    Generator ng, eg;
    setupGenerator(&ng, MC_1_21, 0);
    applySeed(&ng, DIM_NETHER, seed);
    setupGenerator(&eg, MC_1_21, 0);
    applySeed(&eg, DIM_END, seed);

    SurfaceNoise sn, esn;
    initSurfaceNoise(&sn, DIM_OVERWORLD, seed);
    initSurfaceNoise(&esn, DIM_END, seed);

    // 1) Structure cluster scanning
    if (clusterReq.enabled) {
        hasAnyRequirements = true;
        int capacity = 128;
        int clusterCount = 0;
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
                curr_sn  = NULL;
            } else if (sconf.dim == DIM_END) {
                curr_gen = &eg;
                curr_sn  = &esn;
            }

            double blocksPerRegion = sconf.regionSize * 16.0;
            int rx0 = (int)floor(x0 / blocksPerRegion);
            int rz0 = (int)floor(z0 / blocksPerRegion);
            int rx1 = (int)ceil(x1 / blocksPerRegion);
            int rz1 = (int)ceil(z1 / blocksPerRegion);

            for (int rz = rz0; rz <= rz1; rz++) {
                for (int rx = rx0; rx <= rx1; rx++) {
                    Pos pos;
                    if (!getStructurePos(stype, MC_1_21, seed, rx, rz, &pos))
                        continue;
                    if (pos.x < x0 || pos.x > x1 || pos.z < z0 || pos.z > z1)
                        continue;
                    if (!isViableStructurePos(stype, curr_gen, pos.x, pos.z, 0))
                        continue;

                    if (clusterCount == capacity) {
                        capacity *= 2;
                        clusterPositions = realloc(clusterPositions, capacity*sizeof(StructurePos));
                        if (!clusterPositions) { perror("malloc"); exit(1); }
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
            int *parent = malloc(clusterCount*sizeof(int));
            if (!parent) { perror("malloc"); exit(1); }
            for (int i = 0; i < clusterCount; i++)
                parent[i] = i;

            for (int i = 0; i < clusterCount; i++) {
                for (int j = i+1; j < clusterCount; j++) {
                    int dx = clusterPositions[i].x - clusterPositions[j].x;
                    int dz = clusterPositions[i].z - clusterPositions[j].z;
                    if (dx*dx + dz*dz <= clusterReq.clusterDistance * clusterReq.clusterDistance)
                        unionSets(parent, i, j);
                }
            }

            bool *processed = calloc(clusterCount, sizeof(bool));
            if (!processed) { perror("calloc"); exit(1); }

            for (int i = 0; i < clusterCount; i++) {
                int root = findSet(parent, i);
                if (processed[root]) continue;
                processed[root] = true;

                int *indices = malloc(16*sizeof(int));
                int indicesCap = 16;
                int groupSize = 0;

                for (int j = 0; j < clusterCount; j++) {
                    if (findSet(parent, j) == root) {
                        if (groupSize == indicesCap) {
                            indicesCap *= 2;
                            indices = realloc(indices, indicesCap*sizeof(int));
                        }
                        indices[groupSize++] = j;
                    }
                }

                if (groupSize < clusterReq.minClusterSize) {
                    free(indices);
                    continue;
                }

                int*groupTypes = malloc(groupSize*sizeof(int));
                for (int n = 0; n < groupSize; n++) {
                    groupTypes[n] = clusterPositions[indices[n]].structureType;
                }
                qsort(groupTypes, groupSize, sizeof(int), compareInts);

                bool invalid = isInvalidClusterDynamic(groupTypes, groupSize);
                if (invalid) {
                    free(groupTypes);
                    free(indices);
                    continue;
                }

                atLeastOneValidCluster = true;
                printf("== Seed %llu: Found cluster of size %d ==\n",
                       (unsigned long long) seed, groupSize);
                for (int n = 0; n < groupSize; n++) {
                    int idx = indices[n];
                    printf("   Type %d at (%d, %d)\n",
                           clusterPositions[idx].structureType,
                           clusterPositions[idx].x,
                           clusterPositions[idx].z);
                }
                printf("\n");

                free(groupTypes);
                free(indices);
            }
            free(processed);
            free(parent);
        }
        free(clusterPositions);

        if (!atLeastOneValidCluster)
            allRequirementsMet = false;
    }

    // 2) Biome requirements
    if (biomeSearch.requiredCount > 0 || biomeSearch.clusterCount > 0) {
        hasAnyRequirements = true;
        if (!scanBiomes(&g, x0, z0, x1, z1, &biomeSearch))
            allRequirementsMet = false;
    }

    // 3) Additional structure requirements array
    if (NUM_STRUCTURE_REQUIREMENTS > 0) {
        hasAnyRequirements = true;
        typedef struct {
            int x, y, z;
            int biome_id;
            int biome_size;
            int proximity_distance;
            int proximity_biome_id;
        } FoundPos;

        FoundPos foundPositions[256];
        int foundPosCount = 0;

        for (int rIndex = 0; rIndex < NUM_STRUCTURE_REQUIREMENTS; rIndex++) {
            StructureRequirement req = structureRequirements[rIndex];
            int foundCount = 0;

            // Handle spawn point like other structures
            if (req.structureType == STRUCTURE_TYPE_SPAWN) {
                Pos spawnPos = getSpawn(&g);
                float heightArr[256];
                int w = 16, h = 16;
                Range r_range = {4, spawnPos.x >> 2, spawnPos.z >> 2, w, h, 1, 1};
                mapApproxHeight(heightArr, NULL, &g, &sn, r_range.x, r_range.z, w, h);
                int lx = spawnPos.x & 15;
                int lz = spawnPos.z & 15;
                int height = (int)heightArr[lz*w + lx];

                int biome_id = getBiomeAt(&g, 4, spawnPos.x >> 2, height >> 2, spawnPos.z >> 2);
                if (req.requiredBiome == 187) {
                    double islandCenterX, islandCenterZ;
                    int minPatchX, maxPatchX, minPatchZ, maxPatchZ;
                    int patchSize = getIslandPatchInfo(&g, spawnPos.x, spawnPos.z, x0, z0, x1, z1,
                                                       &islandCenterX, &islandCenterZ,
                                                       &minPatchX, &maxPatchX, &minPatchZ, &maxPatchZ);
                    if (patchSize == 0) continue;
                    if ((req.minBiomeSize != -1 && patchSize < req.minBiomeSize) ||
                        (req.maxBiomeSize != -1 && patchSize > req.maxBiomeSize))
                        continue;
                    if (spawnPos.x < minPatchX || spawnPos.x > maxPatchX ||
                        spawnPos.z < minPatchZ || spawnPos.z > maxPatchZ)
                        continue;
                    biome_id = 187;
                }
                if ((req.minHeight != -9999 && height < req.minHeight) ||
                    (req.maxHeight != 9999 && height > req.maxHeight))
                    continue;

                int proximity_distance = -1;
                int closest_biome_id = -1;
                if (req.proximityBiomeCount > 0 && req.biomeProximity > 0) {
                    if (!checkBiomeProximity(&g, spawnPos.x, spawnPos.z, 
                                             req.proximityBiomes, req.proximityBiomeCount, 
                                             req.biomeProximity, &proximity_distance, &closest_biome_id))
                        continue;
                }

                int biome_size = -1;
                if (req.minBiomeSize != -1 || req.maxBiomeSize != -1) {
                    biome_size = getBiomePatchSize(&g, spawnPos.x, spawnPos.z, closest_biome_id != -1 ? closest_biome_id : biome_id);
                    if ((req.minBiomeSize != -1 && biome_size < req.minBiomeSize) ||
                        (req.maxBiomeSize != -1 && biome_size > req.maxBiomeSize))
                        continue;
                }

                foundPositions[foundPosCount].x = spawnPos.x;
                foundPositions[foundPosCount].y = height;
                foundPositions[foundPosCount].z = spawnPos.z;
                foundPositions[foundPosCount].biome_id = biome_id;
                foundPositions[foundPosCount].biome_size = biome_size;
                foundPositions[foundPosCount].proximity_distance = proximity_distance;
                foundPositions[foundPosCount].proximity_biome_id = closest_biome_id;
                foundPosCount++;
                foundCount++;
            }
            else {
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

                for (int rz = rz0; rz <= rz1; rz++) {
                    for (int rx = rx0; rx <= rx1; rx++) {
                        Pos pos;
                        if (!getStructurePos(req.structureType, MC_1_21, seed, rx, rz, &pos))
                            continue;
                        if (pos.x < x0 || pos.x > x1 || pos.z < z0 || pos.z > z1)
                            continue;
                        if (!isViableStructurePos(req.structureType, curr_gen, pos.x, pos.z, 0))
                            continue;

                        int biome_id = -1;
                        int checkUnderground = (req.structureType == 17 || req.structureType == 15 || 
                                                req.structureType == 14 || req.structureType == 11);
                        if (checkUnderground)
                            biome_id = getBiomeAt(curr_gen, 4, pos.x >> 2, 0, pos.z >> 2);
                        else {
                            float heightArr[256];
                            int w = 16, h = 16;
                            Range r_range = {4, pos.x >> 2, pos.z >> 2, w, h, 1, 1};
                            mapApproxHeight(heightArr, NULL, curr_gen, curr_sn, r_range.x, r_range.z, w, h);
                            int lx = pos.x & 15;
                            int lz = pos.z & 15;
                            int surface_y = (int)heightArr[lz*w + lx];
                            biome_id = getBiomeAt(curr_gen, 4, pos.x >> 2, surface_y >> 2, pos.z >> 2);
                        }

                        if (req.requiredBiome != -1) {
                            if (req.requiredBiome == 187) {
                                double islandCenterX, islandCenterZ;
                                int minPatchX, maxPatchX, minPatchZ, maxPatchZ;
                                int patchSize = getIslandPatchInfo(curr_gen, pos.x, pos.z, x0, z0, x1, z1,
                                                                   &islandCenterX, &islandCenterZ,
                                                                   &minPatchX, &maxPatchX, &minPatchZ, &maxPatchZ);
                                if (patchSize == 0) continue;
                                if ((req.minBiomeSize != -1 && patchSize < req.minBiomeSize) ||
                                    (req.maxBiomeSize != -1 && patchSize > req.maxBiomeSize))
                                    continue;
                                if (pos.x < minPatchX || pos.x > maxPatchX || pos.z < minPatchZ || pos.z > maxPatchZ)
                                    continue;
                                biome_id = 187;
                            } else {
                                if (biome_id != req.requiredBiome)
                                    continue;
                                if (req.minBiomeSize != -1 || req.maxBiomeSize != -1) {
                                    int patchSize = getBiomePatchSize(curr_gen, pos.x, pos.z, biome_id);
                                    if ((req.minBiomeSize != -1 && patchSize < req.minBiomeSize) ||
                                        (req.maxBiomeSize != -1 && patchSize > req.maxBiomeSize))
                                        continue;
                                }
                            }
                        }

                        int height = 0;
                        if (req.structureType == 19 || req.structureType == 17 || 
                            req.structureType == 15 || req.structureType == 14 || 
                            req.structureType == 11) {
                            StructureVariant sv;
                            if (getVariant(&sv, req.structureType, MC_1_21, seed, pos.x, pos.z, biome_id))
                                height = sv.y;
                        } else {
                            float heightArr[256];
                            int w = 16, h = 16;
                            Range r_range = {4, pos.x >> 2, pos.z >> 2, w, h, 1, 1};
                            mapApproxHeight(heightArr, NULL, curr_gen, curr_sn, r_range.x, r_range.z, w, h);
                            int lx = pos.x & 15;
                            int lz = pos.z & 15;
                            height = (int)heightArr[lz*w + lx];
                        }

                        if ((req.minHeight != -9999 && height < req.minHeight) ||
                            (req.maxHeight != 9999 && height > req.maxHeight))
                            continue;

                        int proximity_distance = -1;
                        int closest_biome_id = -1;
                        if (req.proximityBiomeCount > 0 && req.biomeProximity > 0) {
                            if (!checkBiomeProximity(curr_gen, pos.x, pos.z, 
                                                       req.proximityBiomes, req.proximityBiomeCount, 
                                                       req.biomeProximity, &proximity_distance, &closest_biome_id))
                                continue;
                        }

                        foundPositions[foundPosCount].x = pos.x;
                        foundPositions[foundPosCount].z = pos.z;
                        foundPositions[foundPosCount].y = height;
                        foundPositions[foundPosCount].biome_id = biome_id;
                        foundPositions[foundPosCount].biome_size = (biome_id != -1) ? getBiomePatchSize(curr_gen, pos.x, pos.z, biome_id) : -1;
                        foundPositions[foundPosCount].proximity_distance = proximity_distance;
                        foundPositions[foundPosCount].proximity_biome_id = closest_biome_id;
                        foundPosCount++;
                        foundCount++;
                    }
                }
            }

            if (foundCount < req.minCount)
                allRequirementsMet = false;
        }

        if (allRequirementsMet) {
            bool printedHeader = false;
            for (int i = 0; i < NUM_STRUCTURE_REQUIREMENTS; i++) {
                StructureRequirement req = structureRequirements[i];
                bool hasProximityReq = (req.proximityBiomeCount > 0 && req.biomeProximity > 0);
                bool foundValidStructure = false;

                for (int j = 0; j < foundPosCount; j++) {
                    if (req.requiredBiome != -1 && foundPositions[j].biome_id != req.requiredBiome)
                        continue;
                    if (hasProximityReq) {
                        if (foundPositions[j].proximity_distance > 0) {
                            foundValidStructure = true;
                            break;
                        }
                        continue;
                    } else {
                        foundValidStructure = true;
                        break;
                    }
                }

                if (foundValidStructure) {
                    if (!printedHeader)
                        printedHeader = true;
                    printf("Seed: %llu\n", (unsigned long long)seed);
                    printf("Structures %s:\n", getStructureName(req.structureType));

                    for (int j = 0; j < foundPosCount; j++) {
                        if (req.requiredBiome != -1 && foundPositions[j].biome_id != req.requiredBiome)
                            continue;
                        if (hasProximityReq && foundPositions[j].proximity_distance <= -1)
                            continue;
                        if (req.requiredBiome == -1 || foundPositions[j].biome_id == req.requiredBiome) {
                            printf("%s at (%d, %d) with height at %d in %s Biome with %d size",
                                getStructureName(req.structureType), 
                                foundPositions[j].x, 
                                foundPositions[j].z,
                                foundPositions[j].y, 
                                getBiomeName(foundPositions[j].biome_id),
                                foundPositions[j].biome_size);
                            if (foundPositions[j].proximity_distance > 0) {
                                printf(", %d blocks from nearest %s biome", 
                                      foundPositions[j].proximity_distance,
                                      getBiomeName(foundPositions[j].proximity_biome_id));
                            }
                            printf("\n");
                        }
                    }
                }
            }
            return true;
        }
    }

    if (!hasAnyRequirements) {
        printf("Warning: No requirements set, skipping validation for seed %llu.\n",
               (unsigned long long) seed);
        return false;
    }

    if (allRequirementsMet) {
        printf("Valid seed found: %llu\n", (unsigned long long) seed);
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Thread function: scans seeds in [currentSeed..end_seed]
void *scanTask(void *arg)
{
    uint64_t *endSeedPtr = (uint64_t*) arg;
    while (true) {
        pthread_mutex_lock(&seedMutex);
        if (currentSeed > *endSeedPtr) {
            pthread_mutex_unlock(&seedMutex);
            break;
        }
        uint64_t seed = currentSeed;
        currentSeed++;
        if (seed % 10000 == 0)
            printf("Seeds scanned: %llu\n", (unsigned long long)seed);
        pthread_mutex_unlock(&seedMutex);
        if (scanSeed(seed)) {
            pthread_mutex_lock(&seedMutex);
            validSeed = seed;
            seedsFound++;
            if (seedsFound >= MAX_SEEDS_TO_FIND) {
                foundValidSeed = true;
                pthread_mutex_unlock(&seedMutex);
                break;
            }
            pthread_mutex_unlock(&seedMutex);
        }
    }
    return NULL;
}

// -----------------------------------------------------------------------------
// Simple helper to skip leading/trailing spaces
static void trim(char *str)
{
    char *p = str;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (p != str) memmove(str, p, strlen(p)+1);
    int len = (int)strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || str[len-1] == '\r' || str[len-1] == '\n')) {
        str[len-1] = '\0';
        len--;
    }
}

// -----------------------------------------------------------------------------
// Parse the "next to biome:" parameter which can contain multiple biome IDs separated by "or"
int parseProximityBiomes(char *biomeStr, int **biomesArray) 
{
    if (strstr(biomeStr, "-1") == biomeStr)
        return 0;
    int biomeCount = 1;
    char *p = biomeStr;
    while ((p = strstr(p, " or ")) != NULL) {
        biomeCount++;
        p += 4;
    }
    *biomesArray = malloc(biomeCount * sizeof(int));
    if (!*biomesArray) {
        perror("malloc for proximity biomes");
        exit(1);
    }
    char *biomeList = strdup(biomeStr);
    char *token = strtok(biomeList, " or ");
    int i = 0;
    while (token != NULL && i < biomeCount) {
        (*biomesArray)[i++] = atoi(token);
        token = strtok(NULL, " or ");
    }
    free(biomeList);
    return biomeCount;
}

// -----------------------------------------------------------------------------
// Parsing function to read the config from lines.
void parseConfigLine(const char *section, char *line)
{
    if (strcmp(section, "===== Scanning options =====") == 0) {
        if (strstr(line, "Starting seed:") == line)
            sscanf(line, "Starting seed: %" SCNu64, &starting_seed);
        else if (strstr(line, "Search radius:") == line)
            sscanf(line, "Search radius: %d", &searchRadius);
        else if (strstr(line, "Use spawn:") == line) {
            char val[16];
            if (sscanf(line, "Use spawn: %15s", val) == 1) {
                if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0)
                    useSpawn = 1;
                else
                    useSpawn = 0;
            }
        }
        else if (strstr(line, "Custom x:") == line)
            sscanf(line, "Custom x: %d", &customX);
        else if (strstr(line, "Custom z:") == line)
            sscanf(line, "Custom z: %d", &customZ);
    }
}

static char currentSection[128] = {0};

void parseParameterLine(char *line)
{
    trim(line);
    if (line[0] == '\0')
        return;
    if (strstr(line, "=====") == line) {
        strcpy(currentSection, line);
        return;
    }
    if (strcmp(currentSection, "===== Scanning options =====") == 0)
        parseConfigLine(currentSection, line);
    else if (strcmp(currentSection, "===== Required structures =====") == 0) {
        int idx, structureType, minCount, minH, maxH, biome, minSz, maxSz, biomeProx;
        char *openParen = strchr(line, '(');
        if (!openParen) return;
        sscanf(line, "%d. %d", &idx, &structureType);
        char parenPart[512];
        strncpy(parenPart, openParen + 1, sizeof(parenPart) - 1);
        parenPart[sizeof(parenPart) - 1] = '\0';
        char *closeParen = strrchr(parenPart, ')');
        if (closeParen) *closeParen = '\0';
        char *params = parenPart;
        char *end;
        if ((end = strstr(params, ", next to biome:")) != NULL) {
            sscanf(params, "min amount: %d", &minCount);
            params = end + 2;
        } else {
            sscanf(params, "min amount: %d", &minCount);
        }
        char proximityBiomeStr[256] = "-1";
        biomeProx = -1;
        char *nextToBiome = strstr(parenPart, "next to biome:");
        char *biomeProxPtr = strstr(parenPart, "biome proximity:");
        if (nextToBiome && biomeProxPtr) {
            char *start = nextToBiome + strlen("next to biome:");
            char *end = biomeProxPtr - 2;
            size_t len = end - start;
            if (len < sizeof(proximityBiomeStr)) {
                strncpy(proximityBiomeStr, start, len);
                proximityBiomeStr[len] = '\0';
                trim(proximityBiomeStr);
            }
            sscanf(biomeProxPtr, "biome proximity: %d", &biomeProx);
        }
        char *minHeightPtr = strstr(parenPart, "min height:");
        char *maxHeightPtr = strstr(parenPart, "max height:");
        if (minHeightPtr && maxHeightPtr) {
            sscanf(minHeightPtr, "min height: %d", &minH);
            sscanf(maxHeightPtr, "max height: %d", &maxH);
        } else {
            minH = -9999;
            maxH = 9999;
        }
        char *biomePtr = strstr(parenPart, "biome:");
        char *minSizePtr = strstr(parenPart, "min size:");
        char *maxSizePtr = strstr(parenPart, "max size:");
        if (biomePtr && minSizePtr && maxSizePtr) {
            sscanf(biomePtr, "biome: %d", &biome);
            sscanf(minSizePtr, "min size: %d", &minSz);
            sscanf(maxSizePtr, "max size: %d", &maxSz);
        } else {
            biome = -1;
            minSz = -1;
            maxSz = -1;
        }
        int skipSurfaceHeight = (structureType == 19 || structureType == 17 ||
                                  structureType == 15 || structureType == 14 ||
                                  structureType == 11);
        if (structureType < 0 && structureType != STRUCTURE_TYPE_SPAWN) {
            fprintf(stderr, "Warning: Invalid structure type %d\n", structureType);
            return;
        }
        structureRequirements = realloc(structureRequirements,
                                      (NUM_STRUCTURE_REQUIREMENTS + 1) * sizeof(StructureRequirement));
        if (!structureRequirements) {
            fprintf(stderr, "Failed to allocate memory for structure requirement\n");
            return;
        }
        int *proximityBiomes = NULL;
        int proximityBiomeCount = parseProximityBiomes(proximityBiomeStr, &proximityBiomes);
        structureRequirements[NUM_STRUCTURE_REQUIREMENTS].structureType = structureType;
        structureRequirements[NUM_STRUCTURE_REQUIREMENTS].minCount = minCount;
        structureRequirements[NUM_STRUCTURE_REQUIREMENTS].minHeight = minH;
        structureRequirements[NUM_STRUCTURE_REQUIREMENTS].maxHeight = maxH;
        structureRequirements[NUM_STRUCTURE_REQUIREMENTS].requiredBiome = biome;
        structureRequirements[NUM_STRUCTURE_REQUIREMENTS].minBiomeSize = minSz;
        structureRequirements[NUM_STRUCTURE_REQUIREMENTS].maxBiomeSize = maxSz;
        structureRequirements[NUM_STRUCTURE_REQUIREMENTS].proximityBiomes = proximityBiomes;
        structureRequirements[NUM_STRUCTURE_REQUIREMENTS].proximityBiomeCount = proximityBiomeCount;
        structureRequirements[NUM_STRUCTURE_REQUIREMENTS].biomeProximity = biomeProx;
        NUM_STRUCTURE_REQUIREMENTS++;
    }
    else if (strcmp(currentSection, "===== Structure Clusters =====") == 0) {
        if (strstr(line, "Enabled:") == line) {
            char val[16];
            if (sscanf(line, "Enabled: %15s", val) == 1) {
                if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0)
                    clusterReq.enabled = true;
                else
                    clusterReq.enabled = false;
            }
        }
        else if (strstr(line, "Valid biomes:") == line) {
            const char *p = strchr(line, ':');
            if (p) {
                p++;
                char list[256];
                strcpy(list, p);
                trim(list);
                char *tok = strtok(list, ",");
                while (tok) {
                    int stype = atoi(tok);
                    clusterReq.structureTypes = realloc(clusterReq.structureTypes, (clusterReq.count+1)*sizeof(int));
                    clusterReq.structureTypes[clusterReq.count] = stype;
                    clusterReq.count++;
                    tok = strtok(NULL, ",");
                }
            }
        }
        else if (strstr(line, "Min cluster distance") == line) {
            int dist;
            if (sscanf(line, "Min cluster distance = %d", &dist) == 1)
                clusterReq.clusterDistance = dist;
        }
        else if (strstr(line, "Min cluster size") == line) {
            int sz;
            if (sscanf(line, "Min cluster size = %d", &sz) == 1)
                clusterReq.minClusterSize = sz;
        }
    }
    else if (strcmp(currentSection, "===== Invalid Structure Clusters =====") == 0) {
        char *dot = strchr(line, '.');
        if (!dot) return;
        dot++;
        trim(dot);
        int tmpArr[64];
        int tmpCount = 0;
        char *tok = strtok(dot, ",");
        while (tok) {
            tmpArr[tmpCount++] = atoi(tok);
            tok = strtok(NULL, ",");
        }
        invalidCombinations = realloc(invalidCombinations, (numInvalidCombinations+1)*sizeof(InvalidCombination));
        invalidCombinations[numInvalidCombinations].count = tmpCount;
        invalidCombinations[numInvalidCombinations].types = malloc(tmpCount*sizeof(int));
        for (int i = 0; i < tmpCount; i++)
            invalidCombinations[numInvalidCombinations].types[i] = tmpArr[i];
        numInvalidCombinations++;
    }
    else if (strcmp(currentSection, "===== Required Biomes =====") == 0) {
        static BiomeSizeConfig *tmpConfigs = NULL;
        static int tmpConfigsCount = 0;
        int idx, biomeId, minSz, maxSz;
        char *paren = strchr(line, '(');
        if (!paren) return;
        int consumed = 0;
        sscanf(line, "%d. %d%n", &idx, &biomeId, &consumed);
        char parenPart[256];
        strcpy(parenPart, paren+1);
        char *endParen = strrchr(parenPart, ')');
        if (endParen) *endParen = '\0';
        sscanf(parenPart, "min size: %d, max size: %d", &minSz, &maxSz);
        tmpConfigs = realloc(tmpConfigs, (tmpConfigsCount+1)*sizeof(BiomeSizeConfig));
        tmpConfigs[tmpConfigsCount].biomeId = biomeId;
        tmpConfigs[tmpConfigsCount].minSize = minSz;
        tmpConfigs[tmpConfigsCount].maxSize = maxSz;
        tmpConfigsCount++;
        if (g_requiredBiomesCount == 0) {
            g_requiredBiomes = malloc(sizeof(BiomeRequirement));
            g_requiredBiomesCount = 1;
            g_requiredBiomes[0].biomeIds     = NULL;
            g_requiredBiomes[0].biomeCount   = 0;
            g_requiredBiomes[0].sizeConfigs  = NULL;
            g_requiredBiomes[0].configCount  = 0;
            g_requiredBiomes[0].logCenters   = 1;
        }
        BiomeRequirement *br = &g_requiredBiomes[0];
        br->biomeIds = realloc(br->biomeIds, (br->biomeCount+1)*sizeof(int));
        br->biomeIds[br->biomeCount] = biomeId;
        br->biomeCount++;
        br->sizeConfigs = realloc(br->sizeConfigs, tmpConfigsCount*sizeof(BiomeSizeConfig));
        memcpy(br->sizeConfigs, tmpConfigs, tmpConfigsCount*sizeof(BiomeSizeConfig));
        br->configCount = tmpConfigsCount;
    }
    else if (strcmp(currentSection, "===== Clustered Biomes =====") == 0) {
        int idx, minSz, maxSz;
        int b1, b2;
        char *paren = strchr(line, '(');
        if (!paren) return;
        char prefix[128];
        strncpy(prefix, line, paren - line);
        prefix[paren - line] = '\0';
        trim(prefix);
        sscanf(prefix, "%d. %d, %d", &idx, &b1, &b2);
        char parenPart[128];
        strcpy(parenPart, paren+1);
        char *endParen = strrchr(parenPart, ')');
        if (endParen) *endParen = '\0';
        trim(parenPart);
        sscanf(parenPart, "min size: %d, max size: %d", &minSz, &maxSz);
        g_biomeClusters = realloc(g_biomeClusters, (g_biomeClustersCount+1)*sizeof(BiomeCluster));
        BiomeCluster *bc = &g_biomeClusters[g_biomeClustersCount];
        g_biomeClustersCount++;
        bc->biomeIds = malloc(2*sizeof(int));
        bc->biomeIds[0] = b1;
        bc->biomeIds[1] = b2;
        bc->biomeCount  = 2;
        bc->minSize     = minSz;
        bc->maxSize     = maxSz;
        bc->logCenters  = 1;
    }
}

// -----------------------------------------------------------------------------
// A function to read from a file or from stdin, building our config
void parseParameterStream(FILE *fp)
{
    char buf[512];
    while (fgets(buf, sizeof(buf), fp))
        parseParameterLine(buf);
}

// -----------------------------------------------------------------------------
// MAIN
int main(int argc, char *argv[])
{
    printf("=== Parameter-Based Scanning ===\n");
    FILE *fp = NULL;
    if (argc > 1) {
        fp = fopen(argv[1], "r");
        if (!fp)
            fprintf(stderr, "Could not open file '%s'. Using stdin instead.\n", argv[1]);
    }
    if (!fp)
        fp = stdin;
    parseParameterStream(fp);
    if (fp != stdin) fclose(fp);
    biomeSearch.required      = g_requiredBiomes;
    biomeSearch.requiredCount = g_requiredBiomesCount;
    biomeSearch.clusters      = g_biomeClusters;
    biomeSearch.clusterCount  = g_biomeClustersCount;
    uint64_t scanCount = 999999999999;
    end_seed = starting_seed + scanCount - 1ULL;
    currentSeed     = starting_seed;
    foundValidSeed  = false;
    seedsFound      = 0;
    pthread_t *threads = malloc(tasksCount * sizeof(pthread_t));
    for (int i = 0; i < tasksCount; i++)
        pthread_create(&threads[i], NULL, scanTask, &end_seed);
    for (int i = 0; i < tasksCount; i++)
        pthread_join(threads[i], NULL);
    free(threads);
    if (foundValidSeed)
        printf("== Found at least one valid seed (e.g., %llu). Seeds found: %d ==\n",
               (unsigned long long) validSeed, seedsFound);
    else
        printf("Finished searching, no valid seeds found in [%llu..%llu].\n",
               (unsigned long long)starting_seed, (unsigned long long)end_seed);
    for (int i = 0; i < NUM_STRUCTURE_REQUIREMENTS; i++)
        free(structureRequirements[i].proximityBiomes);
    free(structureRequirements);
    for (int i = 0; i < numInvalidCombinations; i++)
        free(invalidCombinations[i].types);
    free(invalidCombinations);
    if (g_requiredBiomesCount > 0) {
        for (int i = 0; i < g_requiredBiomesCount; i++) {
            free(g_requiredBiomes[i].biomeIds);
            free(g_requiredBiomes[i].sizeConfigs);
        }
        free(g_requiredBiomes);
    }
    if (g_biomeClustersCount > 0) {
        for (int i = 0; i < g_biomeClustersCount; i++)
            free(g_biomeClusters[i].biomeIds);
        free(g_biomeClusters);
    }
    free(clusterReq.structureTypes);
    return 0;
}