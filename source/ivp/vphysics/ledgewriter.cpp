//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: low-level code to write IVP_Compact_Ledge/IVP_Compact_Triangle.
//			also includes code to pack/unpack outer hull ledges to 8-bit rep
//
//=============================================================================
#include "cbase.h"
#include "convert.h"

#include <ivp_surface_manager.hxx>
#include <ivp_surman_polygon.hxx>
#include <ivp_template_surbuild.hxx>
#include <ivp_compact_surface.hxx>
#include <ivp_compact_ledge.hxx>

#include "tier1/utlbuffer.h"
#include "ledgewriter.h"

// gets the max vertex index referenced by a compact ledge
static int MaxLedgeVertIndex( const IVP_Compact_Ledge *pLedge )
{
	int maxIndex = -1;
	for ( int i = 0; i < pLedge->get_n_triangles(); i++ )
	{
		const IVP_Compact_Triangle *pTri = pLedge->get_first_triangle() + i;
		for ( int j = 0; j < 3; j++ )
		{
			int ivpIndex = pTri->get_edge(j)->get_start_point_index();
			maxIndex = MAX(maxIndex, ivpIndex);
		}
	}
	return maxIndex;
}


struct vertmap_t
{

	CUtlVector<int> map;
	int minRef;
	int maxRef;
};

// searches pVerts for each vert used by pLedge and builds a one way map from ledge indices to pVerts indices
// NOTE: pVerts is in HL coords, pLedge is in IVP coords
static void BuildVertMap( vertmap_t &out, const Vector *pVerts, int vertexCount, const IVP_Compact_Ledge *pLedge )
{
	out.map.EnsureCount(MaxLedgeVertIndex(pLedge)+1);
	for ( auto &m : out.map )
	{
		m = -1;
	}
	out.minRef = vertexCount;
	out.maxRef = 0;
	const IVP_Compact_Poly_Point *pVertList = pLedge->get_point_array();
	for ( int i = 0; i < pLedge->get_n_triangles(); i++ )
	{
		// iterate each triangle, for each referenced vert that hasn't yet been mapped, search for the nearest match
		const IVP_Compact_Triangle *pTri = pLedge->get_first_triangle() + i;
		for ( int j = 0; j < 3; j++ )
		{
			int ivpIndex = pTri->get_edge(j)->get_start_point_index();
			if ( out.map[ivpIndex] < 0 )
			{
				int index = -1;
				Vector tmp;
				ConvertPositionToHL( &pVertList[ivpIndex], tmp);
				float minDist = 1e16f;
				for ( int k = 0; k < vertexCount; k++ )
				{
					float dist = (tmp-pVerts[k]).Length();
					if ( dist < minDist )
					{
						index = k;
						minDist = dist;
					}
				}
				Assert(minDist<0.1f);
				out.map[ivpIndex] = index;
				out.minRef = MIN(out.minRef, index);
				out.maxRef = MAX(out.maxRef, index);
			}
		}
	}
}


// Each IVP_Compact_Triangle and IVP_Compact_Edge occupies an index 
// 0,1,2,3 is tri, edge, edge, edge (tris and edges are both 16 bytes)
// So you can just add the index to get_first_triangle to get a pointer 
inline intp EdgeIndex( const IVP_Compact_Ledge *pLedge, const IVP_Compact_Edge *pEdge )
{
	return pEdge - (const IVP_Compact_Edge *)pLedge->get_first_triangle();
}

// Builds a packedhull_t from a IVP_Compact_Ledge.  Assumes that the utlbuffer points at the memory following pHull (pHull is the header, utlbuffer is the body)
void PackLedgeIntoBuffer( packedhull_t *pHull, CUtlBuffer &buf, const IVP_Compact_Ledge *pLedge, const virtualmeshlist_t &list )
{
	if ( !pLedge )
		return;

	// The lists store the ivp index of each element to be written out
	// The maps store the output packed index for each ivp index
	CUtlVector<intp> triangleList, triangleMap;
	CUtlVector<intp> edgeList, edgeMap;
	vertmap_t vertMap;
	BuildVertMap( vertMap, list.pVerts, list.vertexCount, pLedge );
	pHull->baseVert = vertMap.minRef;
	// clear the maps
	triangleMap.EnsureCount(pLedge->get_n_triangles());
	for ( auto& t : triangleMap )
	{
		t = -1;
	}
	edgeMap.EnsureCount(pLedge->get_n_triangles()*4);	// each triangle also occupies an edge index
	for ( auto& e : edgeMap )
	{
		e = -1;
	}

	// we're going to reorder the triangles and edges so that the ones marked virtual 
	// appear first in the list.  This way we only need a virtual count, not a per-item
	// flag.

	// also, the edges are stored relative to the first triangle that references them
	// so an edge from 0->1 means that the first triangle that references the edge is 0->1 and the 
	// second triangle is 1->0.  This way we store half the edges and the winged edge pointers are implicit

	// sort triangles in two passes
	for ( int i = 0; i < pLedge->get_n_triangles(); i++ )
	{
		const IVP_Compact_Triangle *pTri = pLedge->get_first_triangle() + i;
		if ( pTri->get_is_virtual() )
		{
			triangleMap[i] = triangleList.AddToTail(i);
		}
	}
	pHull->vtriCount = triangleList.Count();
	for ( int i = 0; i < pLedge->get_n_triangles(); i++ )
	{
		const IVP_Compact_Triangle *pTri = pLedge->get_first_triangle() + i;
		if ( !pTri->get_is_virtual() )
		{
			triangleMap[i] = triangleList.AddToTail(i);
		}
	}
	// sort edges in two passes
	for ( int i = 0; i < pLedge->get_n_triangles(); i++ )
	{
		const IVP_Compact_Triangle *pTri = pLedge->get_first_triangle() + triangleList[i];
		for ( int j = 0; j < 3; j++ )
		{
			const IVP_Compact_Edge *pEdge = pTri->get_edge(j);
			if ( pEdge->get_is_virtual() && edgeMap[EdgeIndex(pLedge, pEdge->get_opposite())] < 0 )
			{
				edgeMap[EdgeIndex(pLedge, pEdge)] = edgeList.AddToTail(EdgeIndex(pLedge, pEdge));
			}
		}
	}
	pHull->vedgeCount = edgeList.Count();

	for ( int i = 0; i < pLedge->get_n_triangles(); i++ )
	{
		const IVP_Compact_Triangle *pTri = pLedge->get_first_triangle() + triangleList[i];
		for ( int j = 0; j < 3; j++ )
		{
			const IVP_Compact_Edge *pEdge = pTri->get_edge(j);
			intp index = EdgeIndex(pLedge, pEdge);
			intp oppositeIndex = EdgeIndex(pLedge, pEdge->get_opposite());
			if ( !pEdge->get_is_virtual() && edgeMap[oppositeIndex] < 0 )
			{
				edgeMap[index] = edgeList.AddToTail(index);
			}
			if ( edgeMap[index] < 0 )
			{
				Assert(edgeMap[oppositeIndex] >= 0);
				edgeMap[index] = edgeMap[oppositeIndex];
			}
		}
	}
	Assert( edgeList.Count() == pHull->edgeCount );

	// now write the packed triangles
	for ( int i = 0; i < pHull->triangleCount; i++ )
	{
		packedtriangle_t tri;
		const IVP_Compact_Triangle *pTri = pLedge->get_first_triangle() + triangleList[i];
		const IVP_Compact_Edge *pEdge;
		pEdge = pTri->get_edge(0);
		tri.opposite = triangleMap[pTri->get_pierce_index()];
		Assert(tri.opposite<pHull->triangleCount);
		tri.e0 = edgeMap[EdgeIndex(pLedge, pEdge)];
		pEdge = pTri->get_edge(1);
		tri.e1 = edgeMap[EdgeIndex(pLedge, pEdge)];
		pEdge = pTri->get_edge(2);
		tri.e2 = edgeMap[EdgeIndex(pLedge, pEdge)];
		Assert(tri.e0<pHull->edgeCount);
		Assert(tri.e1<pHull->edgeCount);
		Assert(tri.e2<pHull->edgeCount);
		buf.Put(&tri, sizeof(tri));
	}
	// now write the packed edges
	for ( int i = 0; i < pHull->edgeCount; i++ )
	{
		packededge_t edge;
		const IVP_Compact_Edge *pEdge = (const IVP_Compact_Edge *)pLedge->get_first_triangle() + edgeList[i];
		Assert((edgeList[i]&3) != 0); // must not be a triangle

		int v0 = vertMap.map[pEdge->get_start_point_index()] - pHull->baseVert;
		int v1 = vertMap.map[pEdge->get_next()->get_start_point_index()] - pHull->baseVert;
		Assert(v0>=0 && v0<256);
		Assert(v1>=0 && v1<256);
		edge.v0 = v0;
		edge.v1 = v1;
		buf.Put(&edge, sizeof(edge));
	}
}


// decompress packed hull into a compact ledge
void CVPhysicsVirtualMeshWriter::UnpackCompactLedgeFromHull( IVP_Compact_Ledge *pLedge, int materialIndex, const IVP_Compact_Poly_Point *pPointList, const virtualmeshhull_t *pHullHeader, int hullIndex, bool isVirtualLedge )
{
	const packedhull_t *pHull = pHullHeader->GetPackedHull(hullIndex);
	const packedtriangle_t *pPackedTris = pHullHeader->GetPackedTriangles(hullIndex);
	// write the ledge
	pLedge->set_offset_ledge_points( (int)((const char *)pPointList - (char *)pLedge) ); // byte offset from 'this' to (ledge) point array
	pLedge->set_is_compact( IVP_TRUE );
	pLedge->set_size(sizeof(IVP_Compact_Ledge) + sizeof(IVP_Compact_Triangle)*pHull->triangleCount);	// <0 indicates a non compact compact ledge 
	pLedge->n_triangles = pHull->triangleCount;
	pLedge->has_chilren_flag = static_cast<unsigned>(isVirtualLedge ? IVP_TRUE : IVP_FALSE);

	// Make the offset -pLedge so the result is a NULL ledgetree node - we haven't needed to create one of these as of yet
	//pLedge->ledgetree_node_offset = -((intp)pLedge);

	// keep track of which triangle edge referenced this edge (so the next one can swap the order and point to the first one)
	intp forwardEdgeIndex[255] = {};
	for ( int i = 0; i < pHull->edgeCount; i++ )
	{
		forwardEdgeIndex[i] = -1;
	}
	const packededge_t *pPackedEdges = (const packededge_t *)(pPackedTris + pHull->triangleCount);
	IVP_Compact_Triangle *pOut = pLedge->get_first_triangle();
	// now write the compact triangles and their edges
	int baseVert = pHull->baseVert;
	for ( int i = 0; i < pHull->triangleCount; i++ )
	{
		pOut[i].set_tri_index(i);
		pOut[i].set_material_index(materialIndex);
		pOut[i].set_is_virtual( static_cast<unsigned>(i < pHull->vtriCount ? IVP_TRUE : IVP_FALSE) );
		pOut[i].set_pierce_index(pPackedTris[i].opposite);
		Assert(pPackedTris[i].opposite<pHull->triangleCount);
		int edges[3] = {pPackedTris[i].e0, pPackedTris[i].e1, pPackedTris[i].e2};
		for ( int j = 0; j < 3; j++ )
		{
			Assert(edges[j]<pHull->edgeCount);
			if ( forwardEdgeIndex[edges[j]] < 0 )
			{
				// this is the first triangle to use this edge, so it's forward (and the other triangle sharing (opposite edge pointer) is unknown)
				int startVert = pPackedEdges[edges[j]].v0 + baseVert;
				pOut[i].c_three_edges[j].set_start_point_index(startVert);
				pOut[i].c_three_edges[j].set_is_virtual( static_cast<unsigned>(edges[j] < pHull->vedgeCount ? IVP_TRUE : IVP_FALSE) );
				forwardEdgeIndex[edges[j]] = EdgeIndex(pLedge, &pOut[i].c_three_edges[j]);
			}
			else
			{
				// this is the second triangle to use this edge, so it's reversed (and the other triangle sharing is in the forward edge table)
				intp oppositeIndex = forwardEdgeIndex[edges[j]];

				int startVert = pPackedEdges[edges[j]].v1 + baseVert;
				pOut[i].c_three_edges[j].set_start_point_index(startVert);
				pOut[i].c_three_edges[j].set_is_virtual( static_cast<unsigned>(edges[j] < pHull->vedgeCount ? IVP_TRUE : IVP_FALSE) );
				// now build the links between the triangles sharing this edge
				intp thisEdgeIndex = EdgeIndex( pLedge, &pOut[i].c_three_edges[j] );
				pOut[i].c_three_edges[j].set_opposite_index( oppositeIndex - thisEdgeIndex );
				pOut[i].c_three_edges[j].get_opposite()->set_opposite_index( thisEdgeIndex - oppositeIndex );
			}
		}
	}
}

// low-level code to initialize a 2-sided triangle
static void InitTriangle( IVP_Compact_Triangle *pTri, int index, int materialIndex, int v0, int v1, int v2, int opp0, int opp1, int opp2 )
{
	pTri->set_tri_index(index);
	pTri->set_material_index(materialIndex);

	pTri->c_three_edges[0].set_start_point_index(v0);
	pTri->c_three_edges[1].set_start_point_index(v1);
	pTri->c_three_edges[2].set_start_point_index(v2);

	pTri->c_three_edges[0].set_opposite_index(opp0);
	pTri->c_three_edges[1].set_opposite_index(opp1);
	pTri->c_three_edges[2].set_opposite_index(opp2);
}

void CVPhysicsVirtualMeshWriter::InitTwoSidedTriangleLege( triangleledge_t *pOut, const IVP_Compact_Poly_Point *pPoints, int v0, int v1, int v2, int materialIndex )
{
	IVP_Compact_Ledge *pLedge = &pOut->ledge;
	pLedge->set_offset_ledge_points( (int)((const char *)pPoints - (char *)pLedge) ); // byte offset from 'this' to (ledge) point array
	pLedge->set_is_compact( IVP_TRUE );
	pLedge->set_size(sizeof(triangleledge_t));	// <0 indicates a non compact compact ledge 
	pLedge->n_triangles = 2;
	pLedge->has_chilren_flag = IVP_FALSE;
	// triangles
	InitTriangle( &pOut->faces[0], 0, materialIndex, v0, v1, v2, 6, 4, 2 );
	InitTriangle( &pOut->faces[1], 1, materialIndex, v0, v2, v1, -2, -4, -6);
	pOut->faces[0].set_pierce_index(1);
	pOut->faces[1].set_pierce_index(0);
}

bool CVPhysicsVirtualMeshWriter::LedgeCanBePacked(const IVP_Compact_Ledge *pLedge, const virtualmeshlist_t &list)
{
	int edgeCount = pLedge->get_n_triangles() * 3;
	if ( edgeCount > 512 )
		return false;
	vertmap_t vertMap;
	BuildVertMap( vertMap, list.pVerts, list.vertexCount, pLedge );
	if ( (vertMap.maxRef - vertMap.minRef) > 255 )
		return false;
	return true;
}

// this builds a packed hull array from a compact ledge array (needs the virtualmeshlist for reference)
virtualmeshhull_t *CVPhysicsVirtualMeshWriter::CreatePackedHullFromLedges( const virtualmeshlist_t &list, const IVP_Compact_Ledge **pLedges, int ledgeCount )
{
	int triCount = 0;
	int edgeCount = 0;
	for ( int i = 0; i < ledgeCount; i++ )
	{
		triCount += pLedges[i]->get_n_triangles();
		edgeCount += (pLedges[i]->get_n_triangles() * 3)/2;
		Assert(LedgeCanBePacked(pLedges[i], list));
	}

	unsigned int totalSize = sizeof(packedtriangle_t)*triCount + sizeof(packededge_t)*edgeCount + sizeof(packedhull_t)*ledgeCount + sizeof(virtualmeshhull_t);
	byte *pBuf = new byte[totalSize];

	CUtlBuffer buf;
	buf.SetExternalBuffer( pBuf, totalSize, 0, 0 );

	if ( 1 )
	{
		virtualmeshhull_t tmp;
		Q_memset( &tmp, 0, sizeof(tmp) );
		tmp.hullCount = ledgeCount;
		buf.Put(&tmp, sizeof(tmp));
	}

	// write the headers
	Assert(ledgeCount < 16);
	packedhull_t *pHulls[16];
	for ( int i = 0; i < ledgeCount; i++ )
	{
		pHulls[i] = (packedhull_t *)buf.PeekPut();
		packedhull_t hull;
		hull.triangleCount = pLedges[i]->get_n_triangles();
		hull.edgeCount = (hull.triangleCount * 3) / 2;
		buf.Put(&hull, sizeof(hull));
	}

	// write the data itself
	for ( int i = 0; i < ledgeCount; i++ )
	{
		PackLedgeIntoBuffer( pHulls[i], buf, pLedges[i], list );
	}

	return (virtualmeshhull_t *)pBuf;
}

// frees the memory associated with this packed hull
void CVPhysicsVirtualMeshWriter::DestroyPackedHull( virtualmeshhull_t *pHull )
{
	byte *pData = (byte *)pHull;
	delete[] pData;
}


unsigned int CVPhysicsVirtualMeshWriter::UnpackLedgeListFromHull( byte *pOut, virtualmeshhull_t *pHull, IVP_Compact_Poly_Point *pPoints )
{
	unsigned int memOffset = 0;
	for ( int i = 0; i < pHull->hullCount; i++ )
	{
		IVP_Compact_Ledge *pHullLedge = (IVP_Compact_Ledge *)(pOut + memOffset);
		CVPhysicsVirtualMeshWriter::UnpackCompactLedgeFromHull( pHullLedge, 0, pPoints, pHull, i, true );
		memOffset += pHullLedge->get_size();
	}
	return memOffset;
}
