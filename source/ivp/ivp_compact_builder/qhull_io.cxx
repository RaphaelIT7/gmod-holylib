/*<html><pre>  -<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="TOP">-</a>

   io.c 
   Input/Output routines of qhull application

   see qh-c.htm and io.h

   see user.c for qh_errprint and qh_printfacetlist

   unix.c calls qh_readpoints and qh_produce_output

   unix.c and user.c are the only callers of io.c functions
   This allows the user to avoid loading io.o from qhull.a

   copyright (c) 1993-1999 The Geometry Center        
*/

#include <ivp_physics.hxx>
#include "qhull_a.hxx"

/*========= -prototypes for internal functions ========= */

int qh_compare_vertexpoint(const void *p1, const void *p2); /* not used */

/*========= -functions in alphabetical order after qh_produce_output()  =====*/

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="produce_output">-</a>
  
  qh_produce_output()
    prints out the result of qhull in desired format
    if qh.GETarea
      computes and prints area and volume
    qh.PRINTout[] is an array of output formats

  notes:
    prints output in qh.PRINTout order
*/
void qh_produce_output(void) {
  int i, tempsize= qh_setsize ((setT*)qhmem.tempstack), d_1;

  if (qh VORONOI) {
    qh_clearcenters (qh_ASvoronoi);
    qh_vertexneighbors();
  }
  if (qh GETarea)
    qh_getarea(qh facet_list);
  qh_findgood_all (qh facet_list); 
  if (qh KEEParea || qh KEEPmerge || qh KEEPminArea < REALmax/2)
    qh_markkeep (qh facet_list);
  if (qh PRINTsummary)
    qh_printsummary(qh ferr);
  else if (qh PRINTout[0] == qh_PRINTnone)
    qh_printsummary(qh fout);
  for (i= 0; i < qh_PRINTEND; i++)
    qh_printfacets (qh fout, qh PRINTout[i], qh facet_list, NULL, !qh_ALL);
  qh_allstatistics();
  if (qh PRINTprecision && !qh MERGING && (qh JOGGLEmax > REALmax/2 || qh RERUN))
    qh_printstats (qh ferr, qhstat precision, NULL);
  if (qh VERIFYoutput && (zzval_(Zridge) > 0 || zzval_(Zridgemid) > 0)) 
    qh_printstats (qh ferr, qhstat vridges, NULL);
  if (qh PRINTstatistics) {
    qh_collectstatistics();
    qh_printstatistics(qh ferr, "");
    qh_memstatistics (qh ferr);
    d_1= sizeof(setT) + (qh hull_dim - 1) * SETelemsize;
    ivp_message( "\
    size in bytes: hashentry %d merge %d ridge %d vertex %d facet %d\n\
         normal %d ridge vertices %d facet vertices or neighbors %d\n",
	    sizeof(hashentryT), sizeof(mergeT), sizeof(ridgeT),
	    sizeof(vertexT), sizeof(facetT),
	    qh normal_size, d_1, d_1 + SETelemsize);
  }
  if (qh_setsize ((setT*)qhmem.tempstack) != tempsize) {
    ivp_message( "qhull internal error (qh_produce_output): temporary sets not empty (%d)\n",
	     qh_setsize ((setT*)qhmem.tempstack));
    qh_errexit (qh_ERRqhull, NULL, NULL);
  }
} /* produce_output */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="dfacet">-</a>
  
  dfacet( id )
    print facet by id, for debugging

*/
void dfacet (unsigned id) {
  facetT *facet;

  FORALLfacets {
    if (facet->id == id) {
      qh_printfacet (qh fout, facet);
      break;
    }
  }
} /* dfacet */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="dvertex">-</a>
  
  dvertex( id )
    print vertex by id, for debugging
*/
void dvertex (unsigned id) {
  vertexT *vertex;

  FORALLvertices {
    if (vertex->id == id) {
      qh_printvertex (qh fout, vertex);
      break;
    }
  }
} /* dvertex */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="compare_vertexpoint">-</a>
  
  qh_compare_vertexpoint( p1, p2 )
    used by qsort() to order vertices by point id 
*/
int qh_compare_vertexpoint(const void *p1, const void *p2) {
  vertexT *a= *((vertexT **)p1), *b= *((vertexT **)p2);
 
  return ((qh_pointid(a->point) > qh_pointid(b->point)?1:-1));
} /* compare_vertexpoint */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="compare_facetarea">-</a>
  
  qh_compare_facetarea( p1, p2 )
    used by qsort() to order facets by area
*/
#ifdef SUN
extern "C" {
#endif
static int qh_compare_facetarea(const void *p1, const void *p2) {
  facetT *a= *((facetT **)p1), *b= *((facetT **)p2);

  if (!a->isarea)
    return -1;
  if (!b->isarea)
    return 1; 
  if (a->f.area > b->f.area)
    return 1;
  else if (hk_Math::almost_equal(a->f.area, b->f.area))
    return 0;
  return -1;
} /* compare_facetarea */
#ifdef SUN
}
#endif

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="compare_facetmerge">-</a>
  
  qh_compare_facetmerge( p1, p2 )
    used by qsort() to order facets by number of merges
*/
#ifdef SUN
extern "C" {
#endif
static int qh_compare_facetmerge(const void *p1, const void *p2) {
  facetT *a= *((facetT **)p1), *b= *((facetT **)p2);
 
  return (a->nummerge - b->nummerge);
} /* compare_facetvisit */
#ifdef SUN
}
#endif

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="compare_facetvisit">-</a>
  
  qh_compare_facetvisit( p1, p2 )
    used by qsort() to order facets by visit id or id
*/
#ifdef SUN
extern "C" {
#endif
static int qh_compare_facetvisit(const void *p1, const void *p2) {
  facetT *a= *((facetT **)p1), *b= *((facetT **)p2);
  int i,j;

  if (!(i= a->visitid))
    i= a->id; /* do not convert to int */
  if (!(j= b->visitid))
    j= b->id;
  return (i - j);
} /* compare_facetvisit */
#ifdef SUN
}
#endif

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="countfacets">-</a>
  
  qh_countfacets( facetlist, facets, printall, 
          numfacets, numsimplicial, totneighbors, numridges, numcoplanar  )
    count good facets for printing and set visitid
    if allfacets, ignores qh_skipfacet()

  returns:
    numfacets, numsimplicial, total neighbors, numridges, coplanars
    each facet with ->visitid indicating 1-relative position
      ->visitid==0 indicates not good
  
  notes
    if qh.NEWfacets, 
      does not count visible facets (matches qh_printafacet)

  design:
    for all facets on facetlist and in facets set
      unless facet is skipped or visible (i.e., will be deleted)
        mark facet->visitid
        update counts
*/
void qh_countfacets (facetT *facetlist, setT *facets, boolT printall,
    int *numfacetsp, int *numsimplicialp, int *totneighborsp, int *numridgesp, int *numcoplanarsp) {
  facetT *facet, **facetp;
  int numfacets= 0, numsimplicial= 0, numridges= 0, totneighbors= 0, numcoplanars= 0;

  FORALLfacet_(facetlist) {
    if ((facet->visible && qh NEWfacets)
    || (!printall && qh_skipfacet(facet)))
      facet->visitid= 0;
    else {
      facet->visitid= ++numfacets;
      totneighbors += qh_setsize (facet->neighbors);
      if (facet->simplicial) 
        numsimplicial++;
      else
        numridges += qh_setsize (facet->ridges);
      if (facet->coplanarset)
        numcoplanars += qh_setsize (facet->coplanarset);
    }
  }
  FOREACHfacet_(facets) {
    if ((facet->visible && qh NEWfacets)
    || (!printall && qh_skipfacet(facet)))
      facet->visitid= 0;
    else {
      facet->visitid= ++numfacets;
      totneighbors += qh_setsize (facet->neighbors);
      if (facet->simplicial)
        numsimplicial++;
      else
        numridges += qh_setsize (facet->ridges);
      if (facet->coplanarset)
        numcoplanars += qh_setsize (facet->coplanarset);
    }
  }
  qh visit_id += numfacets+1;
  *numfacetsp= numfacets;
  *numsimplicialp= numsimplicial;
  *totneighborsp= totneighbors;
  *numridgesp= numridges;
  *numcoplanarsp= numcoplanars;
} /* countfacets */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="detvnorm">-</a>
  
  qh_detvnorm( vertex, vertexA, centers, offset )
    compute separating plane of the Voronoi diagram for a pair of input sites
    centers= set of facets (i.e., Voronoi vertices)
      facet->visitid= 0 iff vertex-at-infinity (i.e., unbounded)
        
  assumes:
    qh_ASvoronoi and qh_vertexneighbors() already set
  
  returns:
    norm
      a pointer into qh.gm_matrix to qh.hull_dim-1 reals
      copy the data before reusing qh.gm_matrix
    offset
      if 'QVn'
        sign adjusted so that qh.GOODvertexp is inside
      else
        sign adjusted so that vertex is inside
      
    qh.gm_matrix= simplex of points from centers relative to first center
    
  notes:
    in io.c so that code for 'v Tv' can be removed by removing io.c
    returns pointer into qh.gm_matrix to avoid tracking of temporary memory
  
  design:
    determine midpoint of input sites
    build points as the set of Voronoi vertices
    select a simplex from points (if necessary)
      include midpoint if the Voronoi region is unbounded
    relocate the first vertex of the simplex to the origin
    compute the normalized hyperplane through the simplex
    orient the hyperplane toward 'QVn' or 'vertex'
    if 'Tv' or 'Ts'
      if bounded
        test that hyperplane is the perpendicular bisector of the input sites
      test that Voronoi vertices not in the simplex are still on the hyperplane
    free up temporary memory
*/
pointT *qh_detvnorm (vertexT *vertex, vertexT *vertexA, setT *centers, realT *offsetp) {
  facetT *facet, **facetp;
  int  i, k, pointid, pointidA, point_i, point_n;
  setT *simplex= NULL;
  pointT *point, **pointp, *point0, *midpoint, *normal, *inpoint;
  coordT *coord, *gmcoord, *normalp;
  setT *points= qh_settemp (qh TEMPsize);
  boolT nearzero= False;
  boolT unbounded= False;
  int numcenters= 0;
  int dim= qh hull_dim - 1;
  realT dist, offset, angle, zero= 0.0;

  midpoint= qh gm_matrix + qh hull_dim * qh hull_dim;  /* last row */
  for (k= 0; k < dim; k++)
    midpoint[k]= (vertex->point[k] + vertexA->point[k])/2;
  FOREACHfacet_(centers) {
    numcenters++;
    if (!facet->visitid)
      unbounded= True;
    else {
      if (!facet->center)
        facet->center= qh_facetcenter (facet->vertices);
      qh_setappend (&points, facet->center);
    }
  }
  if (numcenters > dim) {
    simplex= qh_settemp (qh TEMPsize);
    qh_setappend (&simplex, vertex->point);
    if (unbounded)
      qh_setappend (&simplex, midpoint);
    qh_maxsimplex (dim, points, NULL, 0, &simplex);
    qh_setdelnth (simplex, 0);
  }else if (numcenters == dim) {
    if (unbounded)
      qh_setappend (&points, midpoint);
    simplex= points; 
  }else {
    ivp_message( "qh_detvnorm: too few points (%d) to compute separating plane\n", numcenters);
    qh_errexit (qh_ERRqhull, NULL, NULL);
  }
  i= 0;
  gmcoord= qh gm_matrix;
  point0= SETfirstt_(simplex, pointT);
  FOREACHpoint_(simplex) {
    if (qh IStracing >= 4) //-V112
      qh_printmatrix(qh ferr, "qh_detvnorm: Voronoi vertex or midpoint", 
                              &point, 1, dim);
    if (point != point0) {
      qh gm_row[i++]= gmcoord;
      coord= point0;
      for (k= dim; k--; )
        *(gmcoord++)= *point++ - *coord++;
    }
  }
  qh gm_row[i]= gmcoord;  /* does not overlap midpoint, may be used later for qh_areasimplex */
  normal= gmcoord;
  qh_sethyperplane_gauss (dim, qh gm_row, point0, True,
           	normal, &offset, &nearzero);
  if (qh GOODvertexp == vertexA->point)
    inpoint= vertexA->point;
  else
    inpoint= vertex->point;
  zinc_(Zdistio);
  dist= qh_distnorm (dim, inpoint, normal, &offset);
  if (dist > 0) {
    offset= -offset;
    normalp= normal;
    for (k= dim; k--; ) {
      *normalp= -(*normalp);
      normalp++;
    }
  }
  if (qh VERIFYoutput || qh PRINTstatistics) {
    pointid= qh_pointid (vertex->point);
    pointidA= qh_pointid (vertexA->point);
    if (!unbounded) {
      zinc_(Zdiststat);
      dist= qh_distnorm (dim, midpoint, normal, &offset);
      if (dist < 0)
        dist= -dist;
      zzinc_(Zridgemid);
      wwmax_(Wridgemidmax, dist);
      wwadd_(Wridgemid, dist);
      trace4((qh ferr, "qh_detvnorm: points %d %d midpoint dist %2.2g\n",
                 pointid, pointidA, dist));
      for (k= 0; k < dim; k++) 
        midpoint[k]= vertexA->point[k] - vertex->point[k];  /* overwrites midpoint! */
      qh_normalize (midpoint, dim, False);
      angle= qh_distnorm (dim, midpoint, normal, &zero); /* qh_detangle uses dim+1 */
      if (angle < 0.0)
	angle= angle + 1.0;
      else
	angle= angle - 1.0;
      if (angle < 0.0)
	angle -= angle;
      trace4((qh ferr, "qh_detvnorm: points %d %d angle %2.2g nearzero %d\n",
                 pointid, pointidA, angle, nearzero));
      if (nearzero) {
        zzinc_(Zridge0);
        wwmax_(Wridge0max, angle);
        wwadd_(Wridge0, angle);
      }else {
        zzinc_(Zridgeok);
        wwmax_(Wridgeokmax, angle);
        wwadd_(Wridgeok, angle);
      }
    }
    if (simplex != points) {
      FOREACHpoint_i_(points) {
        if (!qh_setin (simplex, point)) {
          facet= SETelemt_(centers, point_i, facetT);
	  zinc_(Zdiststat);
  	  dist= qh_distnorm (dim, point, normal, &offset);
          if (dist < 0)
            dist= -dist;
	  zzinc_(Zridge);
          wwmax_(Wridgemax, dist);
          wwadd_(Wridge, dist);
          trace4((qh ferr, "qh_detvnorm: points %d %d Voronoi vertex %d dist %2.2g\n",
                             pointid, pointidA, facet->visitid, dist));
        }
      }
    }
  }
  *offsetp= offset;
  if (simplex != points)
    qh_settempfree (&simplex);
  qh_settempfree (&points);
  return normal;
} /* detvnorm */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="detvridge">-</a>

  qh_detvridge( vertexA )
    determine Voronoi ridge from 'seen' neighbors of vertexA
    include one vertex-at-infinite if an !neighbor->visitid

  returns:
    temporary set of centers (facets, i.e., Voronoi vertices)
    sorted by center id
*/
setT *qh_detvridge (vertexT *vertex) {
  setT *centers= qh_settemp (qh TEMPsize);
  facetT *neighbor, **neighborp;
  boolT firstinf= True;
  
  FOREACHneighbor_(vertex) {
    if (neighbor->seen) {
      if (neighbor->visitid)
        qh_setappend (&centers, neighbor);
      else if (firstinf) {
        firstinf= False;
        qh_setappend (&centers, neighbor);
      }
    }
  }
  qsort (SETaddr_(centers, facetT), qh_setsize (centers),
             sizeof (facetT *), qh_compare_facetvisit);
  return centers;
} /* detvridge */      

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="detvridge3">-</a>

  qh_detvridge3( atvertex, vertex )
    determine 3-d Voronoi ridge from 'seen' neighbors of atvertex and vertex
    include one vertex-at-infinite for !neighbor->visitid
    assumes all facet->seen2= True

  returns:
    temporary set of centers (facets, i.e., Voronoi vertices)
    listed in adjacency order (not oriented)
    all facet->seen2= True

  design:
    mark all neighbors of atvertex
    for each adjacent neighbor of both atvertex and vertex
      if neighbor selected
        add neighbor to set of Voronoi vertices
*/
setT *qh_detvridge3 (vertexT *atvertex, vertexT *vertex) {
  setT *centers= qh_settemp (qh TEMPsize);
  facetT *neighbor, **neighborp, *facet= NULL;
  boolT firstinf= True;
  
  FOREACHneighbor_(atvertex)
    neighbor->seen2= False;
  FOREACHneighbor_(vertex) {
    if (!neighbor->seen2) {
      facet= neighbor;
      break;
    }
  }
  while (facet) { 
    facet->seen2= True;
    if (neighbor->seen) {
      if (facet->visitid)
        qh_setappend (&centers, facet);
      else if (firstinf) {
        firstinf= False;
        qh_setappend (&centers, facet);
      }
    }
    FOREACHneighbor_(facet) {
      if (!neighbor->seen2) {
	if (qh_setin (vertex->neighbors, neighbor))
          break;
	else
	  neighbor->seen2= True;
      }
    }
    facet= neighbor;
  }

#if !defined(PSXII) && !defined(GEKKO)
  if (qh CHECKfrequently) {
    FOREACHneighbor_(vertex) {
      if (!neighbor->seen2) {
	fprintf (stderr, "qh_detvridge3: neigbors of vertex p%d are not connected at facet %d\n",
	         qh_pointid (vertex->point), neighbor->id);
	qh_errexit (qh_ERRqhull, neighbor, NULL);
      }
    }
  }
#endif

  FOREACHneighbor_(atvertex) 
    neighbor->seen2= True;
  return centers;
} /* detvridge3 */      

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="eachvoronoi">-</a>
  
  qh_eachvoronoi( fp, printvridge, vertex, visitall, innerouter, inorder )
    if visitall,
      visit all Voronoi ridges for vertex (i.e., an input site)
    else
      visit all unvisited Voronoi ridges for vertex
      all vertex->seen= False if unvisited
    assumes
      all facet->seen= False
      all facet->seen2= True (for qh_detvridge3)
      all facet->visitid == 0 if vertex_at_infinity
                         == index of Voronoi vertex 
                         >= qh.num_facets if ignored
    innerouter:
      qh_RIDGEall--  both inner (bounded) and outer (unbounded) ridges
      qh_RIDGEinner- only inner
      qh_RIDGEouter- only outer
      
    if inorder
      orders vertices for 3-d Voronoi diagrams
  
  returns:
    number of visited ridges (does not include previously visited ridges)
    
    if printvridge,
      calls printvridge( fp, vertex, vertexA, centers)
        fp== any pointer (assumes FILE*)
        vertex,vertexA= pair of input sites that define a Voronoi ridge
        centers= set of facets (i.e., Voronoi vertices)
                 ->visitid == index or 0 if vertex_at_infinity
                 ordered for 3-d Voronoi diagram
  notes:
    uses qh.vertex_visit
  
  see:
    qh_eachvoronoi_all()
  
  design:
    mark selected neighbors of atvertex
    for each selected neighbor (either Voronoi vertex or vertex-at-infinity)
      for each unvisited vertex 
        if atvertex and vertex share more than d-1 neighbors
          bump totalcount
          if printvridge defined
            build the set of shared neighbors (i.e., Voronoi vertices)
            call printvridge
*/
int qh_eachvoronoi (FILE *fp, printvridgeT printvridge, vertexT *atvertex, boolT visitall, qh_RIDGE innerouter, boolT inorder) {
  boolT unbounded;
  int count;
  facetT *neighbor, **neighborp, *neighborA, **neighborAp;
  setT *centers;
  vertexT *vertex, **vertexp;
  boolT firstinf;
  unsigned int numfacets= (unsigned int)qh num_facets;
  int totridges= 0;

  qh vertex_visit++;
  atvertex->seen= True;
  if (visitall) {
    FORALLvertices 
      vertex->seen= False;
  }
  FOREACHneighbor_(atvertex) {
    if (neighbor->visitid < numfacets) 
      neighbor->seen= True;
  }
  FOREACHneighbor_(atvertex) {
    if (neighbor->seen) {
      FOREACHvertex_(neighbor->vertices) {
        if (vertex->visitid != qh vertex_visit && !vertex->seen) {
	  vertex->visitid= qh vertex_visit;
          count= 0;
          firstinf= True;
          FOREACHneighborA_(vertex) {
            if (neighborA->seen) {
	      if (neighborA->visitid)
                count++;
              else if (firstinf) {
                count++;
                firstinf= False;
	      }
	    }
          }
          if (count >= qh hull_dim - 1) {  /* e.g., 3 for 3-d Voronoi */
            if (firstinf) {
              if (innerouter == qh_RIDGEouter)
                continue;
              unbounded= False;
            }else {
              if (innerouter == qh_RIDGEinner)
                continue;
              unbounded= True;
            }
            totridges++;
            trace4((qh ferr, "qh_eachvoronoi: Voronoi ridge of %d vertices between sites %d and %d\n",
                  count, qh_pointid (atvertex->point), qh_pointid (vertex->point)));
            if (printvridge) { 
	      if (inorder && qh hull_dim == 3+1) /* 3-d Voronoi diagram */
                centers= qh_detvridge3 (atvertex, vertex);
	      else
                centers= qh_detvridge (vertex);
              (*printvridge) (fp, atvertex, vertex, centers, unbounded);
              qh_settempfree (&centers);
            }
          }
        }
      }
    }
  }
  FOREACHneighbor_(atvertex) 
    neighbor->seen= False;
  return totridges;
} /* eachvoronoi */
  

/*-<a                             href="qh-c.htm#poly"
  >-------------------------------</a><a name="eachvoronoi_all">-</a>
  
  qh_eachvoronoi_all( fp, printvridge, isupper, innerouter, inorder )
    visit all Voronoi ridges
    
    innerouter:
      see qh_eachvoronoi()
      
    if inorder
      orders vertices for 3-d Voronoi diagrams
    
  returns
    total number of ridges 

    if isupper == facet->upperdelaunay  (i.e., a Vornoi vertex)
      facet->visitid= Voronoi vertex index (same as 'o' format)
    else 
      facet->visitid= 0

    if printvridge,
      calls printvridge( fp, vertex, vertexA, centers)
      [see qh_eachvoronoi]
      
  notes:
    same effect as qh_printvdiagram but ridges not sorted by point id
*/
int qh_eachvoronoi_all (FILE *fp, printvridgeT printvridge, boolT isupper, qh_RIDGE innerouter, boolT inorder) {
  facetT *facet;
  vertexT *vertex;
  int numcenters= 1;  /* vertex 0 is vertex-at-infinity */
  int totridges= 0;

  qh_clearcenters (qh_ASvoronoi);
  qh_vertexneighbors();
  maximize_(qh visit_id, (unsigned) qh num_facets);
  FORALLfacets {
    facet->visitid= 0;
    facet->seen= False;
    facet->seen2= True;
  }
  FORALLfacets {
    if (facet->upperdelaunay == isupper)
      facet->visitid= numcenters++;
  }
  FORALLvertices 
    vertex->seen= False;
  FORALLvertices 
    totridges += qh_eachvoronoi (fp, printvridge, vertex, 
                    !qh_ALL, innerouter, inorder);
  return totridges;
} /* eachvoronoi_all */
      
/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="facet2point">-</a>
  
  qh_facet2point( facet, point0, point1, mindist )
    return two projected temporary vertices for a 2-d facet
    may be non-simplicial

  returns:
    point0 and point1 oriented and projected to the facet
    returns mindist (maximum distance below plane)
*/
void qh_facet2point(facetT *facet, pointT **point0, pointT **point1, realT *mindist) {
  vertexT *vertex0, *vertex1;
  realT dist;
  
  if (facet->toporient ^ qh_ORIENTclock) {
    vertex0= SETfirstt_(facet->vertices, vertexT);
    vertex1= SETsecondt_(facet->vertices, vertexT);
  }else {
    vertex1= SETfirstt_(facet->vertices, vertexT);
    vertex0= SETsecondt_(facet->vertices, vertexT);
  }
  zadd_(Zdistio, 2);
  qh_distplane(vertex0->point, facet, &dist);
  *mindist= dist;
  *point0= qh_projectpoint(vertex0->point, facet, dist);
  qh_distplane(vertex1->point, facet, &dist);
  minimize_(*mindist, dist);		
  *point1= qh_projectpoint(vertex1->point, facet, dist);
} /* facet2point */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="facetvertices">-</a>
  
  qh_facetvertices( facetlist, facets, allfacets )
    returns temporary set of vertices in a set and/or list of facets
    if allfacets, ignores qh_skipfacet()

  returns:
    vertices with qh.vertex_visit
    
  notes:
    optimized for allfacets of facet_list

  design:
    if allfacets of facet_list
      create vertex set from vertex_list
    else
      for each selected facet in facets or facetlist
        append unvisited vertices to vertex set
*/
setT *qh_facetvertices (facetT *facetlist, setT *facets, boolT allfacets) {
  setT *vertices;
  facetT *facet, **facetp;
  vertexT *vertex, **vertexp;

  qh vertex_visit++;
  if (facetlist == qh facet_list && allfacets && !facets) {
    vertices= qh_settemp (qh num_vertices);
    FORALLvertices {
      vertex->visitid= qh vertex_visit; 
      qh_setappend (&vertices, vertex);
    }
  }else {
    vertices= qh_settemp (qh TEMPsize);
    FORALLfacet_(facetlist) {
      if (!allfacets && qh_skipfacet (facet))
        continue;
      FOREACHvertex_(facet->vertices) {
        if (vertex->visitid != qh vertex_visit) {
          vertex->visitid= qh vertex_visit;
          qh_setappend (&vertices, vertex);
        }
      }
    }
  }
  FOREACHfacet_(facets) {
    if (!allfacets && qh_skipfacet (facet))
      continue;
    FOREACHvertex_(facet->vertices) {
      if (vertex->visitid != qh vertex_visit) {
        vertex->visitid= qh vertex_visit;
        qh_setappend (&vertices, vertex);
      }
    }
  }
  return vertices;
} /* facetvertices */

/*-<a                             href="qh-c.htm#geom"
  >-------------------------------</a><a name="geomplanes">-</a>
  
  qh_geomplanes( facet, outerplane, innerplane )
    return outer and inner planes for Geomview 
    qh.PRINTradius is size of vertices and points (includes qh.JOGGLEmax)

  notes:
    assume precise calculations in io.c with roundoff covered by qh_GEOMepsilon
*/
void qh_geomplanes (facetT *facet, realT *outerplane, realT *innerplane) {
  realT radius;

  if (qh MERGING || qh JOGGLEmax < REALmax/2) {
    qh_outerinner (facet, outerplane, innerplane);
    radius= qh PRINTradius;
    if (qh JOGGLEmax < REALmax/2)
      radius -= qh JOGGLEmax * sqrt (qh hull_dim);  /* already accounted for in qh_outerinner() */
    *outerplane += radius;
    *innerplane -= radius;
    if (qh PRINTcoplanar || qh PRINTspheres) {
      *outerplane += qh MAXabs_coord * qh_GEOMepsilon;
      *innerplane -= qh MAXabs_coord * qh_GEOMepsilon;
    }
  }else 
    *innerplane= *outerplane= 0;
} /* geomplanes */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="markkeep">-</a>
  
  qh_markkeep( facetlist )
    mark good facets that meet qh.KEEParea, qh.KEEPmerge, and qh.KEEPminArea
    ignores visible facets (not part of convex hull)

  returns:
    may clear facet->good
    recomputes qh.num_good

  design:
    get set of good facets
    if qh.KEEParea
      sort facets by area
      clear facet->good for all but n largest facets
    if qh.KEEPmerge
      sort facets by merge count
      clear facet->good for all but n most merged facets
    if qh.KEEPminarea
      clear facet->good if area too small
    update qh.num_good    
*/
void qh_markkeep (facetT *facetlist) {
  facetT *facet, **facetp;
  setT *facets= qh_settemp (qh num_facets);
  int size, count;

  trace2((qh ferr, "qh_markkeep: only keep %d largest and/or %d most merged facets and/or min area %.2g\n",
          qh KEEParea, qh KEEPmerge, qh KEEPminArea));
  FORALLfacet_(facetlist) {
    if (!facet->visible && facet->good)
      qh_setappend (&facets, facet);
  }
  size= qh_setsize (facets);
  if (qh KEEParea) {
    qsort (SETaddr_(facets, facetT), size,
             sizeof (facetT *), qh_compare_facetarea);
    if ((count= size - qh KEEParea) > 0) {
      FOREACHfacet_(facets) {
        facet->good= False;
        if (--count == 0)
          break;
      }
    }
  }
  if (qh KEEPmerge) {
    qsort (SETaddr_(facets, facetT), size,
             sizeof (facetT *), qh_compare_facetmerge);
    if ((count= size - qh KEEPmerge) > 0) {
      FOREACHfacet_(facets) {
        facet->good= False;
        if (--count == 0)
          break;
      }
    }
  }
  if (qh KEEPminArea < REALmax/2) {
    FOREACHfacet_(facets) {
      if (!facet->isarea || facet->f.area < qh KEEPminArea)
	facet->good= False;
    }
  }
  qh_settempfree (&facets);
  count= 0;
  FORALLfacet_(facetlist) {
    if (facet->good)
      count++;
  }
  qh num_good= count;
} /* markkeep */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="markvoronoi">-</a>
  
  qh_markvoronoi( facetlist, facets, printall, islower, numcenters )
    mark voronoi vertices for printing by site pairs
  
  returns:
    temporary set of vertices indexed by pointid
    islower set if printing lower hull (i.e., at least one facet is lower hull)
    numcenters= total number of Voronoi vertices
    bumps qh.printoutnum for vertex-at-infinity
    clears all facet->seen and sets facet->seen2
    
    if selected
      facet->visitid= Voronoi vertex id
    else if upper hull (or 'Qu' and lower hull)
      facet->visitid= 0
    else
      facet->visitid >= qh num_facets
  
  notes:
    ignores qh ATinfinity, if defined
*/
setT *qh_markvoronoi (facetT *facetlist, setT *facets, boolT printall, boolT *islowerp, int *numcentersp) {
  int numcenters=0;
  facetT *facet, **facetp;
  setT *vertices;
  boolT islower= False;

  qh printoutnum++;
  qh_clearcenters (qh_ASvoronoi);  /* in case, qh_printvdiagram2 called by user */
  qh_vertexneighbors();
  vertices= qh_pointvertex();
  if (qh ATinfinity) 
    SETelem_(vertices, qh num_points-1)= NULL;
  qh visit_id++;
  maximize_(qh visit_id, (unsigned) qh num_facets);
  FORALLfacet_(facetlist) {  /* FIXUP: could merge with below */
    if (printall || !qh_skipfacet (facet)) {
      if (!facet->upperdelaunay) {
        islower= True;
	break;
      }
    }
  }
  FOREACHfacet_(facets) {
    if (printall || !qh_skipfacet (facet)) {
      if (!facet->upperdelaunay) {
        islower= True;
	break;
      }
    }
  }
  FORALLfacets {
    if (facet->normal && (facet->upperdelaunay == islower))
      facet->visitid= 0;  /* facetlist or facets may overwrite */
    else
      facet->visitid= qh visit_id;
    facet->seen= False;
    facet->seen2= True;
  }
  numcenters++;  /* qh_INFINITE */
  FORALLfacet_(facetlist) {
    if (printall || !qh_skipfacet (facet))
      facet->visitid= numcenters++;
  }
  FOREACHfacet_(facets) {
    if (printall || !qh_skipfacet (facet))
      facet->visitid= numcenters++;  
  }
  *islowerp= islower;
  *numcentersp= numcenters;
  trace2((qh ferr, "qh_markvoronoi: islower %d numcenters %d\n", islower, numcenters));
  return vertices;
} /* markvoronoi */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="order_vertexneighbors">-</a>
  
  qh_order_vertexneighbors( vertex )
    order facet neighbors of a 2-d or 3-d vertex by adjacency

  notes:
    does not orient the neighbors

  design:
    initialize a new neighbor set with the first facet in vertex->neighbors
    while vertex->neighbors non-empty
      select next neighbor in the previous facet's neighbor set
    set vertex->neighbors to the new neighbor set
*/
void qh_order_vertexneighbors(vertexT *vertex) {
  setT *newset;
  facetT *facet, *neighbor, **neighborp;

  trace4((qh ferr, "qh_order_vertexneighbors: order neighbors of v%d for 3-d\n", vertex->id));
  newset= qh_settemp (qh_setsize (vertex->neighbors));
  facet= (facetT*)qh_setdellast (vertex->neighbors);
  qh_setappend (&newset, facet);
  while (qh_setsize (vertex->neighbors)) {
    FOREACHneighbor_(vertex) {
      if (qh_setin (facet->neighbors, neighbor)) {
        qh_setdel(vertex->neighbors, neighbor);
        qh_setappend (&newset, neighbor);
        facet= neighbor;
        break;
      }
    }
    if (!neighbor) {
      ivp_message( "qhull internal error (qh_order_vertexneighbors): no neighbor of v%d for f%d\n",
        vertex->id, facet->id);
      qh_errexit (qh_ERRqhull, facet, NULL);
    }
  }
  qh_setfree (&vertex->neighbors);
  qh_settemppop ();
  vertex->neighbors= newset;
} /* order_vertexneighbors */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printafacet">-</a>
  
  qh_printafacet( fp, format, facet, printall )
    print facet to fp in given output format (see qh.PRINTout)

  returns:
    nop if !printall and qh_skipfacet()
    nop if visible facet and NEWfacets and format != PRINTfacets
    must match qh_countfacets

  notes
    preserves qh.visit_id
    facet->normal may be null if PREmerge/MERGEexact and STOPcone before merge

  see
    qh_printbegin() and qh_printend()

  design:
    test for printing facet
    call appropriate routine for format
    or output results directly
*/
void qh_printafacet(FILE *fp, int format, facetT *facet, boolT printall) {
  realT color[4], offset, dist, outerplane, innerplane;
  boolT zerodiv;
  coordT *point, *normp, *coordp, **pointp, *feasiblep;
  int k;
  vertexT *vertex, **vertexp;
  facetT *neighbor, **neighborp;

  if (!fp){
	  return;
  }
  if (!printall && qh_skipfacet (facet))
    return;
  if (facet->visible && qh NEWfacets && format != qh_PRINTfacets)
    return;
  qh printoutnum++;
  switch (format) {
  case qh_PRINTarea:
    if (facet->isarea) {
      fprintf (fp, qh_REAL_1, facet->f.area);
      fprintf (fp, "\n");
    }else
      fprintf (fp, "0\n");
    break;
  case qh_PRINTcoplanars:
    fprintf (fp, "%d", qh_setsize (facet->coplanarset));
    FOREACHpoint_(facet->coplanarset)
      fprintf (fp, " %d", qh_pointid (point));
    fprintf (fp, "\n");
    break;
  case qh_PRINTcentrums:
    qh_printcenter (fp, format, NULL, facet);
    break;
  case qh_PRINTfacets:
    qh_printfacet (fp, facet);
    break;
  case qh_PRINTfacets_xridge:
    qh_printfacetheader (fp, facet);
    break;
  case qh_PRINTgeom:  /* either 2 , 3, or 4-d by qh_printbegin */
    if (!facet->normal)
      break;
    for (k= qh hull_dim; k--; ) {
      color[k]= (facet->normal[k]+1.0)/2.0;
      maximize_(color[k], -1.0);
      minimize_(color[k], +1.0);
    }
    qh_projectdim3 (color, color);
    if (qh PRINTdim != qh hull_dim)
      qh_normalize2 (color, 3, True, NULL, NULL);
    if (qh hull_dim <= 2)
      qh_printfacet2geom (fp, facet, color);
    else if (qh hull_dim == 3) {
      if (facet->simplicial)
        qh_printfacet3geom_simplicial (fp, facet, color);
      else
        qh_printfacet3geom_nonsimplicial (fp, facet, color);
    }else {
      if (facet->simplicial)
        qh_printfacet4geom_simplicial (fp, facet, color);
      else
        qh_printfacet4geom_nonsimplicial (fp, facet, color);
    }
    break;
  case qh_PRINTids:
    fprintf (fp, "%d\n", facet->id);
    break;
  case qh_PRINTincidences:
  case qh_PRINToff:
  case qh_PRINTtriangles:
    if (qh hull_dim == 3 && format != qh_PRINTtriangles) 
      qh_printfacet3vertex (fp, facet, format);
    else if (facet->simplicial || qh hull_dim == 2 || format == qh_PRINToff)
      qh_printfacetNvertex_simplicial (fp, facet, format);
    else
      qh_printfacetNvertex_nonsimplicial (fp, facet, qh printoutvar++, format);
    break;
  case qh_PRINTinner:
    qh_outerinner (facet, NULL, &innerplane);
    offset= facet->offset - innerplane;
    goto LABELprintnorm;
  case qh_PRINTmerges:
    fprintf (fp, "%d\n", facet->nummerge);
    break;
  case qh_PRINTnormals:
    offset= facet->offset;
    goto LABELprintnorm;
  case qh_PRINTouter:
    qh_outerinner (facet, &outerplane, NULL);
    offset= facet->offset - outerplane;
  LABELprintnorm:
    if (!facet->normal) {
      fprintf (fp, "no normal for facet f%d\n", facet->id);
      break;
    }
    if (qh CDDoutput) 
      fprintf (fp, qh_REAL_1, -offset);
    for (k=0; k < qh hull_dim; k++) 
      fprintf (fp, qh_REAL_1, facet->normal[k]);
    if (!qh CDDoutput) 
      fprintf (fp, qh_REAL_1, offset);
    fprintf (fp, "\n");
    break;
  case qh_PRINTmathematica:  /* either 2 or 3-d by qh_printbegin */
    if (qh hull_dim == 2)
      qh_printfacet2math (fp, facet, qh printoutvar++);
    else 
      qh_printfacet3math (fp, facet, qh printoutvar++);
    break;
  case qh_PRINTneighbors:
    fprintf (fp, "%d", qh_setsize (facet->neighbors));
    FOREACHneighbor_(facet)
      fprintf (fp, " %d", 
	       neighbor->visitid ? neighbor->visitid - 1: neighbor->id);
    fprintf (fp, "\n");
    break;
  case qh_PRINTpointintersect:
    if (!qh feasible_point) {
      fprintf (fp, "qhull input error (qh_printafacet): option 'Fp' needs qh feasible_point\n");
      qh_errexit( qh_ERRinput, NULL, NULL);
    }
    if (facet->offset > 0)
      goto LABELprintinfinite;
    point= coordp= (coordT*)qh_memalloc (qh normal_size);
    normp= facet->normal;
    feasiblep= qh feasible_point;
    if (facet->offset < -qh MINdenom) {
      for (k= qh hull_dim; k--; )
        *(coordp++)= (*(normp++) / - facet->offset) + *(feasiblep++);
    }else {
      for (k= qh hull_dim; k--; ) {
        *(coordp++)= qh_divzero (*(normp++), facet->offset, qh MINdenom_1,
				 &zerodiv) + *(feasiblep++);
        if (zerodiv) {
          qh_memfree (point, qh normal_size);
          goto LABELprintinfinite;
        }
      }
    }
    qh_printpoint (fp, NULL, point);
    qh_memfree (point, qh normal_size);
    break;
  LABELprintinfinite:
    for (k= qh hull_dim; k--; )
      fprintf (fp, qh_REAL_1, qh_INFINITE);
    fprintf (fp, "\n");   
    break;
  case qh_PRINTpointnearest:
    FOREACHpoint_(facet->coplanarset) {
      int id, id2;
      vertex= qh_nearvertex (facet, point, &dist);
      id= qh_pointid (vertex->point);
      id2= qh_pointid (point);
      fprintf (fp, "%d %d %d " qh_REAL_1 "\n", id, id2, facet->id, dist);
    }
    break;
  case qh_PRINTpoints:  /* VORONOI only by qh_printbegin */
    if (qh CDDoutput)
      fprintf (fp, "1 ");
    qh_printcenter (fp, format, NULL, facet);
    break;
  case qh_PRINTvertices:
    fprintf (fp, "%d", qh_setsize (facet->vertices));
    FOREACHvertex_(facet->vertices)
      fprintf (fp, " %d", qh_pointid (vertex->point));
    fprintf (fp, "\n");
    break;
  }
} /* printafacet */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printbegin">-</a>
  
  qh_printbegin(  )
    prints header for all output formats

  returns:
    checks for valid format
  
  notes:
    uses qh.visit_id for 3/4off
    changes qh.interior_point if printing centrums
    qh_countfacets clears facet->visitid for non-good facets
    
  see
    qh_printend() and qh_printafacet()
    
  design:
    count facets and related statistics
    print header for format
*/
void qh_printbegin (FILE *fp, int format, facetT *facetlist, setT *facets, boolT printall) {
  int numfacets, numsimplicial, numridges, totneighbors, numcoplanars;
  int i, num;
  facetT *facet, **facetp;
  vertexT *vertex, **vertexp;
  setT *vertices;
  pointT *point, **pointp, *pointtemp;

  if (!fp) return;
  qh printoutnum= 0;
  qh_countfacets (facetlist, facets, printall, &numfacets, &numsimplicial, 
      &totneighbors, &numridges, &numcoplanars);
  switch (format) {
  case qh_PRINTnone:
    break;
  case qh_PRINTarea:
    fprintf (fp, "%d\n", numfacets);
    break;
  case qh_PRINTcoplanars:
    fprintf (fp, "%d\n", numfacets);
    break;
  case qh_PRINTcentrums:
    if (qh CENTERtype == qh_ASnone)
      qh_clearcenters (qh_AScentrum);
    fprintf (fp, "%d\n%d\n", qh hull_dim, numfacets);
    break;
  case qh_PRINTfacets:
  case qh_PRINTfacets_xridge:
    if (facetlist)
      qh_printvertexlist (fp, "Vertices and facets:\n", facetlist, facets, printall);
    break;
  case qh_PRINTgeom: 
    if (qh hull_dim > 4)  /* qh_initqhull_globals also checks */ //-V112
      goto LABELnoformat;
    if (qh VORONOI && qh hull_dim > 3)  /* PRINTdim == DROPdim == hull_dim-1 */
      goto LABELnoformat;
    if (qh hull_dim == 2 && (qh PRINTridges || qh DOintersections))
      ivp_message( "qhull warning: output for ridges and intersections not implemented in 2-d\n");
    if (qh hull_dim == 4 && (qh PRINTinner || qh PRINTouter || //-V112
			     (qh PRINTdim == 4 && qh PRINTcentrums))) //-V112
      ivp_message( "qhull warning: output for outer/inner planes and centrums not implemented in 4-d\n");
    if (qh PRINTdim == 4 && (qh PRINTspheres)) //-V112
      ivp_message( "qhull warning: output for vertices not implemented in 4-d\n");
    if (qh PRINTdim == 4 && qh DOintersections && qh PRINTnoplanes) //-V112
      ivp_message( "qhull warning: 'Gnh' generates no output in 4-d\n");
    if (qh PRINTdim == 2) {
      fprintf(fp, "{appearance {linewidth 3} LIST # %s | %s\n",
	      qh rbox_command, qh qhull_command);
    }else if (qh PRINTdim == 3) {
      fprintf(fp, "{appearance {+edge -evert linewidth 2} LIST # %s | %s\n",
	      qh rbox_command, qh qhull_command);
    }else if (qh PRINTdim == 4) { //-V112
      qh visit_id++;
      num= 0;
      FORALLfacet_(facetlist)    /* get number of ridges to be printed */
        qh_printend4geom (NULL, facet, &num, printall);
      FOREACHfacet_(facets)
        qh_printend4geom (NULL, facet, &num, printall);
      qh ridgeoutnum= num;
      qh printoutvar= 0;  /* counts number of ridges in output */
      fprintf (fp, "LIST # %s | %s\n", qh rbox_command, qh qhull_command);
    }
    if (qh PRINTdots) {
      qh printoutnum++;
      num= qh num_points + qh_setsize (qh other_points);
      if (qh DELAUNAY && qh ATinfinity)
	num--;
      if (qh PRINTdim == 4) //-V112
        fprintf (fp, "4VECT %d %d 1\n", num, num);
      else
	fprintf (fp, "VECT %d %d 1\n", num, num);
      for (i= num; i--; ) {
        if (i % 20 == 0)
          fprintf (fp, "\n");
	fprintf (fp, "1 ");
      }
      fprintf (fp, "# 1 point per line\n1 ");
      for (i= num-1; i--; ) {
        if (i % 20 == 0)
          fprintf (fp, "\n");
	fprintf (fp, "0 ");
      }
      fprintf (fp, "# 1 color for all\n");
      FORALLpoints {
        if (!qh DELAUNAY || !qh ATinfinity || qh_pointid(point) != qh num_points-1) {
	  if (qh PRINTdim == 4) //-V112
	    qh_printpoint (fp, NULL, point);
	  else
	    qh_printpoint3 (fp, point);
	}
      }
      FOREACHpoint_(qh other_points) {
	if (qh PRINTdim == 4) //-V112
	  qh_printpoint (fp, NULL, point);
	else
	  qh_printpoint3 (fp, point);
      }
      fprintf (fp, "0 1 1 1  # color of points\n");
    }
    if (qh PRINTdim == 4  && !qh PRINTnoplanes) //-V112
      /* 4dview loads up multiple 4OFF objects slowly */
      fprintf(fp, "4OFF %d %d 1\n", 3*qh ridgeoutnum, qh ridgeoutnum);
    qh PRINTcradius= 2 * qh DISTround;  /* include test DISTround */
    if (qh PREmerge) {
      maximize_(qh PRINTcradius, qh premerge_centrum + qh DISTround);
    }else if (qh POSTmerge)
      maximize_(qh PRINTcradius, qh postmerge_centrum + qh DISTround);
    qh PRINTradius= qh PRINTcradius;
    if (qh PRINTspheres + qh PRINTcoplanar)
      maximize_(qh PRINTradius, qh MAXabs_coord * qh_MINradius);
    if (qh premerge_cos < REALmax/2) {
      maximize_(qh PRINTradius, (1- qh premerge_cos) * qh MAXabs_coord);
    }else if (!qh PREmerge && qh POSTmerge && qh postmerge_cos < REALmax/2) {
      maximize_(qh PRINTradius, (1- qh postmerge_cos) * qh MAXabs_coord);
    }
    maximize_(qh PRINTradius, qh MINvisible); 
    if (qh JOGGLEmax < REALmax/2)
      qh PRINTradius += qh JOGGLEmax * sqrt (qh hull_dim);
    if (qh PRINTdim != 4 && //-V112
	(qh PRINTcoplanar || qh PRINTspheres || qh PRINTcentrums)) {
      vertices= qh_facetvertices (facetlist, facets, printall);
      if (qh PRINTspheres && qh PRINTdim <= 3)
         qh_printspheres (fp, vertices, qh PRINTradius);
      if (qh PRINTcoplanar || qh PRINTcentrums) {
        qh firstcentrum= True;
        if (qh PRINTcoplanar&& !qh PRINTspheres) {
          FOREACHvertex_(vertices) 
            qh_printpointvect2 (fp, vertex->point, NULL,
				qh interior_point, qh PRINTradius);
	}
        FORALLfacet_(facetlist) {
	  if (!printall && qh_skipfacet(facet))
	    continue;
	  if (!facet->normal)
	    continue;
          if (qh PRINTcentrums && qh PRINTdim <= 3)
            qh_printcentrum (fp, facet, qh PRINTcradius);
	  if (!qh PRINTcoplanar)
	    continue;
          FOREACHpoint_(facet->coplanarset)
            qh_printpointvect2 (fp, point, facet->normal, NULL, qh PRINTradius);
          FOREACHpoint_(facet->outsideset)
            qh_printpointvect2 (fp, point, facet->normal, NULL, qh PRINTradius);
        }
        FOREACHfacet_(facets) {
	  if (!printall && qh_skipfacet(facet))
	    continue;
	  if (!facet->normal)
	    continue;
          if (qh PRINTcentrums && qh PRINTdim <= 3)
            qh_printcentrum (fp, facet, qh PRINTcradius);
	  if (!qh PRINTcoplanar)
	    continue;
          FOREACHpoint_(facet->coplanarset)
            qh_printpointvect2 (fp, point, facet->normal, NULL, qh PRINTradius);
          FOREACHpoint_(facet->outsideset)
            qh_printpointvect2 (fp, point, facet->normal, NULL, qh PRINTradius);
        }
      }
      qh_settempfree (&vertices);
    }
    qh visit_id++; /* for printing hyperplane intersections */
    break;
  case qh_PRINTids:
    fprintf (fp, "%d\n", numfacets);
    break;
  case qh_PRINTincidences:
    if (qh VORONOI)
      ivp_message( "qhull warning: writing Delaunay.  Use 'p' or 'o' for Voronoi centers\n");
    qh printoutvar= qh vertex_id;  /* centrum id for non-simplicial facets */
    if (qh hull_dim <= 3)
      fprintf(fp, "%d\n", numfacets);
    else
      fprintf(fp, "%d\n", numsimplicial+numridges);
    break;
  case qh_PRINTinner:
  case qh_PRINTnormals:
  case qh_PRINTouter:
    if (qh CDDoutput)
      fprintf (fp, "%s | %s\nbegin\n    %d %d real\n", qh rbox_command, 
              qh qhull_command, numfacets, qh hull_dim+1);
    else
      fprintf (fp, "%d\n%d\n", qh hull_dim+1, numfacets);
    break;
  case qh_PRINTmathematica:  
    if (qh hull_dim > 3)  /* qh_initbuffers also checks */
      goto LABELnoformat;
    if (qh VORONOI)
      ivp_message( "qhull warning: output is the Delaunay triangulation\n");
    fprintf(fp, "{\n");
    qh printoutvar= 0;   /* counts number of facets for notfirst */
    break;
  case qh_PRINTmerges:
    fprintf (fp, "%d\n", numfacets);
    break;
  case qh_PRINTpointintersect:
    fprintf (fp, "%d\n%d\n", qh hull_dim, numfacets);
    break;
  case qh_PRINTneighbors:
    fprintf (fp, "%d\n", numfacets);
    break;
  case qh_PRINToff:
  case qh_PRINTtriangles:
    if (qh VORONOI)
      goto LABELnoformat;
    num = qh hull_dim;
    if (format == qh_PRINToff || qh hull_dim == 2)
      fprintf (fp, "%d\n%d %d %d\n", num, 
        qh num_points+qh_setsize (qh other_points), numfacets, totneighbors/2);
    else { /* qh_PRINTtriangles */
      qh printoutvar= qh num_points+qh_setsize (qh other_points); /* first centrum */
      if (qh DELAUNAY)
        num--;  /* drop last dimension */
      fprintf (fp, "%d\n%d %d %d\n", num, qh printoutvar 
	+ numfacets - numsimplicial, numsimplicial + numridges, totneighbors/2);
    }
    FORALLpoints
      qh_printpointid (qh fout, NULL, num, point, -1);
    FOREACHpoint_(qh other_points)
      qh_printpointid (qh fout, NULL, num, point, -1);
    if (format == qh_PRINTtriangles && qh hull_dim > 2) {
      FORALLfacets {
	if (!facet->simplicial && facet->visitid)
          qh_printcenter (qh fout, format, NULL, facet);
      }
    }
    break;
  case qh_PRINTpointnearest:
    fprintf (fp, "%d\n", numcoplanars);
    break;
  case qh_PRINTpoints:
    if (!qh VORONOI)
      goto LABELnoformat;
    if (qh CDDoutput)
      fprintf (fp, "%s | %s\nbegin\n%d %d real\n", qh rbox_command,
             qh qhull_command, numfacets, qh hull_dim);
    else
      fprintf (fp, "%d\n%d\n", qh hull_dim-1, numfacets);
    break;
  case qh_PRINTvertices:
    fprintf (fp, "%d\n", numfacets);
    break;
  case qh_PRINTsummary:
  default:
  LABELnoformat:
    ivp_message( "qhull internal error (qh_printbegin): can not use this format for dimension %d\n",
         qh hull_dim);
    qh_errexit (qh_ERRqhull, NULL, NULL);
  }
} /* printbegin */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printcenter">-</a>
  
  qh_printcenter( fp, string, facet )
    print facet->center as centrum or Voronoi center
    string may be NULL.  Don't include '%' codes.
    nop if qh CENTERtype neither CENTERvoronoi nor CENTERcentrum
    if upper envelope of Delaunay triangulation and point at-infinity
      prints qh_INFINITE instead;

  notes:
    defines facet->center if needed
    if format=PRINTgeom, adds a 0 if would otherwise be 2-d
*/
void qh_printcenter (FILE *fp, int format, const char *string, facetT *facet) {
  int k, num;
	if (!fp) return;
  if (qh CENTERtype != qh_ASvoronoi && qh CENTERtype != qh_AScentrum)
    return;
  if (string)
    fprintf (fp, string, facet->id);
  if (qh CENTERtype == qh_ASvoronoi) {
    num= qh hull_dim-1;
    if (!facet->normal || !facet->upperdelaunay || !qh ATinfinity) {
      if (!facet->center)
        facet->center= qh_facetcenter (facet->vertices);
      for (k=0; k < num; k++)
        fprintf (fp, qh_REAL_1, facet->center[k]);
    }else {
      for (k=0; k < num; k++)
        fprintf (fp, qh_REAL_1, qh_INFINITE);
    }
  }else /* qh CENTERtype == qh_AScentrum */ {
    num= qh hull_dim;
    if (format == qh_PRINTtriangles && qh DELAUNAY) 
      num--;
    if (!facet->center) 
      facet->center= qh_getcentrum (facet);
    for (k=0; k < num; k++)
      fprintf (fp, qh_REAL_1, facet->center[k]);
  }
  if (format == qh_PRINTgeom && num == 2)
    fprintf (fp, " 0\n");
  else
    fprintf (fp, "\n");
} /* printcenter */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printcentrum">-</a>
  
  qh_printcentrum( fp, facet, radius )
    print centrum for a facet in OOGL format
    radius defines size of centrum
    2-d or 3-d only

  returns:
    defines facet->center if needed
*/
void qh_printcentrum (FILE *fp, facetT *facet, realT radius) {
  pointT *centrum, *projpt;
  boolT tempcentrum= False;
  realT xaxis[4], yaxis[4], normal[4], dist;
  realT green[3]={0, 1, 0};
  vertexT *apex;
  int k;
	if (!fp ) return;  
  if (qh CENTERtype == qh_AScentrum) {
    if (!facet->center)
      facet->center= qh_getcentrum (facet);
    centrum= facet->center;
  }else {
    centrum= qh_getcentrum (facet);
    tempcentrum= True;
  }
  fprintf (fp, "{appearance {-normal -edge normscale 0} ");
  if (qh firstcentrum) {
    qh firstcentrum= False;
    fprintf (fp, "{INST geom { define centrum CQUAD  # f%d\n\
-0.3 -0.3 0.0001     0 0 1 1\n\
 0.3 -0.3 0.0001     0 0 1 1\n\
 0.3  0.3 0.0001     0 0 1 1\n\
-0.3  0.3 0.0001     0 0 1 1 } transform { \n", facet->id);
  }else
    fprintf (fp, "{INST geom { : centrum } transform { # f%d\n", facet->id);
  apex= SETfirstt_(facet->vertices, vertexT);
  qh_distplane(apex->point, facet, &dist);
  projpt= qh_projectpoint(apex->point, facet, dist);
  for (k= qh hull_dim; k--; ) {
    xaxis[k]= projpt[k] - centrum[k];
    normal[k]= facet->normal[k];
  }
  if (qh hull_dim == 2) {
    xaxis[2]= 0;
    normal[2]= 0;
  }else if (qh hull_dim == 4) { //-V112
    qh_projectdim3 (xaxis, xaxis);
    qh_projectdim3 (normal, normal);
    qh_normalize2 (normal, qh PRINTdim, True, NULL, NULL);
  }
  qh_crossproduct (3, xaxis, normal, yaxis);
  fprintf (fp, "%8.4g %8.4g %8.4g 0\n", xaxis[0], xaxis[1], xaxis[2]);
  fprintf (fp, "%8.4g %8.4g %8.4g 0\n", yaxis[0], yaxis[1], yaxis[2]);
  fprintf (fp, "%8.4g %8.4g %8.4g 0\n", normal[0], normal[1], normal[2]);
  qh_printpoint3 (fp, centrum);
  fprintf (fp, "1 }}}\n"); 
  qh_memfree (projpt, qh normal_size);
  qh_printpointvect (fp, centrum, facet->normal, NULL, radius, green);
  if (tempcentrum)
    qh_memfree (centrum, qh normal_size);
} /* printcentrum */
  
/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printend">-</a>
  
  qh_printend( fp, format )
    prints trailer for all output formats

  see:
    qh_printbegin() and qh_printafacet()
      
*/
void qh_printend (FILE *fp, int format, facetT *facetlist, setT *facets, boolT printall) {
  int num;
  facetT *facet, **facetp;
	if (!fp) return;
  if (!qh printoutnum)
    ivp_message( "qhull warning: no facets printed\n");
  switch (format) {
  case qh_PRINTgeom:
    if (qh hull_dim == 4 && qh DROPdim < 0  && !qh PRINTnoplanes) { //-V112
      qh visit_id++;
      num= 0;
      FORALLfacet_(facetlist)
        qh_printend4geom (fp, facet,&num, printall);
      FOREACHfacet_(facets) 
        qh_printend4geom (fp, facet, &num, printall);
      if (num != qh ridgeoutnum || qh printoutvar != qh ridgeoutnum) {
	ivp_message( "qhull internal error (qh_printend): number of ridges %d != number printed %d and at end %d\n", qh ridgeoutnum, qh printoutvar, num);
	qh_errexit (qh_ERRqhull, NULL, NULL);
      }
    }else
      fprintf(fp, "}\n");
    break;
  case qh_PRINTinner:
  case qh_PRINTnormals:
  case qh_PRINTouter:
    if (qh CDDoutput) 
      fprintf (fp, "end\n");
    break;
  case qh_PRINTmathematica:
    fprintf(fp, "}\n");
    break;
  case qh_PRINTpoints:
    if (qh CDDoutput)
      fprintf (fp, "end\n");
    break;
  }
} /* printend */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printend4geom">-</a>
  
  qh_printend4geom( fp, facet, numridges, printall )
    helper function for qh_printbegin/printend

  returns:
    number of printed ridges
  
  notes:
    just counts printed ridges if fp=NULL
    uses facet->visitid
    must agree with qh_printfacet4geom...

  design:
    computes color for facet from its normal
    prints each ridge of facet 
*/
void qh_printend4geom (FILE *fp, facetT *facet, int *nump, boolT printall) {
  realT color[3];
  int i, num= *nump;
  facetT *neighbor, **neighborp;
  ridgeT *ridge, **ridgep;
	if (!fp) return;  
  if (!printall && qh_skipfacet(facet))
    return;
  if (qh PRINTnoplanes || (facet->visible && qh NEWfacets))
    return;
  if (!facet->normal)
    return;
  if (fp) {
    for (i=0; i < 3; i++) {
      color[i]= (facet->normal[i]+1.0)/2.0;
      maximize_(color[i], -1.0);
      minimize_(color[i], +1.0);
    }
  }
  facet->visitid= qh visit_id;
  if (facet->simplicial) {
    FOREACHneighbor_(facet) {
      if (neighbor->visitid != qh visit_id) {
	if (fp)
          fprintf (fp, "3 %d %d %d %8.4g %8.4g %8.4g 1 # f%d f%d\n",
		 3*num, 3*num+1, 3*num+2, color[0], color[1], color[2],
		 facet->id, neighbor->id);
	num++;
      }
    }
  }else {
    FOREACHridge_(facet->ridges) {
      neighbor= otherfacet_(ridge, facet);
      if (neighbor->visitid != qh visit_id) {
	if (fp)
          fprintf (fp, "3 %d %d %d %8.4g %8.4g %8.4g 1 #r%d f%d f%d\n",
		 3*num, 3*num+1, 3*num+2, color[0], color[1], color[2],
		 ridge->id, facet->id, neighbor->id);
	num++;
      }
    }
  }
  *nump= num;
} /* printend4geom */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printextremes">-</a>
  
  qh_printextremes( fp, facetlist, facets, printall )
    print extreme points for convex hulls or halfspace intersections

  notes:
    #points, followed by ids, one per line
    
    sorted by id
    same order as qh_printpoints_out if no coplanar/interior points
*/
void qh_printextremes (FILE *fp, facetT *facetlist, setT *facets, int printall) {
  setT *vertices, *points;
  pointT *point;
  vertexT *vertex, **vertexp;
  int id;
  int numpoints=0, point_i, point_n;
  int allpoints= qh num_points + qh_setsize (qh other_points);

  points= qh_settemp (allpoints);
  qh_setzero (points, 0, allpoints);
  vertices= qh_facetvertices (facetlist, facets, printall);
  FOREACHvertex_(vertices) {
    id= qh_pointid (vertex->point);
    if (id >= 0) {
      SETelem_(points, id)= vertex->point;
      numpoints++;
    }
  }
  qh_settempfree (&vertices);
  fprintf (fp, "%d\n", numpoints);
  FOREACHpoint_i_(points) {
    if (point) 
      fprintf (fp, "%d\n", point_i);
  }
  qh_settempfree (&points);
} /* printextremes */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printextremes_2d">-</a>
  
  qh_printextremes_2d( fp, facetlist, facets, printall )
    prints point ids for facets in qh_ORIENTclock order

  notes:
    #points, followed by ids, one per line
    if facetlist/facets are disjoint than the output includes skips
    errors if facets form a loop
    does not print coplanar points
*/
void qh_printextremes_2d (FILE *fp, facetT *facetlist, setT *facets, int printall) {
  int numfacets, numridges, totneighbors, numcoplanars, numsimplicial;
  setT *vertices;
  facetT *facet, *startfacet, *nextfacet;
  vertexT *vertexA, *vertexB;
	if (!fp) return;
  qh_countfacets (facetlist, facets, printall, &numfacets, &numsimplicial, 
      &totneighbors, &numridges, &numcoplanars); /* marks qh visit_id */
  vertices= qh_facetvertices (facetlist, facets, printall);
  fprintf(fp, "%d\n", qh_setsize (vertices));
  qh_settempfree (&vertices);
  if (!numfacets)
    return;
  facet= startfacet= facetlist ? facetlist : SETfirstt_(facets, facetT);
  qh vertex_visit++;
  qh visit_id++;
  do {
    if (facet->toporient ^ qh_ORIENTclock) {
      vertexA= SETfirstt_(facet->vertices, vertexT);
      vertexB= SETsecondt_(facet->vertices, vertexT);
      nextfacet= SETfirstt_(facet->neighbors, facetT);
    }else {
      vertexA= SETsecondt_(facet->vertices, vertexT);
      vertexB= SETfirstt_(facet->vertices, vertexT);
      nextfacet= SETsecondt_(facet->neighbors, facetT);
    }
    if (facet->visitid == qh visit_id) {
      ivp_message( "qh_printextremes_2d: loop in facet list.  facet %d nextfacet %d\n",
                 facet->id, nextfacet->id);
      qh_errexit2 (qh_ERRqhull, facet, nextfacet);
    }
    if (facet->visitid) {
      if (vertexA->visitid != qh vertex_visit) {
	vertexA->visitid= qh vertex_visit;
	fprintf(fp, "%d\n", qh_pointid (vertexA->point));
      }
      if (vertexB->visitid != qh vertex_visit) {
	vertexB->visitid= qh vertex_visit;
	fprintf(fp, "%d\n", qh_pointid (vertexB->point));
      }
    }
    facet->visitid= qh visit_id;
    facet= nextfacet;
  }while (facet && facet != startfacet);
} /* printextremes_2d */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printextremes_d">-</a>
  
  qh_printextremes_d( fp, facetlist, facets, printall )
    print extreme points of input sites for Delaunay triangulations

  notes:
    #points, followed by ids, one per line
    
    unordered
*/
void qh_printextremes_d (FILE *fp, facetT *facetlist, setT *facets, int printall) {
  setT *vertices;
  vertexT *vertex, **vertexp;
  boolT upperseen, lowerseen;
  facetT *neighbor, **neighborp;
  int numpoints=0;

  vertices= qh_facetvertices (facetlist, facets, printall);
  qh_vertexneighbors();
  FOREACHvertex_(vertices) {
    upperseen= lowerseen= False;
    FOREACHneighbor_(vertex) {
      if (neighbor->upperdelaunay)
        upperseen= True;
      else
        lowerseen= True;
    }
    if (upperseen && lowerseen) {
      vertex->seen= True;
      numpoints++;
    }else
      vertex->seen= False;
  }
  fprintf (fp, "%d\n", numpoints);
  FOREACHvertex_(vertices) {
    if (vertex->seen)
      fprintf (fp, "%d\n", qh_pointid (vertex->point));
  }
  qh_settempfree (&vertices);
} /* printextremes_d */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet">-</a>
  
  qh_printfacet( fp, facet )
    prints all fields of a facet to fp

  notes:
    ridges printed in neighbor order
*/
void qh_printfacet(FILE *fp, facetT *facet) {

  qh_printfacetheader (fp, facet);
  if (facet->ridges)
    qh_printfacetridges (fp, facet);
} /* printfacet */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet2geom">-</a>
  
  qh_printfacet2geom( fp, facet, color )
    print facet as part of a 2-d VECT for Geomview
  
    notes:
      assume precise calculations in io.c with roundoff covered by qh_GEOMepsilon
      mindist is calculated within io.c.  maxoutside is calculated elsewhere
      so a DISTround error may have occured.
*/
void qh_printfacet2geom(FILE *fp, facetT *facet, realT color[3]) {
  pointT *point0, *point1;
  realT mindist, innerplane, outerplane;
  int k;

  qh_facet2point (facet, &point0, &point1, &mindist);
  qh_geomplanes (facet, &outerplane, &innerplane);
  if (qh PRINTouter || (!qh PRINTnoplanes && !qh PRINTinner))
    qh_printfacet2geom_points(fp, point0, point1, facet, outerplane, color);
  if (qh PRINTinner || (!qh PRINTnoplanes && !qh PRINTouter &&
                outerplane - innerplane > 2 * qh MAXabs_coord * qh_GEOMepsilon)) {
    for(k= 3; k--; )
      color[k]= 1.0 - color[k];
    qh_printfacet2geom_points(fp, point0, point1, facet, innerplane, color);
  }
  qh_memfree (point1, qh normal_size);
  qh_memfree (point0, qh normal_size); 
} /* printfacet2geom */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet2geom_points">-</a>
  
  qh_printfacet2geom_points( fp, point1, point2, facet, offset, color )
    prints a 2-d facet as a VECT with 2 points at some offset.   
    The points are on the facet's plane.
*/
void qh_printfacet2geom_points(FILE *fp, pointT *point1, pointT *point2,
			       facetT *facet, realT offset, realT color[3]) {
  pointT *p1= point1, *p2= point2;
	if (!fp) return;
  fprintf(fp, "VECT 1 2 1 2 1 # f%d\n", facet->id);
  if (offset != 0.0) {
    p1= qh_projectpoint (p1, facet, -offset);
    p2= qh_projectpoint (p2, facet, -offset);
  }
  fprintf(fp, "%8.4g %8.4g %8.4g\n%8.4g %8.4g %8.4g\n",
           p1[0], p1[1], 0.0, p2[0], p2[1], 0.0);
  if (offset != 0.0) {
    qh_memfree (p1, qh normal_size);
    qh_memfree (p2, qh normal_size);
  }
  fprintf(fp, "%8.4g %8.4g %8.4g 1.0\n", color[0], color[1], color[2]);
} /* printfacet2geom_points */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet2math">-</a>
  
  qh_printfacet2math( fp, facet, notfirst )
    print 2-d Mathematica output for a facet
    may be non-simplicial

  notes:
    use %16.8f since Mathematica 2.2 does not handle exponential format
*/
void qh_printfacet2math(FILE *fp, facetT *facet, int notfirst) {
  pointT *point0, *point1;
  realT mindist;
	if (!fp) return;  
  qh_facet2point (facet, &point0, &point1, &mindist);
  if (notfirst)
    fprintf(fp, ",");
  fprintf(fp, "Line[{{%16.8f, %16.8f}, {%16.8f, %16.8f}}]\n",
	  point0[0], point0[1], point1[0], point1[1]);
  qh_memfree (point1, qh normal_size);
  qh_memfree (point0, qh normal_size);
} /* printfacet2math */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet3geom_nonsimplicial">-</a>
  
  qh_printfacet3geom_nonsimplicial( fp, facet, color )
    print Geomview OFF for a 3-d nonsimplicial facet.
    if DOintersections, prints ridges to unvisited neighbors (qh visit_id) 

  notes
    uses facet->visitid for intersections and ridges
*/
void qh_printfacet3geom_nonsimplicial(FILE *fp, facetT *facet, realT color[3]) {
  ridgeT *ridge, **ridgep;
  setT *projectedpoints, *vertices;
  vertexT *vertex, **vertexp, *vertexA, *vertexB;
  pointT *projpt, *point, **pointp;
  facetT *neighbor;
  realT dist, outerplane, innerplane;
  int cntvertices, k;
  realT black[3]={0, 0, 0}, green[3]={0, 1, 0};

  qh_geomplanes (facet, &outerplane, &innerplane); 
  vertices= qh_facet3vertex (facet); /* oriented */
  cntvertices= qh_setsize(vertices);
  projectedpoints= qh_settemp(cntvertices);
  FOREACHvertex_(vertices) {
    zinc_(Zdistio);
    qh_distplane(vertex->point, facet, &dist);
    projpt= qh_projectpoint(vertex->point, facet, dist);
    qh_setappend (&projectedpoints, projpt);
  }
  if (qh PRINTouter || (!qh PRINTnoplanes && !qh PRINTinner))
    qh_printfacet3geom_points(fp, projectedpoints, facet, outerplane, color);
  if (qh PRINTinner || (!qh PRINTnoplanes && !qh PRINTouter &&
                outerplane - innerplane > 2 * qh MAXabs_coord * qh_GEOMepsilon)) {
    for (k=3; k--; )
      color[k]= 1.0 - color[k];
    qh_printfacet3geom_points(fp, projectedpoints, facet, innerplane, color);
  }
  FOREACHpoint_(projectedpoints)
    qh_memfree (point, qh normal_size);
  qh_settempfree(&projectedpoints);
  qh_settempfree(&vertices);
  if ((qh DOintersections || qh PRINTridges)
  && (!facet->visible || !qh NEWfacets)) {
    facet->visitid= qh visit_id;
    FOREACHridge_(facet->ridges) {
      neighbor= otherfacet_(ridge, facet);
      if (neighbor->visitid != qh visit_id) {
        if (qh DOintersections)
          qh_printhyperplaneintersection(fp, facet, neighbor, ridge->vertices, black);
        if (qh PRINTridges) {
          vertexA= SETfirstt_(ridge->vertices, vertexT);
          vertexB= SETsecondt_(ridge->vertices, vertexT);
          qh_printline3geom (fp, vertexA->point, vertexB->point, green);
        }
      }
    }
  }
} /* printfacet3geom_nonsimplicial */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet3geom_points">-</a>
  
  qh_printfacet3geom_points( fp, points, facet, offset )
    prints a 3-d facet as OFF Geomview object. 
    offset is relative to the facet's hyperplane
    Facet is determined as a list of points
*/
void qh_printfacet3geom_points(FILE *fp, setT *points, facetT *facet, realT offset, realT color[3]) {
  int k, n= qh_setsize(points), i;
  pointT *point, **pointp;
  setT *printpoints;
	if (!fp) return;
  fprintf(fp, "{ OFF %d 1 1 # f%d\n", n, facet->id);
  if (offset != 0.0) {
    printpoints= qh_settemp (n);
    FOREACHpoint_(points) 
      qh_setappend (&printpoints, qh_projectpoint(point, facet, -offset));
  }else
    printpoints= points;
  FOREACHpoint_(printpoints) {
    for (k=0; k < qh hull_dim; k++) {
      if (k == qh DROPdim)
        fprintf(fp, "0 ");
      else
        fprintf(fp, "%8.4g ", point[k]);
    }
    if (printpoints != points)
      qh_memfree (point, qh normal_size);
    fprintf (fp, "\n");
  }
  if (printpoints != points)
    qh_settempfree (&printpoints);
  fprintf(fp, "%d ", n);
  for(i= 0; i < n; i++)
    fprintf(fp, "%d ", i);
  fprintf(fp, "%8.4g %8.4g %8.4g 1.0 }\n", color[0], color[1], color[2]);
} /* printfacet3geom_points */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet3geom_simplicial">-</a>
  
  qh_printfacet3geom_simplicial(  )
    print Geomview OFF for a 3-d simplicial facet.

  notes:
    may flip color
    uses facet->visitid for intersections and ridges

    assume precise calculations in io.c with roundoff covered by qh_GEOMepsilon
    innerplane may be off by qh DISTround.  Maxoutside is calculated elsewhere
    so a DISTround error may have occured.
*/
void qh_printfacet3geom_simplicial(FILE *fp, facetT *facet, realT color[3]) {
  setT *points, *vertices;
  vertexT *vertex, **vertexp, *vertexA, *vertexB;
  facetT *neighbor, **neighborp;
  realT outerplane, innerplane;
  realT black[3]={0, 0, 0}, green[3]={0, 1, 0};
  int k;

  qh_geomplanes (facet, &outerplane, &innerplane); 
  vertices= qh_facet3vertex (facet);
  points= qh_settemp (qh TEMPsize);
  FOREACHvertex_(vertices)
    qh_setappend(&points, vertex->point);
  if (qh PRINTouter || (!qh PRINTnoplanes && !qh PRINTinner))
    qh_printfacet3geom_points(fp, points, facet, outerplane, color);
  if (qh PRINTinner || (!qh PRINTnoplanes && !qh PRINTouter &&
              outerplane - innerplane > 2 * qh MAXabs_coord * qh_GEOMepsilon)) {
    for (k= 3; k--; )
      color[k]= 1.0 - color[k];
    qh_printfacet3geom_points(fp, points, facet, innerplane, color);
  }
  qh_settempfree(&points);
  qh_settempfree(&vertices);
  if ((qh DOintersections || qh PRINTridges)
  && (!facet->visible || !qh NEWfacets)) {
    facet->visitid= qh visit_id;
    FOREACHneighbor_(facet) {
      if (neighbor->visitid != qh visit_id) {
	vertices= qh_setnew_delnthsorted (facet->vertices, qh hull_dim,
	                  SETindex_(facet->neighbors, neighbor), 0);
        if (qh DOintersections)
	   qh_printhyperplaneintersection(fp, facet, neighbor, vertices, black); 
        if (qh PRINTridges) {
          vertexA= SETfirstt_(vertices, vertexT);
          vertexB= SETsecondt_(vertices, vertexT);
          qh_printline3geom (fp, vertexA->point, vertexB->point, green);
        }
	qh_setfree(&vertices);
      }
    }
  }
} /* printfacet3geom_simplicial */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet3math">-</a>
  
  qh_printfacet3math( fp, facet, notfirst )
    print 3-d Mathematica output for a facet

  notes:
    may be non-simplicial
    use %16.8f since Mathematica 2.2 does not handle exponential format
*/
void qh_printfacet3math (FILE *fp, facetT *facet, int notfirst) {
  vertexT *vertex, **vertexp;
  setT *points, *vertices;
  pointT *point, **pointp;
  boolT firstpoint= True;
  realT dist;
	if (!fp) return;  
  if (notfirst)
    fprintf(fp, ",\n");
  vertices= qh_facet3vertex (facet);
  points= qh_settemp (qh_setsize (vertices));
  FOREACHvertex_(vertices) {
    zinc_(Zdistio);
    qh_distplane(vertex->point, facet, &dist);
    point= qh_projectpoint(vertex->point, facet, dist);
    qh_setappend (&points, point);
  }
  fprintf(fp, "Polygon[{");
  FOREACHpoint_(points) {
    if (firstpoint)
      firstpoint= False;
    else
      fprintf(fp, ",\n");
    fprintf(fp, "{%16.8f, %16.8f, %16.8f}", point[0], point[1], point[2]);
  }
  FOREACHpoint_(points)
    qh_memfree (point, qh normal_size);
  qh_settempfree(&points);
  qh_settempfree(&vertices);
  fprintf(fp, "}]");
} /* printfacet3math */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet3vertex">-</a>
  
  qh_printfacet3vertex( fp, facet, format )
    print vertices in a 3-d facet as point ids

  notes:
    prints number of vertices first if format == qh_PRINToff
    the facet may be non-simplicial
*/
void qh_printfacet3vertex(FILE *fp, facetT *facet, int format) {
  vertexT *vertex, **vertexp;
  setT *vertices;
	if (!fp) return;
  vertices= qh_facet3vertex (facet);
  if (format == qh_PRINToff)
    fprintf (fp, "%d ", qh_setsize (vertices));
  FOREACHvertex_(vertices) 
    fprintf (fp, "%d ", qh_pointid(vertex->point));
  fprintf (fp, "\n");
  qh_settempfree(&vertices);
} /* printfacet3vertex */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet4geom_nonsimplicial">-</a>
  
  qh_printfacet4geom_nonsimplicial(  )
    print Geomview 4OFF file for a 4d nonsimplicial facet
    prints all ridges to unvisited neighbors (qh.visit_id)
    if qh.DROPdim
      prints in OFF format
  
  notes:
    must agree with printend4geom()
*/
void qh_printfacet4geom_nonsimplicial(FILE *fp, facetT *facet, realT color[3]) {
  facetT *neighbor;
  ridgeT *ridge, **ridgep;
  vertexT *vertex, **vertexp;
  pointT *point;
  int k;
  realT dist;
	if (!fp) return;  
  facet->visitid= qh visit_id;
  if (qh PRINTnoplanes || (facet->visible && qh NEWfacets))
    return;
  FOREACHridge_(facet->ridges) {
    neighbor= otherfacet_(ridge, facet);
    if (neighbor->visitid == qh visit_id) 
      continue;
    if (qh PRINTtransparent && !neighbor->good)
      continue;  
    if (qh DOintersections)
      qh_printhyperplaneintersection(fp, facet, neighbor, ridge->vertices, color);
    else {
      if (qh DROPdim >= 0) 
	fprintf(fp, "OFF 3 1 1 # f%d\n", facet->id);
      else {
	qh printoutvar++;
	fprintf (fp, "# r%d between f%d f%d\n", ridge->id, facet->id, neighbor->id);
      }
      FOREACHvertex_(ridge->vertices) {
	zinc_(Zdistio);
	qh_distplane(vertex->point,facet, &dist);
	point=qh_projectpoint(vertex->point,facet, dist);
	for(k= 0; k < qh hull_dim; k++) {
	  if (k != qh DROPdim)
  	    fprintf(fp, "%8.4g ", point[k]);
  	}
	fprintf (fp, "\n");
	qh_memfree (point, qh normal_size);
      }
      if (qh DROPdim >= 0)
        fprintf(fp, "3 0 1 2 %8.4g %8.4g %8.4g\n", color[0], color[1], color[2]);
    }
  }
} /* printfacet4geom_nonsimplicial */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacet4geom_simplicial">-</a>
  
  qh_printfacet4geom_simplicial( fp, facet, color )
    print Geomview 4OFF file for a 4d simplicial facet
    prints triangles for unvisited neighbors (qh.visit_id)

  notes:
    must agree with printend4geom()
*/
void qh_printfacet4geom_simplicial(FILE *fp, facetT *facet, realT color[3]) {
  setT *vertices;
  facetT *neighbor, **neighborp;
  vertexT *vertex, **vertexp;
  int k;
	if (!fp) return;  
  
  facet->visitid= qh visit_id;
  if (qh PRINTnoplanes || (facet->visible && qh NEWfacets))
    return;
  FOREACHneighbor_(facet) {
    if (neighbor->visitid == qh visit_id)
      continue;
    if (qh PRINTtransparent && !neighbor->good)
      continue;  
    vertices= qh_setnew_delnthsorted (facet->vertices, qh hull_dim,
	                  SETindex_(facet->neighbors, neighbor), 0);
    if (qh DOintersections)
      qh_printhyperplaneintersection(fp, facet, neighbor, vertices, color);
    else {
      if (qh DROPdim >= 0) 
	fprintf(fp, "OFF 3 1 1 # ridge between f%d f%d\n",
		facet->id, neighbor->id);
      else {
	qh printoutvar++;
	fprintf (fp, "# ridge between f%d f%d\n", facet->id, neighbor->id);
      }
      FOREACHvertex_(vertices) {
	for(k= 0; k < qh hull_dim; k++) {
	  if (k != qh DROPdim)
  	    fprintf(fp, "%8.4g ", vertex->point[k]);
  	}
	fprintf (fp, "\n");
      }
      if (qh DROPdim >= 0) 
        fprintf(fp, "3 0 1 2 %8.4g %8.4g %8.4g\n", color[0], color[1], color[2]);
    }
    qh_setfree(&vertices);
  }
} /* printfacet4geom_simplicial */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacetNvertex_nonsimplicial">-</a>
  
  qh_printfacetNvertex_nonsimplicial( fp, facet, id, format )
    print vertices for an N-d non-simplicial facet
    triangulates each ridge to the id
*/
void qh_printfacetNvertex_nonsimplicial(FILE *fp, facetT *facet, int id, int format) {
  vertexT *vertex, **vertexp;
  ridgeT *ridge, **ridgep;
	if (!fp) return;  

  if (facet->visible && qh NEWfacets)
    return;
  FOREACHridge_(facet->ridges) {
    if (format == qh_PRINTtriangles)
      fprintf(fp, "%d ", qh hull_dim);
    fprintf(fp, "%d ", id);
    if ((ridge->top == facet) ^ qh_ORIENTclock) {
      FOREACHvertex_(ridge->vertices)
        fprintf(fp, "%d ", qh_pointid(vertex->point));
    }else {
      FOREACHvertexreverse12_(ridge->vertices)
        fprintf(fp, "%d ", qh_pointid(vertex->point));
    }
    fprintf(fp, "\n");
  }
} /* printfacetNvertex_nonsimplicial */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacetNvertex_simplicial">-</a>
  
  qh_printfacetNvertex_simplicial( fp, facet, format )
    print vertices for an N-d simplicial facet
    prints vertices for non-simplicial facets
      2-d facets (orientation preserved by qh_mergefacet2d)
      PRINToff ('o') for 4-d and higher
*/
void qh_printfacetNvertex_simplicial(FILE *fp, facetT *facet, int format) {
  vertexT *vertex, **vertexp;
	if (!fp) return;  
  
  if (format == qh_PRINToff || format == qh_PRINTtriangles)
    fprintf (fp, "%d ", qh_setsize (facet->vertices));
  if ((facet->toporient ^ qh_ORIENTclock) 
  || (qh hull_dim > 2 && !facet->simplicial)) {
    FOREACHvertex_(facet->vertices)
      fprintf(fp, "%d ", qh_pointid(vertex->point));
  }else {
    FOREACHvertexreverse12_(facet->vertices)
      fprintf(fp, "%d ", qh_pointid(vertex->point));
  }
  fprintf(fp, "\n");
} /* printfacetNvertex_simplicial */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacetheader">-</a>
  
  qh_printfacetheader( fp, facet )
    prints header fields of a facet to fp
    
  notes:
    for 'f' output and debugging
*/
void qh_printfacetheader(FILE *fp, facetT *facet) {
  pointT *point, **pointp, *furthest;
  facetT *neighbor, **neighborp;
  realT dist;
	if (!fp) return;  

  if (facet == qh_MERGEridge) {
    fprintf (fp, " MERGEridge\n");
    return;
  }else if (facet == qh_DUPLICATEridge) {
    fprintf (fp, " DUPLICATEridge\n");
    return;
  }else if (!facet) {
    fprintf (fp, " NULLfacet\n");
    return;
  }
  qh old_randomdist= qh RANDOMdist;
  qh RANDOMdist= False;
  fprintf(fp, "- f%d\n", facet->id);
  fprintf(fp, "    - flags:");
  if (facet->toporient) 
    fprintf(fp, " top");
  else
    fprintf(fp, " bottom");
  if (facet->simplicial)
    fprintf(fp, " simplicial");
  if (facet->upperdelaunay)
    fprintf(fp, " upperDelaunay");
  if (facet->visible)
    fprintf(fp, " visible");
  if (facet->newfacet)
    fprintf(fp, " new");
  if (facet->tested)
    fprintf(fp, " tested");
  if (!facet->good)
    fprintf(fp, " notG");
  if (facet->seen)
    fprintf(fp, " seen");
  if (facet->coplanar)
    fprintf(fp, " coplanar");
  if (facet->mergehorizon)
    fprintf(fp, " mergehorizon");
  if (facet->keepcentrum)
    fprintf(fp, " keepcentrum");
  if (facet->dupridge)
    fprintf(fp, " dupridge");
  if (facet->mergeridge && !facet->mergeridge2)
    fprintf(fp, " mergeridge1");
  if (facet->mergeridge2)
    fprintf(fp, " mergeridge2");
  if (facet->newmerge)
    fprintf(fp, " newmerge");
  if (facet->flipped) 
    fprintf(fp, " flipped");
  if (facet->notfurthest) 
    fprintf(fp, " notfurthest");
  if (facet->degenerate)
    fprintf(fp, " degenerate");
  if (facet->redundant)
    fprintf(fp, " redundant");
  fprintf(fp, "\n");
  if (facet->isarea)
    fprintf(fp, "    - area: %2.2g\n", facet->f.area);
  else if (qh NEWfacets && facet->visible && facet->f.replace)
    fprintf(fp, "    - replacement: f%d\n", facet->f.replace->id);
  else if (facet->newfacet) {
    if (facet->f.samecycle && facet->f.samecycle != facet)
      fprintf(fp, "    - shares same visible/horizon as f%d\n", facet->f.samecycle->id);
  }else if (facet->f.newcycle)
    fprintf(fp, "    - was horizon to f%d\n", facet->f.newcycle->id);
  if (facet->nummerge)
    fprintf(fp, "    - merges: %d\n", facet->nummerge);
  qh_printpointid(fp, "    - normal: ", qh hull_dim, facet->normal, -1);
  fprintf(fp, "    - offset: %10.7g\n", facet->offset);
  if (qh CENTERtype == qh_ASvoronoi || facet->center)
    qh_printcenter (fp, qh_PRINTfacets, "    - center: ", facet);
#if qh_MAXoutside
  if (facet->maxoutside > qh DISTround)
    fprintf(fp, "    - maxoutside: %10.7g\n", facet->maxoutside);
#endif
  if (!SETempty_(facet->outsideset)) {
    furthest= (pointT*)qh_setlast(facet->outsideset);
    if (qh_setsize (facet->outsideset) < 6) {
      fprintf(fp, "    - outside set (furthest p%d):\n", qh_pointid(furthest));
      FOREACHpoint_(facet->outsideset)
	qh_printpoint(fp, "     ", point);
    }else if (qh_setsize (facet->outsideset) < 21) {
      qh_printpoints(fp, "    - outside set:", facet->outsideset);
    }else {
      fprintf(fp, "    - outside set:  %d points.", qh_setsize(facet->outsideset));
      qh_printpoint(fp, "  Furthest", furthest);
    }
#if !qh_COMPUTEfurthest
    fprintf(fp, "    - furthest distance= %2.2g\n", facet->furthestdist);
#endif
  }
  if (!SETempty_(facet->coplanarset)) {
    furthest= (pointT*)qh_setlast(facet->coplanarset);
    if (qh_setsize (facet->coplanarset) < 6) {
      fprintf(fp, "    - coplanar set (furthest p%d):\n", qh_pointid(furthest));
      FOREACHpoint_(facet->coplanarset)
	qh_printpoint(fp, "     ", point);
    }else if (qh_setsize (facet->coplanarset) < 21) {
      qh_printpoints(fp, "    - coplanar set:", facet->coplanarset);
    }else {
      fprintf(fp, "    - coplanar set:  %d points.", qh_setsize(facet->coplanarset));
      qh_printpoint(fp, "  Furthest", furthest);
    }
    zinc_(Zdistio);
    qh_distplane (furthest, facet, &dist);
    fprintf(fp, "      furthest distance= %2.2g\n", dist);
  }
  qh_printvertices (fp, "    - vertices:", facet->vertices);
  fprintf(fp, "    - neighboring facets: ");
  FOREACHneighbor_(facet) {
    if (neighbor == qh_MERGEridge)
      fprintf(fp, " MERGE");
    else if (neighbor == qh_DUPLICATEridge)
      fprintf(fp, " DUP");
    else
      fprintf(fp, " f%d", neighbor->id);
  }
  fprintf(fp, "\n");
  qh RANDOMdist= qh old_randomdist;
} /* printfacetheader */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacetridges">-</a>
  
  qh_printfacetridges( fp, facet )
    prints ridges of a facet to fp

  notes:
    ridges printed in neighbor order
    assumes the ridges exist
    for 'f' output
*/
void qh_printfacetridges(FILE *fp, facetT *facet) {
  facetT *neighbor, **neighborp;
  ridgeT *ridge, **ridgep;
  int numridges= 0;

	if (!fp) return;  

  if (facet->visible && qh NEWfacets) {
    fprintf(fp, "    - ridges (ids may be garbage):");
    FOREACHridge_(facet->ridges)
      fprintf(fp, " r%d", ridge->id);
    fprintf(fp, "\n");
  }else {
    fprintf(fp, "    - ridges:\n");
    FOREACHridge_(facet->ridges)
      ridge->seen= False;
    if (qh hull_dim == 3) {
      ridge= SETfirstt_(facet->ridges, ridgeT);
      while (ridge && !ridge->seen) {
	ridge->seen= True;
	qh_printridge(fp, ridge);
	numridges++;
	ridge= qh_nextridge3d (ridge, facet, NULL);
	}
    }else {
      FOREACHneighbor_(facet) {
	FOREACHridge_(facet->ridges) {
	  if (otherfacet_(ridge,facet) == neighbor) {
	    ridge->seen= True;
	    qh_printridge(fp, ridge);
	    numridges++;
	  }
	}
      }
    }
    if (numridges != qh_setsize (facet->ridges)) {
      fprintf (fp, "     - all ridges:");
      FOREACHridge_(facet->ridges) 
	fprintf (fp, " r%d", ridge->id);
        fprintf (fp, "\n");
    }
    FOREACHridge_(facet->ridges) {
      if (!ridge->seen) 
	qh_printridge(fp, ridge);
    }
  }
} /* printfacetridges */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printfacets">-</a>
  
  qh_printfacets( fp, format, facetlist, facets, printall )
    prints facetlist and/or facet set in output format
  
  notes:
    also used for specialized formats ('FO' and summary)
    turns off 'Rn' option since want actual numbers
*/
void qh_printfacets(FILE *fp, int format, facetT *facetlist, setT *facets, boolT printall) {
  int numfacets, numsimplicial, numridges, totneighbors, numcoplanars;
  facetT *facet, **facetp;
  setT *vertices;
  coordT *center;
  realT outerplane, innerplane;

	if (!fp) return;  
  qh old_randomdist= qh RANDOMdist;
  qh RANDOMdist= False;
  if (qh CDDoutput && (format == qh_PRINTcentrums || format == qh_PRINTpointintersect || format == qh_PRINToff))
    ivp_message( "qhull warning: CDD format is not available for centrums, halfspace\nintersections, and OFF file format.\n");
  if (format == qh_PRINTnone)
    ; /* print nothing */
  else if (format == qh_PRINTaverage) {
    vertices= qh_facetvertices (facetlist, facets, printall);
    center= qh_getcenter (vertices);
    fprintf (fp, "%d 1\n", qh hull_dim);
    qh_printpointid (fp, NULL, qh hull_dim, center, -1);
    qh_memfree (center, qh normal_size);
    qh_settempfree (&vertices);
  }else if (format == qh_PRINTextremes) {
    if (qh DELAUNAY)
      qh_printextremes_d (fp, facetlist, facets, printall);
    else if (qh hull_dim == 2)
      qh_printextremes_2d (fp, facetlist, facets, printall);
    else 
      qh_printextremes (fp, facetlist, facets, printall);
  }else if (format == qh_PRINToptions)
    fprintf(fp, "Options selected for qhull %s:\n%s\n", qh_version, qh qhull_options);
  else if (format == qh_PRINTpoints && !qh VORONOI)
    qh_printpoints_out (fp, facetlist, facets, printall);
  else if (format == qh_PRINTqhull)
    fprintf (fp, "%s | %s\n", qh rbox_command, qh qhull_command);
  else if (format == qh_PRINTsize) {
    fprintf (fp, "0\n2 ");
    fprintf (fp, qh_REAL_1, qh totarea);
    fprintf (fp, qh_REAL_1, qh totvol);
    fprintf (fp, "\n");
  }else if (format == qh_PRINTsummary) {
    qh_countfacets (facetlist, facets, printall, &numfacets, &numsimplicial, 
      &totneighbors, &numridges, &numcoplanars);
    vertices= qh_facetvertices (facetlist, facets, printall); 
    fprintf (fp, "7 %d %d %d %d %d %d %d\n2 ", qh hull_dim, 
                qh num_points + qh_setsize (qh other_points),
                qh num_vertices, qh num_facets - qh num_visible,
                qh_setsize (vertices), numfacets, numcoplanars);
    qh_settempfree (&vertices);
    qh_outerinner (NULL, &outerplane, &innerplane);
    fprintf (fp, qh_REAL_2n, outerplane, innerplane);
  }else if (format == qh_PRINTvneighbors)
    qh_printvneighbors (fp, facetlist, facets, printall);
  else if (qh VORONOI && format == qh_PRINToff)
    qh_printvoronoi (fp, format, facetlist, facets, printall);
  else if (qh VORONOI && format == qh_PRINTgeom) {
    qh_printbegin (fp, format, facetlist, facets, printall);
    qh_printvoronoi (fp, format, facetlist, facets, printall);
    qh_printend (fp, format, facetlist, facets, printall);
  }else if (qh VORONOI 
  && (format == qh_PRINTvertices || format == qh_PRINTinner || format == qh_PRINTouter))
    qh_printvdiagram (fp, format, facetlist, facets, printall);
  else {
    qh_printbegin (fp, format, facetlist, facets, printall);
    FORALLfacet_(facetlist)
      qh_printafacet (fp, format, facet, printall);
    FOREACHfacet_(facets) 
      qh_printafacet (fp, format, facet, printall);
    qh_printend (fp, format, facetlist, facets, printall);
  }
  qh RANDOMdist= qh old_randomdist;
} /* printfacets */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printhelp_degenerate">-</a>
  
  qh_printhelp_degenerate( fp )
    prints descriptive message for precision error

  notes:
    no message if qh_QUICKhelp
*/
void qh_printhelp_degenerate(FILE *fp) {
  
  if (qh MERGEexact || qh PREmerge || qh JOGGLEmax < REALmax/2) 
    fprintf(fp, "\n\
A Qhull error has occurred.  Qhull should have corrected the above\n\
precision error.  Please send the input and all of the output to\n\
qhull_bug@geom.umn.edu\n");
  else if (!qh_QUICKhelp) {
    fprintf(fp, "\n\
Precision problems were detected during construction of the convex hull.\n\
This occurs because convex hull algorithms assume that calculations are\n\
exact, but floating-point arithmetic has roundoff errors.\n\
\n\
To correct for precision problems, do not use 'Q0'.  By default, Qhull\n\
selects 'C-0' or 'Qx' and merges non-convex facets.  With option 'QJ',\n\
Qhull joggles the input to prevent precision problems.  See \"Imprecision\n\
in Qhull\" (qh-impre.htm).\n\
\n\
If you use 'Q0', the output may include\n\
coplanar ridges, concave ridges, and flipped facets.  In 4-d and higher,\n\
Qhull may produce a ridge with four neighbors or two facets with the same \n\
vertices.  Qhull reports these events when they occur.  It stops when a\n\
concave ridge, flipped facet, or duplicate facet occurs.\n");
#if REALfloat
    fprintf (fp, "\
\n\
Qhull is currently using single precision arithmetic.  The following\n\
will probably remove the precision problems:\n\
  - recompile qhull for double precision (#define REALfloat 0 in user.h).\n");
#endif
    if (qh DELAUNAY && !qh SCALElast && qh MAXabs_coord > 1e4)
      fprintf( fp, "\
\n\
When computing the Delaunay triangulation of coordinates > 1.0,\n\
  - use 'Qbb' to scale the last coordinate to [0,m] (max previous coordinate)\n");
    if (qh DELAUNAY && !qh ATinfinity) 
      fprintf( fp, "\
When computing the Delaunay triangulation:\n\
  - use 'Qz' to add a point at-infinity.  This reduces precision problems.\n");
 
    fprintf(fp, "\
\n\
If you need triangular output:\n\
  - use option 'QJ' to joggle the input points and remove precision errors\n\
  - or use option 'Ft' instead of 'Q0'.  It triangulates non-simplicial\n\
    facets with added points.\n\
\n\
If you must use 'Q0',\n\
try one or more of the following options.  They can not guarantee an output.\n\
  - use 'QbB' to scale the input to a cube.\n\
  - use 'Po' to produce output and prevent partitioning for flipped facets\n\
  - use 'V0' to set min. distance to visible facet as 0 instead of roundoff\n\
  - use 'En' to specify a maximum roundoff error less than %2.2g.\n\
  - options 'Qf', 'Qbb', and 'QR0' may also help\n",
               qh DISTround);
    fprintf(fp, "\
\n\
To guarantee simplicial output:\n\
  - use option 'QJ' to joggle the input points and remove precision errors\n\
  - use option 'Ft' to triangulate the output by adding points\n\
  - use exact arithmetic (see \"Imprecision in Qhull\", qh-impre.htm)\n\
");
  }
} /* printhelp_degenerate */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printhelp_singular">-</a>
  
  qh_printhelp_singular( fp )
    prints descriptive message for singular input
*/
void qh_printhelp_singular(FILE *fp) {
  facetT *facet;
  vertexT *vertex, **vertexp;
  realT min, max, *coord, dist;
  int i,k;
  
	if (!fp) return;  
  fprintf(fp, "\n\
The input to qhull appears to be less than %d dimensional, or a\n\
computation has overflowed.\n\n\
Qhull could not construct a clearly convex simplex from points:\n",
           qh hull_dim);
  qh_printvertexlist (fp, "", qh facet_list, NULL, qh_ALL);
  if (!qh_QUICKhelp)
    fprintf(fp, "\n\
The center point is coplanar with a facet, or a vertex is coplanar\n\
with a neighboring facet.  The maximum round off error for\n\
computing distances is %2.2g.  The center point, facets and distances\n\
to the center point are as follows:\n\n", qh DISTround);
  qh_printpointid (fp, "center point", qh hull_dim, qh interior_point, -1);
  fprintf (fp, "\n");
  FORALLfacets {
    fprintf (fp, "facet");
    FOREACHvertex_(facet->vertices)
      fprintf (fp, " p%d", qh_pointid(vertex->point));
    zinc_(Zdistio);
    qh_distplane(qh interior_point, facet, &dist);
    fprintf (fp, " distance= %4.2g\n", dist);
  }
  if (!qh_QUICKhelp) {
    if (qh HALFspace) 
      fprintf (fp, "\n\
These points are the dual of the given halfspaces.  They indicate that\n\
the intersection is degenerate.\n");
    fprintf (fp,"\n\
These points either have a maximum or minimum x-coordinate, or\n\
they maximize the determinant for k coordinates.  Trial points\n\
are first selected from points that maximize a coordinate.\n");
    if (qh hull_dim >= qh_INITIALmax)
      fprintf (fp, "\n\
Because of the high dimension, the min x-coordinate and max-coordinate\n\
points are used if the determinant is non-zero.  Option 'Qs' will\n\
do a better, though much slower, job.  Instead of 'Qs', you can change\n\
the points by randomly rotating the input with 'QR0'.\n");
  }
  fprintf (fp, "\nThe min and max coordinates for each dimension are:\n");
  for (k=0; k < qh hull_dim; k++) {
    min= REALmax;
    max= -REALmin;
    for (i=qh num_points, coord= qh first_point+k; i--; coord += qh hull_dim) {
      maximize_(max, *coord);
      minimize_(min, *coord);
    }
    fprintf (fp, "  %d:  %8.4g  %8.4g  difference= %4.4g\n", k, min, max, max-min);
  }
  if (!qh_QUICKhelp) {
    fprintf (fp, "\n\
If the input should be full dimensional, you have several options that\n\
may determine an initial simplex:\n\
  - use 'QJ'  to joggle the input and make it full dimensional\n\
  - use 'QbB' to scale the points to the unit cube\n\
  - use 'QR0' to randomly rotate the input for different maximum points\n\
  - use 'Qs'  to search all points for the initial simplex\n\
  - use 'En'  to specify a maximum roundoff error less than %2.2g.\n\
  - trace execution with 'T3' to see the determinant for each point.\n",
                     qh DISTround);
#if REALfloat
    fprintf (fp, "\
  - recompile qhull for double precision (#define REALfloat 0 in qhull.h).\n");
#endif
    fprintf (fp, "\n\
If the input is lower dimensional:\n\
  - use 'QJ' to joggle the input and make it full dimensional\n\
  - use 'Qbk:0Bk:0' to delete coordinate k from the input.  You should\n\
    pick the coordinate with the least range.  The hull will have the\n\
    correct topology.\n\
  - determine the flat containing the points, rotate the points\n\
    into a coordinate plane, and delete the other coordinates.\n\
  - add one or more points to make the input full dimensional.\n\
");
    if (qh DELAUNAY && !qh ATinfinity)
      fprintf (fp, "\n\n\
This is a Delaunay triangulation and the input is co-circular or co-spherical:\n\
  - use 'Qz' to add a point \"at infinity\" (i.e., above the paraboloid)\n\
  - or use 'QJ' to joggle the input and avoid co-circular data\n");
  }
} /* printhelp_singular */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printhyperplaneintersection">-</a>
  
  qh_printhyperplaneintersection( fp, facet1, facet2, vertices, color )
    print Geomview OFF or 4OFF for the intersection of two hyperplanes in 3-d or 4-d
*/
void qh_printhyperplaneintersection(FILE *fp, facetT *facet1, facetT *facet2,
		   setT *vertices, realT color[3]) {
  realT costheta, denominator, dist1, dist2, s, t, mindenom, p[4];
  vertexT *vertex, **vertexp;
  int i, k;
  boolT nearzero1, nearzero2;
  
	if (!fp) return;  

	costheta= qh_getangle(facet1->normal, facet2->normal);
  denominator= 1 - costheta * costheta;
  i= qh_setsize(vertices);
  if (qh hull_dim == 3)
    fprintf(fp, "VECT 1 %d 1 %d 1 ", i, i);
  else if (qh hull_dim == 4 && qh DROPdim >= 0) //-V112
    fprintf(fp, "OFF 3 1 1 ");
  else
    qh printoutvar++;
  fprintf (fp, "# intersect f%d f%d\n", facet1->id, facet2->id);
  mindenom= 1 / (10.0 * qh MAXabs_coord);
  FOREACHvertex_(vertices) {
    zadd_(Zdistio, 2);
    qh_distplane(vertex->point, facet1, &dist1);
    qh_distplane(vertex->point, facet2, &dist2);
    s= qh_divzero (-dist1 + costheta * dist2, denominator,mindenom,&nearzero1);
    t= qh_divzero (-dist2 + costheta * dist1, denominator,mindenom,&nearzero2);
    if (nearzero1 || nearzero2)
      s= t= 0.0;
    for(k= qh hull_dim; k--; )
      p[k]= vertex->point[k] + facet1->normal[k] * s + facet2->normal[k] * t;
    if (qh PRINTdim <= 3) {
      qh_projectdim3 (p, p);
      fprintf(fp, "%8.4g %8.4g %8.4g # ", p[0], p[1], p[2]);
    }else 
      fprintf(fp, "%8.4g %8.4g %8.4g %8.4g # ", p[0], p[1], p[2], p[3]);
    if (nearzero1+nearzero2)
      fprintf (fp, "p%d (coplanar facets)\n", qh_pointid (vertex->point));
    else
      fprintf (fp, "projected p%d\n", qh_pointid (vertex->point));
  }
  if (qh hull_dim == 3)
    fprintf(fp, "%8.4g %8.4g %8.4g 1.0\n", color[0], color[1], color[2]); 
  else if (qh hull_dim == 4 && qh DROPdim >= 0)   //-V112
    fprintf(fp, "3 0 1 2 %8.4g %8.4g %8.4g 1.0\n", color[0], color[1], color[2]);
} /* printhyperplaneintersection */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printline3geom">-</a>
  
  qh_printline3geom( fp, pointA, pointB, color )
    prints a line as a VECT
    prints 0's for qh.DROPdim
  
  notes:
    if pointA == pointB, 
      it's a 1 point VECT
*/
void qh_printline3geom (FILE *fp, pointT *pointA, pointT *pointB, realT color[3]) {
  int k;
  realT pA[4], pB[4];

	if (!fp) return;  
  qh_projectdim3(pointA, pA);
  qh_projectdim3(pointB, pB);
  if ((fabs(pA[0] - pB[0]) > 1e-3) || 
      (fabs(pA[1] - pB[1]) > 1e-3) || 
      (fabs(pA[2] - pB[2]) > 1e-3)) {
    fprintf (fp, "VECT 1 2 1 2 1\n");
    for (k= 0; k < 3; k++)
       fprintf (fp, "%8.4g ", pB[k]);
    fprintf (fp, " # p%d\n", qh_pointid (pointB));
  }else
    fprintf (fp, "VECT 1 1 1 1 1\n");
  for (k=0; k < 3; k++)
    fprintf (fp, "%8.4g ", pA[k]);
  fprintf (fp, " # p%d\n", qh_pointid (pointA));
  fprintf (fp, "%8.4g %8.4g %8.4g 1\n", color[0], color[1], color[2]);
}

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printneighborhood">-</a>
  
  qh_printneighborhood( fp, format, facetA, facetB, printall )
    print neighborhood of one or two facets

  notes:
    calls qh_findgood_all() 
    bumps qh.visit_id
*/
void qh_printneighborhood (FILE *fp, int format, facetT *facetA, facetT *facetB, boolT printall) {
  facetT *neighbor, **neighborp, *facet;
  setT *facets;

  if (format == qh_PRINTnone)
    return;
  qh_findgood_all (qh facet_list);
  if (facetA == facetB)
    facetB= NULL;
  facets= qh_settemp (2*(qh_setsize (facetA->neighbors)+1));
  qh visit_id++;
  for (facet= facetA; facet; facet= ((facet == facetA) ? facetB : 0)) {
    if (facet->visitid != qh visit_id) {
      facet->visitid= qh visit_id;
      qh_setappend (&facets, facet);
    }
    FOREACHneighbor_(facet) {
      if (neighbor->visitid == qh visit_id)
        continue;
      neighbor->visitid= qh visit_id;
      if (printall || !qh_skipfacet (neighbor))
        qh_setappend (&facets, neighbor);
    }
  }
  qh_printfacets (fp, format, NULL, facets, printall);
  qh_settempfree (&facets);
} /* printneighborhood */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printpoint">-</a>
  
  qh_printpoint( fp, string, point )
  qh_printpointid( fp, string, dim, point, id )
    prints the coordinates of a point

  returns:
    if string is defined
      prints 'string p%d' (skips p%d if id=-1)

  notes:
    nop if point is NULL
    prints id unless it is undefined (-1)
*/
void qh_printpoint(FILE *fp, const char *string, pointT *point) {
  int id= qh_pointid( point);

  qh_printpointid( fp, string, qh hull_dim, point, id);
} /* printpoint */

void qh_printpointid(FILE *fp, const char *string, int dim, pointT *point, int id) {
  int k;
  realT r; /*bug fix*/
	if (!fp) return;  
  
  if (!point)
    return;
  if (string) {
    fputs (string, fp);
   if (id != -1)
      fprintf(fp, " p%d: ", id);
  }
  for(k= dim; k--; ) {
    r= *point++;
    if (string)
      fprintf(fp, " %8.4g", r);
    else
      fprintf(fp, qh_REAL_1, r);
  }
  fprintf(fp, "\n");
} /* printpointid */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printpoint3">-</a>
  
  qh_printpoint3( fp, point )
    prints 2-d, 3-d, or 4-d point as Geomview 3-d coordinates
*/
void qh_printpoint3 (FILE *fp, pointT *point) {
  int k;
  realT p[4];
  
  qh_projectdim3 (point, p);
  for (k=0; k < 3; k++)
    fprintf (fp, "%8.4g ", p[k]);
  fprintf (fp, " # p%d\n", qh_pointid (point));
} /* printpoint3 */

/*----------------------------------------
-printpoints- print pointids for a set of points starting at index 
   see geom.c
*/

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printpoints_out">-</a>
  
  qh_printpoints_out( fp, facetlist, facets, printall )
    prints vertices, coplanar/inside points, for facets by their point coordinates
    allows qh.CDDoutput

  notes:
    same format as qhull input
    if no coplanar/interior points,
      same order as qh_printextremes
*/
void qh_printpoints_out (FILE *fp, facetT *facetlist, setT *facets, int printall) {
  int allpoints= qh num_points + qh_setsize (qh other_points);
  int numpoints=0, point_i, point_n;
  setT *vertices, *points;
  facetT *facet, **facetp;
  pointT *point, **pointp;
  vertexT *vertex, **vertexp;
  int id;

  points= qh_settemp (allpoints);
  qh_setzero (points, 0, allpoints);
  vertices= qh_facetvertices (facetlist, facets, printall);
  FOREACHvertex_(vertices) {
    id= qh_pointid (vertex->point);
    if (id >= 0)
      SETelem_(points, id)= vertex->point;
  }
  if (qh KEEPinside || qh KEEPcoplanar || qh KEEPnearinside) {
    FORALLfacet_(facetlist) {
      if (!printall && qh_skipfacet(facet))
        continue;
      FOREACHpoint_(facet->coplanarset) {
        id= qh_pointid (point);
        if (id >= 0)
          SETelem_(points, id)= point;
      }
    }
    FOREACHfacet_(facets) {
      if (!printall && qh_skipfacet(facet))
        continue;
      FOREACHpoint_(facet->coplanarset) {
        id= qh_pointid (point);
        if (id >= 0)
          SETelem_(points, id)= point;
      }
    }
  }
  qh_settempfree (&vertices);
  FOREACHpoint_i_(points) {
    if (point)
      numpoints++;
  }
  if (qh CDDoutput)
    fprintf (fp, "%s | %s\nbegin\n%d %d real\n", qh rbox_command,
             qh qhull_command, numpoints, qh hull_dim + 1);
  else
    fprintf (fp, "%d\n%d\n", qh hull_dim, numpoints);
  FOREACHpoint_i_(points) {
    if (point) {
      if (qh CDDoutput)
	fprintf (fp, "1 ");
      qh_printpoint (fp, NULL, point);
    }
  }
  if (qh CDDoutput)
    fprintf (fp, "end\n");
  qh_settempfree (&points);
} /* printpoints_out */
  

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printpointvect">-</a>
  
  qh_printpointvect( fp, point, normal, center, radius, color )
    prints a 2-d, 3-d, or 4-d point as 3-d VECT's relative to normal or to center point
*/
void qh_printpointvect (FILE *fp, pointT *point, coordT *normal, pointT *center, realT radius, realT color[3]) {
  realT diff[4], pointA[4];
  int k;
  
  for (k= qh hull_dim; k--; ) {
    if (center)
      diff[k]= point[k]-center[k];
    else if (normal) 
      diff[k]= normal[k];
    else
      diff[k]= 0;
  }
  if (center)
    qh_normalize2 (diff, qh hull_dim, True, NULL, NULL);
  for (k= qh hull_dim; k--; ) 
    pointA[k]= point[k]+diff[k] * radius;
  qh_printline3geom (fp, point, pointA, color);
} /* printpointvect */  

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printpointvect2">-</a>
  
  qh_printpointvect2( fp, point, normal, center, radius )
    prints a 2-d, 3-d, or 4-d point as 2 3-d VECT's for an imprecise point
*/
void qh_printpointvect2 (FILE *fp, pointT *point, coordT *normal, pointT *center, realT radius) {
  realT red[3]={1, 0, 0}, yellow[3]={1, 1, 0};

  qh_printpointvect (fp, point, normal, center, radius, red);
  qh_printpointvect (fp, point, normal, center, -radius, yellow);
} /* printpointvect2 */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printridge">-</a>
  
  qh_printridge( fp, ridge )
    prints the information in a ridge

  notes:
    for qh_printfacetridges()
*/
void qh_printridge(FILE *fp, ridgeT *ridge) {
	if (!fp) return;  
  
  fprintf(fp, "     - r%d", ridge->id);
  if (ridge->tested)
    fprintf (fp, " tested");
  if (ridge->nonconvex)
    fprintf (fp, " nonconvex");
  fprintf (fp, "\n");
  qh_printvertices (fp, "           vertices:", ridge->vertices);
  if (ridge->top && ridge->bottom)
    fprintf(fp, "           between f%d and f%d\n",
	    ridge->top->id, ridge->bottom->id);
} /* printridge */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printspheres">-</a>
  
  qh_printspheres( fp, vertices, radius )
    prints 3-d vertices as OFF spheres

  notes:
    inflated octahedron from Stuart Levy earth/mksphere2
*/
void qh_printspheres(FILE *fp, setT *vertices, realT radius) {
  vertexT *vertex, **vertexp;

	if (!fp) return;  
  qh printoutnum++;
  fprintf (fp, "{appearance {-edge -normal normscale 0} {\n\
INST geom {define vsphere OFF\n\
18 32 48\n\
\n\
0 0 1\n\
1 0 0\n\
0 1 0\n\
-1 0 0\n\
0 -1 0\n\
0 0 -1\n\
0.707107 0 0.707107\n\
0 -0.707107 0.707107\n\
0.707107 -0.707107 0\n\
-0.707107 0 0.707107\n\
-0.707107 -0.707107 0\n\
0 0.707107 0.707107\n\
-0.707107 0.707107 0\n\
0.707107 0.707107 0\n\
0.707107 0 -0.707107\n\
0 0.707107 -0.707107\n\
-0.707107 0 -0.707107\n\
0 -0.707107 -0.707107\n\
\n\
3 0 6 11\n\
3 0 7 6	\n\
3 0 9 7	\n\
3 0 11 9\n\
3 1 6 8	\n\
3 1 8 14\n\
3 1 13 6\n\
3 1 14 13\n\
3 2 11 13\n\
3 2 12 11\n\
3 2 13 15\n\
3 2 15 12\n\
3 3 9 12\n\
3 3 10 9\n\
3 3 12 16\n\
3 3 16 10\n\
3 4 7 10\n\
3 4 8 7\n\
3 4 10 17\n\
3 4 17 8\n\
3 5 14 17\n\
3 5 15 14\n\
3 5 16 15\n\
3 5 17 16\n\
3 6 13 11\n\
3 7 8 6\n\
3 9 10 7\n\
3 11 12 9\n\
3 14 8 17\n\
3 15 13 14\n\
3 16 12 15\n\
3 17 10 16\n} transforms { TLIST\n");
  FOREACHvertex_(vertices) {
    fprintf(fp, "%8.4g 0 0 0 # v%d\n 0 %8.4g 0 0\n0 0 %8.4g 0\n",
      radius, vertex->id, radius, radius);
    qh_printpoint3 (fp, vertex->point);
    fprintf (fp, "1\n");
  }
  fprintf (fp, "}}}\n");
} /* printspheres */


/*----------------------------------------------
-printsummary-
                see qhull.c
*/

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printvdiagram">-</a>
  
  qh_printvdiagram( fp, format, facetlist, facets, printall )
    print voronoi diagram
      # of pairs of input sites
      #indices site1 site2 vertex1 ...
    
    sites indexed by input point id
      point 0 is the first input point
    vertices indexed by 'o' and 'p' order
      vertex 0 is the 'vertex-at-infinity'
      vertex 1 is the first Voronoi vertex

  see:
    qh_printvoronoi()
    qh_eachvoronoi_all()

  notes:
    if all facets are upperdelaunay, 
      prints upper hull (furthest-site Voronoi diagram)
*/
void qh_printvdiagram (FILE *fp, int format, facetT *facetlist, setT *facets, boolT printall) {
  setT *vertices;
  int totcount, numcenters;
  boolT islower;
  qh_RIDGE innerouter= qh_RIDGEall;
  printvridgeT printvridge= NULL;
	if (!fp) return;  

  if (format == qh_PRINTvertices) {
    innerouter= qh_RIDGEall;
    printvridge= qh_printvridge;
  }else if (format == qh_PRINTinner) {
    innerouter= qh_RIDGEinner;
    printvridge= qh_printvnorm;
  }else if (format == qh_PRINTouter) {
    innerouter= qh_RIDGEouter;
    printvridge= qh_printvnorm;
  }else {
    ivp_message( "qh_printvdiagram: unknown print format %d.\n", format);
    qh_errexit (qh_ERRinput, NULL, NULL);
  }
  vertices= qh_markvoronoi (facetlist, facets, printall, &islower, &numcenters);
  totcount= qh_printvdiagram2 (NULL, NULL, vertices, innerouter, False);
  fprintf (fp, "%d\n", totcount);
  totcount= qh_printvdiagram2 (fp, printvridge, vertices, innerouter, True /* inorder*/);
  qh_settempfree (&vertices);
#if 0  /* for testing qh_eachvoronoi_all */
  fprintf (fp, "\n");
  totcount= qh_eachvoronoi_all(fp, printvridge, qh UPPERdelaunay, innerouter, True /* inorder*/);
  fprintf (fp, "%d\n", totcount);
#endif
} /* printvdiagram */
  
/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printvdiagram2">-</a>
  
  qh_printvdiagram2( fp, printvridge, vertices, innerouter, inorder )
    visit all pairs of input sites (vertices) for selected Voronoi vertices
    vertices may include NULLs
  
  innerouter:
    qh_RIDGEall   print inner ridges (bounded) and outer ridges (unbounded)
    qh_RIDGEinner print only inner ridges
    qh_RIDGEouter print only outer ridges
  
  inorder:
    print 3-d Voronoi vertices in order
  
  assumes:
    qh_markvoronoi marked facet->visitid for Voronoi vertices
    all facet->seen= False
    all facet->seen2= True
  
  returns:
    total number of Voronoi ridges 
    if printvridge,
      calls printvridge( fp, vertex, vertexA, centers) for each ridge
      [see qh_eachvoronoi()]
  
  see:
    qh_eachvoronoi_all()
*/
int qh_printvdiagram2 (FILE *fp, printvridgeT printvridge, setT *vertices, qh_RIDGE innerouter, boolT inorder) {
  int totcount= 0;
  int vertex_i, vertex_n;
  vertexT *vertex;

  FORALLvertices 
    vertex->seen= False;
  FOREACHvertex_i_(vertices) {
    if (vertex)
      totcount += qh_eachvoronoi (fp, printvridge, vertex, !qh_ALL, innerouter, inorder);
  }
  return totcount;
} /* printvdiagram2 */
  
/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printvertex">-</a>
  
  qh_printvertex( fp, vertex )
    prints the information in a vertex
*/
void qh_printvertex(FILE *fp, vertexT *vertex) {
  pointT *point;
  int k;
  facetT *neighbor, **neighborp;
  realT r; /*bug fix*/
	if (!fp) return;  

  if (!vertex) {
    fprintf (fp, "  NULLvertex\n");
    return;
  }
  fprintf(fp, "- p%d (v%d):", qh_pointid(vertex->point), vertex->id);
  point= vertex->point;
  if (point) {
    for(k= qh hull_dim; k--; ) {
      r= *point++;
      fprintf(fp, " %5.2g", r);
    }
  }
  if (vertex->deleted)
    fprintf(fp, " deleted");
  if (vertex->delridge)
    fprintf (fp, " ridgedeleted");
  fprintf(fp, "\n");
  if (vertex->neighbors) {
    fprintf(fp, "  neighbors:");
    FOREACHneighbor_(vertex)
      fprintf(fp, " f%d", neighbor->id);
    fprintf(fp, "\n");
  }
} /* printvertex */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printvertexlist">-</a>
  
  qh_printvertexlist( fp, string, facetlist, facets, printall )
    prints vertices used by a facetlist or facet set
    tests qh_skipfacet() if !printall
*/
void qh_printvertexlist (FILE *fp, const char* string, facetT *facetlist, 
                         setT *facets, boolT printall) {
  vertexT *vertex, **vertexp;
  setT *vertices;
  
  vertices= qh_facetvertices (facetlist, facets, printall);
  fputs (string, fp);
  FOREACHvertex_(vertices)
    qh_printvertex(fp, vertex);
  qh_settempfree (&vertices);
} /* printvertexlist */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printvertices">-</a>
  
  qh_printvertices( fp, string, vertices )
    prints vertices in a set
*/
void qh_printvertices(FILE *fp, const char* string, setT *vertices) {
  vertexT *vertex, **vertexp;
	if (!fp) return;  
  
  fputs (string, fp);
  FOREACHvertex_(vertices) 
    fprintf (fp, " p%d (v%d)", qh_pointid(vertex->point), vertex->id);
  fprintf(fp, "\n");
} /* printvertices */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printvneighbors">-</a>
  
  qh_printvneighbors( fp, facetlist, facets, printall )
    print vertex neighbors of vertices in facetlist and facets ('FN')

  notes:
    qh_countfacets clears facet->visitid for non-printed facets

  design:
    collect facet count and related statistics
    if necessary, build neighbor sets for each vertex
    collect vertices in facetlist and facets
    build a point array for point->vertex and point->coplanar facet
    for each point
      list vertex neighbors or coplanar facet
*/
void qh_printvneighbors (FILE *fp, facetT* facetlist, setT *facets, boolT printall) {
  int numfacets, numsimplicial, numridges, totneighbors, numneighbors, numcoplanars;
  setT *vertices, *vertex_points, *coplanar_points;
  int numpoints= qh num_points + qh_setsize (qh other_points);
  vertexT *vertex, **vertexp;
  int vertex_i, vertex_n;
  facetT *facet, **facetp, *neighbor, **neighborp;
  pointT *point, **pointp;
	if (!fp) return;  

  qh_countfacets (facetlist, facets, printall, &numfacets, &numsimplicial, 
      &totneighbors, &numridges, &numcoplanars);  /* sets facet->visitid */
  fprintf (fp, "%d\n", numpoints);
  qh_vertexneighbors();
  vertices= qh_facetvertices (facetlist, facets, printall);
  vertex_points= qh_settemp (numpoints);
  coplanar_points= qh_settemp (numpoints);
  qh_setzero (vertex_points, 0, numpoints);
  qh_setzero (coplanar_points, 0, numpoints);
  FOREACHvertex_(vertices)
    qh_point_add (vertex_points, vertex->point, vertex);
  FORALLfacet_(facetlist) {
    FOREACHpoint_(facet->coplanarset)
      qh_point_add (coplanar_points, point, facet);
  }
  FOREACHfacet_(facets) {
    FOREACHpoint_(facet->coplanarset)
      qh_point_add (coplanar_points, point, facet);
  }
  FOREACHvertex_i_(vertex_points) {
    if (vertex) { 
      numneighbors= qh_setsize (vertex->neighbors);
      fprintf (fp, "%d", numneighbors);
      if (qh hull_dim == 3)
        qh_order_vertexneighbors (vertex);
      else if (qh hull_dim >= 4) //-V112
        qsort (SETaddr_(vertex->neighbors, facetT), numneighbors,
             sizeof (facetT *), qh_compare_facetvisit);
      FOREACHneighbor_(vertex) 
        fprintf (fp, " %d", 
		 neighbor->visitid ? neighbor->visitid - 1 : neighbor->id);
      fprintf (fp, "\n");
    }else if ((facet= SETelemt_(coplanar_points, vertex_i, facetT)))
      fprintf (fp, "1 %d\n",
                  facet->visitid ? facet->visitid - 1 : facet->id);
    else
      fprintf (fp, "0\n");
  }
  qh_settempfree (&coplanar_points);
  qh_settempfree (&vertex_points);
  qh_settempfree (&vertices);
} /* printvneighbors */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printvoronoi">-</a>
  
  qh_printvoronoi( fp, format, facetlist, facets, printall )
    print voronoi diagram in 'o' or 'G' format
    for 'o' format
      prints voronoi centers for each facet and for infinity
      for each vertex, lists ids of printed facets or infinity
      assumes facetlist and facets are disjoint
    for 'G' format
      prints an OFF object
      adds a 0 coordinate to center
      prints infinity but does not list in vertices

  see:
    qh_printvdiagram()

  notes:
    if 'o', 
      prints a line for each point except "at-infinity"
    if all facets are upperdelaunay, 
      reverses lower and upper hull
*/
void qh_printvoronoi (FILE *fp, int format, facetT *facetlist, setT *facets, boolT printall) {
  int k, numcenters, numvertices= 0, numneighbors, numinf, vid=1, vertex_i, vertex_n;
  facetT *facet, **facetp, *neighbor, **neighborp;
  setT *vertices;
  vertexT *vertex;
  boolT islower;
	if (!fp) return;  
  unsigned int numfacets= (unsigned int) qh num_facets;

  vertices= qh_markvoronoi (facetlist, facets, printall, &islower, &numcenters);
  FOREACHvertex_i_(vertices) {
    if (vertex) {
      numvertices++;
      numneighbors = numinf = 0;
      FOREACHneighbor_(vertex) {
        if (neighbor->visitid == 0)
	  numinf= 1;
        else if (neighbor->visitid < numfacets)
          numneighbors++;
      }
      if (numinf && !numneighbors) {
	SETelem_(vertices, vertex_i)= NULL;
	numvertices--;
      }
    }
  }
  if (format == qh_PRINTgeom) 
    fprintf (fp, "{appearance {+edge -face} OFF %d %d 1 # Voronoi centers and cells\n", 
                numcenters, numvertices);
  else
    fprintf (fp, "%d\n%d %d 1\n", qh hull_dim-1, numcenters, qh_setsize(vertices));
  if (format == qh_PRINTgeom) {
    for (k= qh hull_dim-1; k--; )
      fprintf (fp, qh_REAL_1, 0.0);
    fprintf (fp, " 0 # infinity not used\n");
  }else {
    for (k= qh hull_dim-1; k--; )
      fprintf (fp, qh_REAL_1, qh_INFINITE);
    fprintf (fp, "\n");
  }
  FORALLfacet_(facetlist) {
    if (facet->visitid && facet->visitid < numfacets) {
      if (format == qh_PRINTgeom)
        fprintf (fp, "# %d f%d\n", vid++, facet->id);
      qh_printcenter (fp, format, NULL, facet);
    }
  }
  FOREACHfacet_(facets) {
    if (facet->visitid && facet->visitid < numfacets) {
      if (format == qh_PRINTgeom)
        fprintf (fp, "# %d f%d\n", vid++, facet->id);
      qh_printcenter (fp, format, NULL, facet);
    }
  }
  FOREACHvertex_i_(vertices) {
    numneighbors= 0;
    numinf=0;
    if (vertex) {
      if (qh hull_dim == 3)
        qh_order_vertexneighbors(vertex);
      else if (qh hull_dim >= 4) //-V112
        qsort (SETaddr_(vertex->neighbors, vertexT), 
	     qh_setsize (vertex->neighbors),
	     sizeof (facetT *), qh_compare_facetvisit);
      FOREACHneighbor_(vertex) {
        if (neighbor->visitid == 0)
	  numinf= 1;
	else if (neighbor->visitid < numfacets)
          numneighbors++;
      }
    }
    if (format == qh_PRINTgeom) {
      if (vertex) {
	fprintf (fp, "%d", numneighbors);
	if (vertex) {
	  FOREACHneighbor_(vertex) {
	    if (neighbor->visitid && neighbor->visitid < numfacets)
	      fprintf (fp, " %d", neighbor->visitid);
	  }
	}
	fprintf (fp, " # p%d (v%d)\n", vertex_i, vertex->id);
      }else
	fprintf (fp, " # p%d is coplanar or isolated\n", vertex_i);
    }else {
      if (numinf)
	numneighbors++;
      fprintf (fp, "%d", numneighbors);
      if (vertex) {
        FOREACHneighbor_(vertex) {
  	  if (neighbor->visitid == 0) {
  	    if (numinf) {
  	      numinf= 0;
	      fprintf (fp, " %d", neighbor->visitid);
	    }
	  }else if (neighbor->visitid < numfacets)
	    fprintf (fp, " %d", neighbor->visitid);
	}
      }
      fprintf (fp, "\n");
    }
  }
  if (format == qh_PRINTgeom)
    fprintf (fp, "}\n");
  qh_settempfree (&vertices);
} /* printvoronoi */
  
/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printvnorm">-</a>
  
  qh_printvnorm( fp, vertex, vertexA, centers, unbounded )
    print one separating plane of the Voronoi diagram for a pair of input sites
    unbounded==True if centers includes vertex-at-infinity
  
  assumes:
    qh_ASvoronoi and qh_vertexneighbors() already set
    
  see:
    qh_printvdiagram()
    qh_eachvoronoi()
*/
void qh_printvnorm (FILE *fp, vertexT *vertex, vertexT *vertexA, setT *centers, boolT /*unbounded*/) {
  pointT *normal;
  realT offset;
  int k;
	if (!fp) return;  
  
  normal= qh_detvnorm (vertex, vertexA, centers, &offset);
  fprintf (fp, "%d %d %d ", 
      2+qh hull_dim, qh_pointid (vertex->point), qh_pointid (vertexA->point));
  for (k= 0; k< qh hull_dim-1; k++)
    fprintf (fp, qh_REAL_1, normal[k]);
  fprintf (fp, qh_REAL_1, offset);
  fprintf (fp, "\n");
} /* printvnorm */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="printvridge">-</a>
  
  qh_printvridge( fp, vertex, vertexA, centers, unbounded )
    print one ridge of the Voronoi diagram for a pair of input sites
    unbounded==True if centers includes vertex-at-infinity
  
  see:
    qh_printvdiagram()
  
  notes:
    the user may use a different function
*/
void qh_printvridge (FILE *fp, vertexT *vertex, vertexT *vertexA, setT *centers, boolT /*unbounded*/) {
  facetT *facet, **facetp;
	if (!fp) return;  

  fprintf (fp, "%d %d %d", qh_setsize (centers)+2, 
       qh_pointid (vertex->point), qh_pointid (vertexA->point));
  FOREACHfacet_(centers) 
    fprintf (fp, " %d", facet->visitid);
  fprintf (fp, "\n");
} /* printvridge */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="projectdim3">-</a>
  
  qh_projectdim3( source, destination )
    project 2-d 3-d or 4-d point to a 3-d point
    uses qh.DROPdim and qh.hull_dim
    source and destination may be the same
    
  notes:
    allocate 4 elements to destination just in case
*/
void qh_projectdim3 (pointT *source, pointT *destination) {
  int i,k;

  for (k= 0, i=0; k < qh hull_dim; k++) {
    if (qh hull_dim == 4) { //-V112
      if (k != qh DROPdim)
        destination[i++]= source[k];
    }else if (k == qh DROPdim)
      destination[i++]= 0;
    else
      destination[i++]= source[k];
  }
  while (i < 3)
    destination[i++]= 0.0;
} /* projectdim3 */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="readfeasible">-</a>
  
  qh_readfeasible( dim, remainder )
    read feasible point from remainder string and qh.fin

  returns:
    number of lines read from qh.fin
    sets qh.FEASIBLEpoint with malloc'd coordinates

  notes:
    checks for qh.HALFspace
    assumes dim > 1

  see:
    qh_setfeasible
*/
int qh_readfeasible (int dim, char *remainder) {
  boolT isfirst= True;
  int linecount= 0, tokcount= 0;
  char *s, *t, firstline[qh_MAXfirst+1];
  coordT *coords, value;

  if (!qh HALFspace) {
    ivp_message( "qhull input error: feasible point (dim 1 coords) is only valid for halfspace intersection\n");
    qh_errexit (qh_ERRinput, NULL, NULL);
  }  
  if (qh feasible_string)
    ivp_message( "qhull input warning: feasible point (dim 1 coords) overrides 'Hn,n,n' feasible point for halfspace intersection\n");
  if (!(qh feasible_point= (coordT*)malloc (dim* sizeof(coordT)))) {
    ivp_message( "qhull error: insufficient memory for feasible point\n");
    qh_errexit(qh_ERRmem, NULL, NULL);
  }
  coords= qh feasible_point;
  while ((s= (isfirst ?  remainder : fgets(firstline, qh_MAXfirst, qh fin)))) {
    if (isfirst)
      isfirst= False;
    else
      linecount++;
    while (*s) {
      while (isspace(*s))
        s++;
      value= qh_strtod (s, &t);
      if (s == t)
        break;
      s= t;
      *(coords++)= value;
      if (++tokcount == dim) {
        while (isspace (*s))
          s++;
        qh_strtod (s, &t);
        if (s != t) {
          ivp_message( "qhull input error: coordinates for feasible point do not finish out the line: %s\n",
               s);
          qh_errexit (qh_ERRinput, NULL, NULL);
        }
        return linecount;
      }
    }
  }
  ivp_message( "qhull input error: only %d coordinates.  Could not read %d-d feasible point.\n",
           tokcount, dim);
  qh_errexit (qh_ERRinput, NULL, NULL);
  return 0;
} /* readfeasible */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="readpoints">-</a>
  
  qh_readpoints( numpoints, dimension, ismalloc )
    read points from qh.fin into qh.first_point, qh.num_points
    qh.fin is lines of coordinates, one per vertex, first line number of points
    if 'rbox D4',
      gives message
    if qh.ATinfinity,
      adds point-at-infinity for Delaunay triangulations

  returns:
    number of points, array of point coordinates, dimension, ismalloc True
    if qh.DELAUNAY & !qh.PROJECTinput, projects points to paraboloid
        and clears qh.PROJECTdelaunay
    if qh.HALFspace, reads optional feasible point, reads halfspaces,
        converts to dual.

  for feasible point in "cdd format" in 3-d:
    3 1
    coordinates
    comments
    begin
    n 4 real/integer
    ...
    end

  notes:
    dimension will change in qh_initqhull_globals if qh.PROJECTinput
    uses malloc() since qh_mem not initialized
    FIXUP: this routine needs rewriting
*/
coordT *qh_readpoints(int *numpoints, int *dimension, boolT *ismalloc) {
  coordT *points, *coords, *infinity= NULL;
  realT paraboloid, maxboloid= -REALmax, value;
  realT *coordp= NULL, *offsetp= NULL, *normalp= NULL;
  char *s=0, *t, firstline[qh_MAXfirst+1];
  int diminput=0, numinput=0, dimfeasible= 0, newnum, k, tempi;
  int firsttext=0, firstshort=0, firstlong=0, firstpoint=0;
  int tokcount= 0, linecount=0, maxcount, coordcount=0;
  boolT islong, isfirst= True, wasbegin= False;
  boolT isdelaunay= qh DELAUNAY && !qh PROJECTinput;

  if (qh CDDinput) {
    while ((s= fgets(firstline, qh_MAXfirst, qh fin))) {
      linecount++;
      if (qh HALFspace && linecount == 1 && isdigit(*s)) {
	dimfeasible= qh_strtol (s, &s);	
	while (isspace(*s))
          s++;
        if (qh_strtol (s, &s) == 1)
          linecount += qh_readfeasible (dimfeasible, s);
        else
          dimfeasible= 0;
      }else if (!memcmp (firstline, "begin", 5) || !memcmp (firstline, "BEGIN", 5))
        break;
      else if (!*qh rbox_command)
	strncat(qh rbox_command, s, sizeof (qh rbox_command)-1);
    }
    if (!s) {
      ivp_message( "qhull input error: missing \"begin\" for cdd-formated input\n");
      qh_errexit (qh_ERRinput, NULL, NULL);
    }
  }
  while(!numinput && (s= fgets(firstline, qh_MAXfirst, qh fin))) {
    linecount++;
    if (!memcmp (s, "begin", 5) || !memcmp (s, "BEGIN", 5))
      wasbegin= True;
    while (*s) {
      while (isspace(*s))
        s++;
      if (!*s)
        break;
      if (!isdigit(*s)) {
        if (!*qh rbox_command) {
          strncat(qh rbox_command, s, sizeof (qh rbox_command)-1);
	  firsttext= linecount;
        }
        break;
      }
      if (!diminput) 
        diminput= qh_strtol (s, &s);
      else {
        numinput= qh_strtol (s, &s);
        if (numinput == 1 && diminput >= 2 && qh HALFspace && !qh CDDinput) {
          linecount += qh_readfeasible (diminput, s); /* checks if ok */
          dimfeasible= diminput;
          diminput= numinput= 0;
        }else 
          break;
      }
    }
  }
  if (!s) {
    ivp_message( "qhull input error: short input file.  Did not find dimension and number of points\n");
    qh_errexit(qh_ERRinput, NULL, NULL);
  }
  if (diminput > numinput) {
    tempi= diminput;	/* exchange dim and n, e.g., for cdd input format */
    diminput= numinput;
    numinput= tempi;
  }
  if (diminput < 2) {
    ivp_message("qhull input error: dimension %d (first number) should be at least 2\n",
	    diminput);
    qh_errexit(qh_ERRinput, NULL, NULL);
  }
  if (isdelaunay) {
    qh PROJECTdelaunay= False;
    if (qh CDDinput)
      *dimension= diminput;
    else
      *dimension= diminput+1;
    *numpoints= numinput;
    if (qh ATinfinity)
      (*numpoints)++;
  }else if (qh HALFspace) {
    *dimension= diminput - 1;
    *numpoints= numinput;
    if (diminput < 3) {
      ivp_message("qhull input error: dimension %d (first number, includes offset) should be at least 3 for halfspaces\n",
  	    diminput);
      qh_errexit(qh_ERRinput, NULL, NULL);
    }
    if (dimfeasible) {
      if (dimfeasible != *dimension) {
        ivp_message("qhull input error: dimension %d of feasible point is not one less than dimension %d for halfspaces\n",
          dimfeasible, diminput);
        qh_errexit(qh_ERRinput, NULL, NULL);
      }
    }else 
      qh_setfeasible (*dimension);
  }else {
    if (qh CDDinput) 
      *dimension= diminput-1;
    else
      *dimension= diminput;
    *numpoints= numinput;
  }
  qh normal_size= *dimension * sizeof(coordT); /* for tracing with qh_printpoint */
  if (qh HALFspace) {
    qh half_space= coordp= (coordT*) p_malloc (qh normal_size + sizeof(coordT));
    if (qh CDDinput) {
      offsetp= qh half_space;
      normalp= offsetp + 1;
    }else {
      normalp= qh half_space;
      offsetp= normalp + *dimension;
    }
  } 
  qh maxline= diminput * (qh_REALdigits + 5);
  maximize_(qh maxline, 500);
  qh line= (char*)p_malloc ((qh maxline+1) * sizeof (char));
  *ismalloc= True;  /* use malloc since memory not setup */
  coords= points= qh temp_malloc= 
        (coordT*)p_malloc((*numpoints)*(*dimension)*sizeof(coordT));
  if (!coords || !qh line || (qh HALFspace && !qh half_space)) {
    ivp_message( "qhull error: insufficient memory to read %d points\n",
	    numinput);
    qh_errexit(qh_ERRmem, NULL, NULL);
  }
  if (isdelaunay && qh ATinfinity) {
    infinity= points + numinput * (*dimension);
    for (k= (*dimension) - 1; k--; )
      infinity[k]= 0.0;
  }
  maxcount= numinput * diminput;
  paraboloid= 0.0;
  while ((s= (isfirst ?  s : fgets(qh line, qh maxline, qh fin)))) {
    if (!isfirst) {
      linecount++;
      if (*s == 'e' || *s == 'E') {
	if (!memcmp (s, "end", 3) || !memcmp (s, "END", 3)) {
	  if (qh CDDinput )
	    break;
	  else if (wasbegin) 
	    ivp_message( "qhull input warning: the input appears to be in cdd format.  If so, use 'Fd'\n");
	}
      }
    }
    islong= False;
    while (*s) {
      while (isspace(*s))
        s++;
      value= qh_strtod (s, &t);
      if (s == t) {
        if (!*qh rbox_command)
 	 strncat(qh rbox_command, s, sizeof (qh rbox_command)-1);
        if (*s && !firsttext) 
          firsttext= linecount;
        if (!islong && !firstshort && coordcount)
          firstshort= linecount;
        break;
      }
      if (!firstpoint)
	firstpoint= linecount;
      s= t;
      if (++tokcount > maxcount)
        continue;
      if (qh HALFspace) {
	if (qh CDDinput && !coordcount) 
	  *(coordp++)= -value; /* offset */
	else
	  *(coordp++)= value;
      }else {
        *(coords++)= value;
        if (qh CDDinput && !coordcount) {
          if (value != 1.0) {
            ivp_message( "qhull input error: for cdd format, point at line %d does not start with '1'\n",
                   linecount);
            qh_errexit (qh_ERRinput, NULL, NULL);
          }
          coords--;
        }else if (isdelaunay) {
	  paraboloid += value * value;
	  if (qh ATinfinity) {
	    if (qh CDDinput)
	      infinity[coordcount-1] += value;
	    else
	      infinity[coordcount] += value;
	  }
	}
      }
      if (++coordcount == diminput) {
        coordcount= 0;
        if (isdelaunay) {
          *(coords++)= paraboloid;
          maximize_(maxboloid, paraboloid);
          paraboloid= 0.0;
        }else if (qh HALFspace) {
          if (!qh_sethalfspace (*dimension, coords, &coords, normalp, offsetp, qh feasible_point)) {
	    ivp_message( "The halfspace was on line %d\n", linecount);
	    if (wasbegin)
	      ivp_message( "The input appears to be in cdd format.  If so, you should use option 'Fd'\n");
	    qh_errexit (qh_ERRinput, NULL, NULL);
	  }
          coordp= qh half_space;
        }          
        while (isspace(*s))
          s++;
        if (*s) {
          islong= True;
          if (!firstlong)
            firstlong= linecount;
	}
      }
    }
    if (!islong && !firstshort && coordcount)
      firstshort= linecount;
    if (!isfirst && s - qh line >= qh maxline) {
      ivp_message( "qhull input error: line %d contained more than %d characters\n", 
	      linecount, (int) (s - qh line));
      qh_errexit(qh_ERRinput, NULL, NULL);
    }
    isfirst= False;
  }
  if (tokcount != maxcount) {
    newnum= fmin_(numinput, tokcount/diminput);
    ivp_message("\
qhull warning: instead of %d %d-dimensional points, input contains\n\
%d points and %d extra coordinates.  Line %d is the first\npoint",
       numinput, diminput, tokcount/diminput, tokcount % diminput, firstpoint);
    if (firsttext)
      ivp_message( ", line %d is the first comment", firsttext);
    if (firstshort)
      ivp_message( ", line %d is the first short\nline", firstshort);
    if (firstlong)
      ivp_message( ", line %d is the first long line", firstlong);
    ivp_message( ".  Continue with %d points.\n", newnum);
    numinput= newnum;
    if (isdelaunay && qh ATinfinity) {
      for (k= tokcount % diminput; k--; )
	infinity[k] -= *(--coords);
      *numpoints= newnum+1;
    }else {
      coords -= tokcount % diminput;
      *numpoints= newnum;
    }
  }
  if (isdelaunay && qh ATinfinity) {
    for (k= (*dimension) -1; k--; )
      infinity[k] /= numinput;
    if (coords == infinity)
      coords += (*dimension) -1;
    else {
      for (k= 0; k < (*dimension) -1; k++)
	*(coords++)= infinity[k];
    }
    *(coords++)= maxboloid * 1.1;
  }
  if (qh rbox_command[0]) {
    qh rbox_command[strlen(qh rbox_command)-1]= '\0';
    if (!strcmp (qh rbox_command, "./rbox D4")) 
      ivp_message( "\n\
This is the qhull test case.  If any errors or core dumps occur,\n\
recompile qhull with 'make new'.  If errors still occur, there is\n\
an incompatibility.  You should try a different compiler.  You can also\n\
change the choices in user.h.  If you discover the source of the problem,\n\
please send mail to qhull_bug@geom.umn.edu.\n\
\n\
Type 'qhull' for a short list of options.\n");
  }
  P_FREE (qh line);
  qh line= NULL;
  if (qh half_space) {
    P_FREE (qh half_space);
    qh half_space= NULL;
  }
  qh temp_malloc= NULL;
  trace1((qh ferr,"qh_readpoints: read in %d %d-dimensional points\n",
	  numinput, diminput));
  return(points);
} /* readpoints */


/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="setfeasible">-</a>
  
  qh_setfeasible( dim )
    set qh.FEASIBLEpoint from qh.feasible_string in "n,n,n" or "n n n" format

  notes:
    "n,n,n" already checked by qh_initflags()
    see qh_readfeasible()
*/
void qh_setfeasible (int dim) {
  int tokcount= 0;
  char *s;
  coordT *coords, value;

  if (!(s= qh feasible_string)) {
    ivp_message( "\
qhull input error: halfspace intersection needs a feasible point.\n\
Either prepend the input with 1 point or use 'Hn,n,n'.  See manual.\n");
    qh_errexit (qh_ERRinput, NULL, NULL);
  }
  if (!(qh feasible_point= (pointT*)p_malloc (dim* sizeof(coordT)))) {
    ivp_message( "qhull error: insufficient memory for 'Hn,n,n'\n");
    qh_errexit(qh_ERRmem, NULL, NULL);
  }
  coords= qh feasible_point;
  while (*s) {
    value= qh_strtod (s, &s);
    if (++tokcount > dim) {
      ivp_message( "qhull input warning: more coordinates for 'H%s' than dimension %d\n",
          qh feasible_string, dim);
      break;
    }
    *(coords++)= value;
    if (*s)
      s++;
  }
  while (++tokcount <= dim)    
    *(coords++)= 0.0;
} /* setfeasible */

/*-<a                             href="qh-c.htm#io"
  >-------------------------------</a><a name="skipfacet">-</a>
  
  qh_skipfacet( facet )
    returns 'True' if this facet is not to be printed 

  notes:
    based on the user provided slice thresholds and 'good' specifications
*/
boolT qh_skipfacet(facetT *facet) {
  facetT *neighbor, **neighborp;

  if (qh PRINTneighbors) {
    if (facet->good)
      return !qh PRINTgood;
    FOREACHneighbor_(facet) {
      if (neighbor->good)
	return False;
    }
    return True;
  }else if (qh PRINTgood)
    return !facet->good;
  else if (!facet->normal)
    return True;
  return (!qh_inthresholds (facet->normal, NULL));
} /* skipfacet */

