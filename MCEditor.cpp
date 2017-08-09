#include "MCEditor.h"
#include <vector>
#include <algorithm>
#include <queue>
#include "globals.h"
using namespace std;

inline bool in_region(int x, int z, int y,
		      int x1, int z1, int y1,
		      int x2, int z2, int y2)
{
    return x1 <= x && x < x2 && z1 <= z && z < z2 && y1 <= y && y < y2;
}

void MCEditor::setRegion(int A[512][512][256],
			 int B[512][512][256],
			 int xl, int zl, int yl,
			 int x0, int z0, int y0)
{
    x_len = xl, y_len = yl, z_len = zl;
    x_ori = x0, z_ori = z0, y_ori = y0;
    
    initBlocks(A, B);
    
    computeBlockLight();
    
    computeSkyLight();
    
    updateMCA();
}

void MCEditor::initBlocks(int A[512][512][256], int B[512][512][256])
{
    int d = 30;
    for (int i = 0; i < x_len; i++)
	for (int j = 0; j < z_len; j++)
	    for (int k = 0; k < y_len; k++)
	    {
		blocks[d + i][d + j][y_ori + k] = A[i][j][k];
		blockdata[d + i][d + j][y_ori + k] = B[i][j][k];
	    }
    
    int x0 = x_ori - 30, x1 = x_ori + x_len + 30,
	z0 = z_ori - 30, z1 = z_ori + z_len + 30,
	y0 = 0, y1 = 256;
    //coordinates corresponding to the "lower" and "upper" corner of
    //the whole region stored in array;

    vector<Pos> V;
    for (int x = x0; x < x1; x++)
	for (int z = z0; z < z1; z++)
	    for (int y = y0; y < y1; y++)
	    {
		if (in_region(x, z, y,
			      x_ori, z_ori, y_ori,
			      x_ori + x_len, z_ori + z_len, y_ori + y_len))
		    continue;
		//leave edited blocks unchanged;
		V.push_back(Pos(x, z, y));
	    }
    sort(V.begin(), V.end());
    
    for (Pos u : V)
    {
	int i = u.x - x0, j = u.z - z0, k = u.y - y0;
	BlockInfo res = mca_coder.getBlock(u.x, u.z, u.y);;
	
	blocks[i][j][k] = res.id + (res.add << 8);
	blockdata[i][j][k] = res.data;
	blocklight[i][j][k] = res.block_light;
	skylight[i][j][k] = res.sky_light;
    }
}

void MCEditor::computeBlockLight()
{
	fprintf(stderr, "Computing Block Light...\n");
    for (int x = 15; x < x_len + 45; x++)
	for (int z = 15; z < z_len + 45; z++)
	    for (int y = 0; y < 256; y++)
		blocklight[x][z][y] = get_block_light(blocks[x][z][y]);

    lightPropagate(blocklight);
}

void MCEditor::computeSkyLight()
{
	fprintf(stderr, "Computing Sky Light...\n");
    for (int x = 15; x < x_len + 45; x++)
	for (int z = 15; z < z_len + 45; z++)
	    for (int y = 0; y < 256; y++)
		skylight[x][z][y] = 0;

    for (int x = 15; x < x_len + 45; x++)
	for (int z = 15; z < z_len + 45; z++)
	    for (int y = 255; y >= 0; y--)
		if (!blocks[x][z][y])
		    skylight[x][z][y] = 15;
		else break;

    lightPropagate(skylight);
}

void MCEditor::updateMCA()
{
    vector<Block> V;
    int x0 = x_ori - 30, z0 = z_ori - 30;
    
    for (int x = 15; x < x_len + 45; x++)
	for (int z = 15; z < z_len + 45; z++)
	    for (int y = 0; y < 256; y++)
	    {
		Pos position = Pos(x + x0, z + z0, y);
		ui id = blocks[x][z][y] & 0xFF;
		ui add = (blocks[x][z][y] >> 8) & 0xF;
		ui data = blockdata[x][z][y];
		BlockInfo info = BlockInfo(id, add, data,
					   blocklight[x][z][y], skylight[x][z][y]);
		V.push_back(Block(position, info));
	    }
    
    sort(V.begin(), V.end());

    for (Block u : V)
	mca_coder.setBlock(u.position.x, u.position.z, 
			   u.position.y, u.info);
    mca_coder.saveModification();
}

void MCEditor::lightPropagate(ui light[512][512][256])
{
	fprintf(stderr, "Propagating Light...\n");
    queue<Pos> Q;
    
    for (int x = 0; x < x_len + 60; x++)
	for (int z = 0; z < z_len + 60; z++)
	    for (int y = 0; y < 256; y++)
		if (light[x][z][y])
		{
		    Q.push(Pos(x, z, y));
		    while (!Q.empty())
		    {
			Pos u = Q.front(); Q.pop();
			for (int d = 0; d < 6; d++)
			{
			    int vx = u.x + DX[d],
				vz = u.z + DZ[d],
				vy = u.y + DY[d];
			    if (!in_region(vx, vz, vy, 0, 0, 0,
					  x_len + 60, z_len + 60, 256))
				continue;

			    int dec = get_opacity(blocks[vx][vz][vy]) + 1;
			    int vlight = (int)light[u.x][u.z][u.y] - dec;
				if (vlight <= 0) continue;

			    if (light[vx][vz][vy] < vlight)
			    {
				light[vx][vz][vy] = vlight;
				Q.push(Pos(vx, vz, vy));
			    }
			}
		    }
		}
	fprintf(stderr, "Finished Propagating Light.\n");
}
