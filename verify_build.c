// Simple verification program for cubiomes library
#include "generator.h"
#include <stdio.h>

int main()
{
    // Set up a basic generator test
    Generator g;
    setupGenerator(&g, MC_1_18, 0);
    
    // Test with a known seed
    uint64_t seed = 12345;
    applySeed(&g, DIM_OVERWORLD, seed);
    
    // Get biome at origin
    int scale = 1;
    int x = 0, y = 63, z = 0;
    int biomeID = getBiomeAt(&g, scale, x, y, z);
    
    printf("Biome ID at origin: %d\n", biomeID);
    
    return 0;
}
