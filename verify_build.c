// Simple verification program for cubiomes library
#include "generator.h"
#include "biomes.h"
#include <stdio.h>

int main()
{
    // Set up a basic generator test
    Generator g;
    setupGenerator(&g, MC_1_18, 0);

    // Test with a known seed
    uint64_t seed = 12345;
    applySeed(&g, DIM_OVERWORLD, seed);

    // Get biome at origin and nearby points
    int scale = 1;
    int y = 63;

    // Check origin
    int x = 0, z = 0;
    int biomeID = getBiomeAt(&g, scale, x, y, z);
    printf("At (0,0): Biome ID %d", biomeID);

    // Print some known biome IDs for reference
    printf("\nSome reference biome IDs:\n");
    printf("plains: %d\n", plains);
    printf("forest: %d\n", forest);
    printf("desert: %d\n", desert);
    printf("ocean: %d\n", ocean);

    // Check a few nearby locations
    printf("\nNearby biomes:\n");
    for(int dx = -1; dx <= 1; dx++) {
        for(int dz = -1; dz <= 1; dz++) {
            if(dx == 0 && dz == 0) continue;
            biomeID = getBiomeAt(&g, scale, dx*100, y, dz*100);
            printf("At (%d,%d): Biome ID %d\n", dx*100, dz*100, biomeID);
        }
    }

    return 0;
}